# YOLO Inference Sandbox

Standalone C++ prototype for running a YOLO ONNX model (pretrained, or the
output of [`../train`](../train)) via OpenCV's `dnn` module — no MFC,
shared memory, or VLM involved yet. Once this works on a static
image/video, the same load/pre-process/NMS logic moves into a
`YOLOInference` class inside `VA/`, mirroring `VLMInference`.

## Models
Drop `.onnx` weights into `models/` (gitignored — they're build artifacts,
not source). Start with a stock pretrained model (e.g. `yolo11n.onnx`
exported from Ultralytics) before swapping in a fine-tuned one from
`../train`.
