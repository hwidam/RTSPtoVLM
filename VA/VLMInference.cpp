#include "pch.h"
#include "VLMInference.h"
#include <cstring>
#include "../common/Logger.h"

static void LlamaLogSink(ggml_log_level level, const char* text, void* /*ud*/)
{
    if (!text || text[0] == '\0' || text[0] == '\n') return;
    switch (level) {
    case GGML_LOG_LEVEL_ERROR: Logger::Error("[llama] {}", text); break;
    case GGML_LOG_LEVEL_WARN:  Logger::Warn ("[llama] {}", text); break;
    default: break;
    }
}

// ─── lifetime ────────────────────────────────────────────────────────────────

VLMInference::~VLMInference()
{
    m_stopping = true;
    m_queueCv.notify_all();
    if (m_worker.joinable())
        m_worker.join();

    if (m_mtmdCtx) { mtmd_free(m_mtmdCtx);     m_mtmdCtx = nullptr; }
    if (m_ctx)     { llama_free(m_ctx);         m_ctx     = nullptr; }
    if (m_model)   { llama_model_free(m_model); m_model   = nullptr; }
    llama_backend_free();
}

// ─── Init ────────────────────────────────────────────────────────────────────

bool VLMInference::Init(const std::string& modelPath,
                        const std::string& mmprojPath,
                        int nCtx, int nGpuLayers, int nThreads, int maxNewTokens)
{
    m_maxNewTokens = maxNewTokens;

    llama_backend_init();
    llama_log_set(LlamaLogSink, nullptr);

    auto mparams         = llama_model_default_params();
    mparams.n_gpu_layers = nGpuLayers;

    m_model = llama_model_load_from_file(modelPath.c_str(), mparams);
    if (!m_model) {
        Logger::Error("VLM: failed to load model: {}", modelPath);
        return false;
    }

    auto cparams            = llama_context_default_params();
    cparams.n_ctx           = (uint32_t)nCtx;
    cparams.n_threads       = nThreads;
    cparams.n_threads_batch = nThreads;

    m_ctx = llama_init_from_model(m_model, cparams);
    if (!m_ctx) {
        Logger::Error("VLM: failed to create llama context");
        return false;
    }

    auto mp          = mtmd_context_params_default();
    mp.use_gpu       = (nGpuLayers != 0);
    mp.n_threads     = nThreads;
    mp.print_timings = false;
    mp.warmup        = false;

    m_mtmdCtx = mtmd_init_from_file(mmprojPath.c_str(), m_model, mp);
    if (!m_mtmdCtx) {
        Logger::Error("VLM: failed to load mmproj: {}", mmprojPath);
        return false;
    }

    m_ready  = true;
    m_worker = std::thread([this]() { WorkerLoop(); });
    Logger::Info("VLM: ready | ctx={} gpu_layers={} threads={}", nCtx, nGpuLayers, nThreads);
    return true;
}

// ─── Push / TryGetResult ─────────────────────────────────────────────────────

void VLMInference::Push(const cv::Mat& frame, const std::string& prompt, uint64_t timestamp)
{
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        // Discard any pending request — keep only the most recent frame
        while (!m_inputQueue.empty())
            m_inputQueue.pop();
        m_inputQueue.push({ frame.clone(), prompt, timestamp });
    }
    m_queueCv.notify_one();
}

bool VLMInference::TryGetResult(Result& out)
{
    if (!m_hasNewResult)
        return false;
    std::lock_guard<std::mutex> lock(m_resultMutex);
    out            = std::move(m_latestResult);
    m_hasNewResult = false;
    return true;
}

// ─── WorkerLoop ──────────────────────────────────────────────────────────────

