#pragma once
// Minimal stub — only the types that llama.h / mtmd.h require when compiling against pre-built DLLs.
#include "ggml.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ggml_backend_buffer_type * ggml_backend_buffer_type_t;
typedef struct ggml_backend_dev         * ggml_backend_dev_t;

// Called by the backend scheduler; return true to continue evaluation.
typedef bool (*ggml_backend_sched_eval_callback)(struct ggml_tensor * t, bool ask, void * user_data);

#ifdef __cplusplus
}
#endif
