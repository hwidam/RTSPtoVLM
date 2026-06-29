#pragma once
#define WM_VLM_RESULT  (WM_APP + 1)

#include <opencv2/opencv.hpp>
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

#define LLAMA_SHARED
#include <llama.h>
#include <mtmd/mtmd.h>

class VLMInference {
public:
    struct Request {
        cv::Mat     frame;
        std::string prompt;
        uint64_t    timestamp = 0; // ms epoch from ShmFrameHeader, carried through to Result
    };

    struct Result {
        cv::Mat     image;         // original frame (unchanged)
        std::string text;          // generated response
        uint64_t    timestamp = 0; // ms epoch of the source frame
    };

    VLMInference() = default;
    ~VLMInference();

    VLMInference(const VLMInference&) = delete;
    VLMInference& operator=(const VLMInference&) = delete;

    // Loads models and starts the worker thread.
    // modelPath  : GGUF language model  (e.g. llava-v1.6-mistral-7b.Q4_K_M.gguf)
    // mmprojPath : vision encoder GGUF  (e.g. llava-v1.6-mistral-7b-mmproj-f16.gguf)
    // nGpuLayers : layers to offload to GPU (0 = CPU only, -1 = all)
    bool Init(const std::string& modelPath,
              const std::string& mmprojPath,
              int  nCtx         = 8192,
              int  nGpuLayers   = -1,
              int  nThreads     = 4,
              int  maxNewTokens = 512);

    bool IsReady()   const { return m_ready; }
    bool IsBusy()    const { return m_busy; }

    void SetNotifyWnd(HWND hwnd) { m_hNotifyWnd = hwnd; }

    // Enqueue a frame+prompt for inference.
    // If a request is already waiting, it is replaced with the new one (keep only latest).
    void Push(const cv::Mat& frame, const std::string& prompt, uint64_t timestamp = 0);

    // Non-blocking. Returns true and moves the result out if a new one is available.
    bool TryGetResult(Result& out);

private:
    Result      Infer(const cv::Mat& frame, const std::string& prompt);
    bool        SubmitChunks(mtmd_input_chunks* chunks, int32_t& nPast);
    std::string Generate(int32_t nPast);
    void        WorkerLoop();

    // llama / mtmd handles
    llama_model*   m_model        = nullptr;
    llama_context* m_ctx          = nullptr;
    mtmd_context*  m_mtmdCtx      = nullptr;
    bool           m_ready        = false;
    int            m_maxNewTokens = 512;

    // worker thread
    std::thread             m_worker;
    std::atomic<bool>       m_stopping{ false };
    std::atomic<bool>       m_busy    { false };

    // input queue (max depth 1 — always keep the latest frame)
    std::queue<Request>     m_inputQueue;
    std::mutex              m_queueMutex;
    std::condition_variable m_queueCv;

    // latest result
    Result                  m_latestResult;
    std::mutex              m_resultMutex;
    std::atomic<bool>       m_hasNewResult{ false };

    HWND                    m_hNotifyWnd{ nullptr };
};