void VLMInference::WorkerLoop()
{
    while (!m_stopping) {
        Request req;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCv.wait(lock, [this] {
                return !m_inputQueue.empty() || m_stopping;
            });
            if (m_stopping) break;
            req = std::move(m_inputQueue.front());
            m_inputQueue.pop();
        }

        m_busy = true;
        Result r = Infer(req.frame, req.prompt);
        r.timestamp = req.timestamp;
        m_busy   = false;

        {
            std::lock_guard<std::mutex> lock(m_resultMutex);
            m_latestResult = std::move(r);
            m_hasNewResult = true;
        }

        if (m_hNotifyWnd)
            ::PostMessage(m_hNotifyWnd, WM_VLM_RESULT, 0, 0);
    }
}

// ─── Infer ───────────────────────────────────────────────────────────────────

VLMInference::Result VLMInference::Infer(const cv::Mat& frame, const std::string& prompt)
{
    Result result;
    result.image = frame.clone();

    if (!m_ready || frame.empty()) {
        Logger::Error("VLM: Infer called before Init or with empty frame");
        return result;
    }

    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);
    if (!rgb.isContinuous())
        rgb = rgb.clone();

    mtmd_bitmap* bmp = mtmd_bitmap_init(
        (uint32_t)rgb.cols, (uint32_t)rgb.rows, rgb.data);
    if (!bmp) {
        Logger::Error("VLM: mtmd_bitmap_init failed");
        return result;
    }

    std::string fullPrompt = prompt + "\n" + mtmd_default_marker();

    mtmd_input_text textInput{};
    textInput.text          = fullPrompt.c_str();
    textInput.add_special   = true;
    textInput.parse_special = true;

    auto*              chunks  = mtmd_input_chunks_init();
    const mtmd_bitmap* bmps[]  = { bmp };

    int32_t ret = mtmd_tokenize(m_mtmdCtx, chunks, &textInput, bmps, 1);
    mtmd_bitmap_free(bmp);

    if (ret != 0) {
        Logger::Error("VLM: mtmd_tokenize failed ({})", ret);
        mtmd_input_chunks_free(chunks);
        return result;
    }

    llama_memory_clear(llama_get_memory(m_ctx), false);

    int32_t nPast = 0;
    if (!SubmitChunks(chunks, nPast)) {
        mtmd_input_chunks_free(chunks);
        return result;
    }
    mtmd_input_chunks_free(chunks);

    result.text = Generate(nPast);
    Logger::Info("VLM: generated {}", result.text);
    return result;
}

// ─── SubmitChunks ────────────────────────────────────────────────────────────

