#pragma once
// Stub — optimizer training API not used for inference.
#include "ggml.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle types for the training API referenced in llama.h
typedef void * ggml_opt_dataset_t;
typedef void * ggml_opt_result_t;

// Function pointer type for optimizer params callback (returned as opaque void*)
typedef void * ggml_opt_get_optimizer_params;

typedef void (*ggml_opt_epoch_callback)(bool train, void * opt_ctx,
                                        ggml_opt_dataset_t dataset,
                                        ggml_opt_result_t  result,
                                        int64_t ibatch, int64_t ibatch_max,
                                        int64_t idata,  int64_t idata_max,
                                        void * userdata);

enum ggml_opt_optimizer_type {
    GGML_OPT_OPTIMIZER_TYPE_ADAM  = 0,
    GGML_OPT_OPTIMIZER_TYPE_ADAMW = 1,
};

#ifdef __cplusplus
}
#endif
