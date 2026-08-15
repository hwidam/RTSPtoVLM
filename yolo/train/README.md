# YOLO Training

Python/Ultralytics side of the YOLO work: fine-tuning a pretrained model on
custom samples and exporting it to ONNX for the C++ inference sandbox in
[`../inference`](../inference).

## Setup
```
pip install -r requirements.txt
```

## Steps
1. Fill in `dataset/images/{train,val}` and matching YOLO-format label files
   (see [Ultralytics dataset docs](https://docs.ultralytics.com/datasets/detect/)).
2. Update `data.yaml` with the real class names.
3. `python train.py` — fine-tunes `yolo11n.pt`, then exports `best.onnx`.
4. Copy the exported `.onnx` into `../inference/models/` to test it in C++.