bool VLMInference::SubmitChunks(mtmd_input_chunks* chunks, int32_t& nPast)
{
    const int32_t nEmbd   = llama_model_n_embd_inp(m_model);
    const size_t  nChunks = mtmd_input_chunks_size(chunks);

    // Find the last chunk that actually has tokens — only ITS last token needs logits.
    // (If the prompt ends with the image marker there may be no trailing text chunk,
    //  so the image chunk itself must expose its last-position logit for sampling.)
    size_t lastIdx = nChunks; // sentinel = none found yet
    for (size_t i = nChunks; i-- > 0; ) {
        const mtmd_input_chunk* c = mtmd_input_chunks_get(chunks, i);
        auto t = mtmd_input_chunk_get_type(c);
        if (t == MTMD_INPUT_CHUNK_TYPE_TEXT) {
            size_t n = 0; mtmd_input_chunk_get_tokens_text(c, &n);
            if (n > 0) { lastIdx = i; break; }
        } else if (t == MTMD_INPUT_CHUNK_TYPE_IMAGE) {
            if (mtmd_input_chunk_get_n_tokens(c) > 0) { lastIdx = i; break; }
        }
    }

    for (size_t i = 0; i < nChunks; ++i) {
        const mtmd_input_chunk* chunk = mtmd_input_chunks_get(chunks, i);
        auto type = mtmd_input_chunk_get_type(chunk);
        bool isLast = (i == lastIdx);

        if (type == MTMD_INPUT_CHUNK_TYPE_TEXT) {
            size_t nTok = 0;
            const llama_token* toks = mtmd_input_chunk_get_tokens_text(chunk, &nTok);
            if (nTok == 0) continue;

            llama_batch batch = llama_batch_init((int32_t)nTok, 0, 1);
            batch.n_tokens = (int32_t)nTok;
            for (int32_t j = 0; j < (int32_t)nTok; ++j) {
                batch.token[j]     = toks[j];
                batch.pos[j]       = nPast + j;
                batch.n_seq_id[j]  = 1;
                batch.seq_id[j][0] = 0;
                batch.logits[j]    = (isLast && j == (int32_t)nTok - 1) ? 1 : 0;
            }

            int rc = llama_decode(m_ctx, batch);
            llama_batch_free(batch);
            if (rc != 0) {
                Logger::Error("VLM: llama_decode text chunk {} failed ({})", i, rc);
                return false;
            }
            nPast += (int32_t)nTok;

        } else if (type == MTMD_INPUT_CHUNK_TYPE_IMAGE) {
            bool nonCausal = mtmd_decode_use_non_causal(m_mtmdCtx, chunk);
            if (nonCausal)
                llama_set_causal_attn(m_ctx, false);

            if (mtmd_encode_chunk(m_mtmdCtx, chunk) != 0) {
                Logger::Error("VLM: mtmd_encode_chunk {} failed", i);
                if (nonCausal) llama_set_causal_attn(m_ctx, true);
                return false;
            }

            float*  embd = mtmd_get_output_embd(m_mtmdCtx);
            int32_t nTok = (int32_t)mtmd_input_chunk_get_n_tokens(chunk);

            llama_batch batch = llama_batch_init(nTok, nEmbd, 1);
            batch.n_tokens = nTok;
            std::memcpy(batch.embd, embd, (size_t)nTok * nEmbd * sizeof(float));
            for (int32_t j = 0; j < nTok; ++j) {
                batch.pos[j]       = nPast + j;
                batch.n_seq_id[j]  = 1;
                batch.seq_id[j][0] = 0;
                // If this is the last chunk, expose the last position's logit for sampling.
                batch.logits[j]    = (isLast && j == nTok - 1) ? 1 : 0;
            }

            int rc = llama_decode(m_ctx, batch);
            llama_batch_free(batch);
            if (nonCausal) llama_set_causal_attn(m_ctx, true);

            if (rc != 0) {
                Logger::Error("VLM: llama_decode image chunk {} failed ({})", i, rc);
                return false;
            }
            nPast += nTok;
        }
    }
    return true;
}

// ─── Generate ────────────────────────────────────────────────────────────────

std::string VLMInference::Generate(int32_t nPast)
{
    const llama_vocab* vocab = llama_model_get_vocab(m_model);
    const llama_token  eos   = llama_vocab_eos(vocab);

    auto  scp     = llama_sampler_chain_default_params();
    auto* sampler = llama_sampler_chain_init(scp);
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.95f, 1));
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.7f));
    llama_sampler_chain_add(sampler, llama_sampler_init_greedy());

    std::string output;
    llama_token token = llama_sampler_sample(sampler, m_ctx, -1);

    for (int32_t n = 0; token != eos && n < m_maxNewTokens; ++n) {
        char    piece[256] = {};
        int32_t len = llama_token_to_piece(vocab, token, piece, (int32_t)sizeof(piece) - 1, 0, true);
        if (len > 0)
            output.append(piece, (size_t)len);

        llama_sampler_accept(sampler, token);

        llama_batch batch   = llama_batch_init(1, 0, 1);
        batch.n_tokens      = 1;
        batch.token[0]      = token;
        batch.pos[0]        = nPast;
        batch.n_seq_id[0]   = 1;
        batch.seq_id[0][0]  = 0;
        batch.logits[0]     = 1;

        int rc = llama_decode(m_ctx, batch);
        llama_batch_free(batch);

        if (rc != 0) {
            Logger::Error("VLM: generate decode failed at token {} ({})", n, rc);
            break;
        }

        ++nPast;
        token = llama_sampler_sample(sampler, m_ctx, -1);
    }

    llama_sampler_free(sampler);
    return output;
}
