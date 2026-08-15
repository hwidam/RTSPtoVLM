"""Fine-tune a pretrained YOLO model on custom samples.

Usage:
    pip install -r requirements.txt
    python train.py
"""
from ultralytics import YOLO


def main():
    model = YOLO("yolo11n.pt")  # pretrained checkpoint, downloaded on first run

    model.train(
        data="data.yaml",
        epochs=100,
        imgsz=640,
    )

    model.export(format="onnx")  # -> runs/detect/train/weights/best.onnx


if __name__ == "__main__":
    main()
