// Standalone YOLO inference sandbox using OpenCV's dnn module.
// Built by the YoloSandbox project (yolo/inference/YoloSandbox.vcxproj,
// part of RTSPtoVLM.sln) -- run with a model path and an image path as
// command-line arguments; writes an annotated copy next to the input.
//
// Once this is working end-to-end, port the load/preprocess/decode/NMS
// logic here into VA/YOLOInference.h/.cpp as a class shaped like
// VA/VLMInference.h, so it can plug into the ShmReadLoop pipeline
// described in the README's Architecture section.

#include <opencv2/opencv.hpp>
#include <iostream>
#include <filesystem>

#ifdef _DEBUG
#pragma comment(lib, "opencv_core4130d.lib")
#pragma comment(lib, "opencv_imgproc4130d.lib")
#pragma comment(lib, "opencv_imgcodecs4130d.lib")
#pragma comment(lib, "opencv_dnn4130d.lib")
#else
#pragma comment(lib, "opencv_core4130.lib")
#pragma comment(lib, "opencv_imgproc4130.lib")
#pragma comment(lib, "opencv_imgcodecs4130.lib")
#pragma comment(lib, "opencv_dnn4130.lib")
#endif

namespace {

// Must match the training class order (see model.names in the Python
// venv) -- these are COCO's 80 classes, what the stock yolo11n.pt knows.
const std::vector<std::string> kClassNames = {
    "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat",
    "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat",
    "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack",
    "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball",
    "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket",
    "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple",
    "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair",
    "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse",
    "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator",
    "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"
};

constexpr int kInputSize = 640;
constexpr float kScoreThreshold = 0.25f;
constexpr float kNmsThreshold = 0.45f;

struct Detection {
    cv::Rect box;
    int classId = 0;
    float confidence = 0.0f;
};

// Decodes a YOLOv8/11-style ONNX output of shape [1, 84, 8400]
// (4 box coords + 80 class scores, no separate objectness score) into
// image-space boxes, then applies NMS.
std::vector<Detection> DecodeOutput(const cv::Mat& rawOutput, const cv::Size& originalSize) {
    // rawOutput dims: [1, 84, 8400] -> reshape to [84, 8400] -> transpose to [8400, 84]
    cv::Mat output = rawOutput.reshape(1, static_cast<int>(kClassNames.size() + 4));
    cv::transpose(output, output);

    const float scaleX = static_cast<float>(originalSize.width) / kInputSize;
    const float scaleY = static_cast<float>(originalSize.height) / kInputSize;

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> classIds;

    for (int i = 0; i < output.rows; ++i) {
        const float* row = output.ptr<float>(i);
        const float cx = row[0], cy = row[1], w = row[2], h = row[3];

        cv::Mat scores(1, static_cast<int>(kClassNames.size()), CV_32F, const_cast<float*>(row + 4));
        cv::Point classIdPoint;
        double maxScore;
        cv::minMaxLoc(scores, nullptr, &maxScore, nullptr, &classIdPoint);
        if (maxScore < kScoreThreshold) continue;

        const float x = (cx - w / 2.0f) * scaleX;
        const float y = (cy - h / 2.0f) * scaleY;
        boxes.emplace_back(cv::Rect(static_cast<int>(x), static_cast<int>(y),
                                     static_cast<int>(w * scaleX), static_cast<int>(h * scaleY)));
        confidences.push_back(static_cast<float>(maxScore));
        classIds.push_back(classIdPoint.x);
    }

    std::vector<int> keep;
    cv::dnn::NMSBoxes(boxes, confidences, kScoreThreshold, kNmsThreshold, keep);

    std::vector<Detection> detections;
    detections.reserve(keep.size());
    for (int idx : keep) {
        detections.push_back({boxes[idx], classIds[idx], confidences[idx]});
    }
    return detections;
}

void DrawDetections(cv::Mat& image, const std::vector<Detection>& detections) {
    for (const auto& d : detections) {
        cv::rectangle(image, d.box, cv::Scalar(255, 0, 255), 2);
        const std::string label = kClassNames[d.classId] + " " + cv::format("%.2f", d.confidence);
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
        cv::rectangle(image, {d.box.x, d.box.y - textSize.height - 6},
                      {d.box.x + textSize.width, d.box.y}, cv::Scalar(255, 0, 255), cv::FILLED);
        cv::putText(image, label, {d.box.x, d.box.y - 4}, cv::FONT_HERSHEY_SIMPLEX, 0.6,
                    cv::Scalar(255, 255, 255), 2);
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: YoloSandbox.exe <model.onnx> <image_path>\n";
        return 1;
    }

    const std::string modelPath = argv[1];
    const std::string imagePath = argv[2];

    cv::dnn::Net net = cv::dnn::readNetFromONNX(modelPath);
    if (net.empty()) {
        std::cerr << "failed to load model: " << modelPath << "\n";
        return 1;
    }

    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cerr << "failed to load image: " << imagePath << "\n";
        return 1;
    }

    cv::Mat blob = cv::dnn::blobFromImage(
        image, 1.0 / 255.0, cv::Size(kInputSize, kInputSize), cv::Scalar(), true, false);
    net.setInput(blob);

    cv::Mat output = net.forward();
    std::vector<Detection> detections = DecodeOutput(output, image.size());

    std::cout << "detected " << detections.size() << " objects\n";
    for (const auto& d : detections) {
        std::cout << "  " << kClassNames[d.classId] << ": " << d.confidence << "\n";
    }

    DrawDetections(image, detections);

    std::filesystem::path outPath = std::filesystem::path(imagePath).parent_path()
        / (std::filesystem::path(imagePath).stem().string() + "_detected.jpg");
    cv::imwrite(outPath.string(), image);
    std::cout << "saved to: " << outPath.string() << "\n";

    return 0;
}
