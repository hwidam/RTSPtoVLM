// Standalone YOLO inference sandbox using OpenCV's dnn module.
// Not wired into the Visual Studio solution yet -- compile ad hoc while
// learning the API, e.g.:
//   cl /EHsc /I <opencv include> yolo_infer.cpp /link /LIBPATH:<opencv lib> opencv_world4130.lib
//
// Once this is working end-to-end (load model -> preprocess -> forward ->
// decode boxes -> NMS), port it into VA/YOLOInference.h/.cpp as a class
// shaped like VA/VLMInference.h, so it can plug into the ShmReadLoop
// pipeline described in the README's Architecture section.

#include <opencv2/opencv.hpp>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: yolo_infer <model.onnx> <image_path>\n";
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
        image, 1.0 / 255.0, cv::Size(640, 640), cv::Scalar(), true, false);
    net.setInput(blob);

    cv::Mat output = net.forward();
    std::cout << "output shape: " << output.size << "\n";

    // TODO: decode YOLO output (boxes/scores/classes), apply NMS
    // (cv::dnn::NMSBoxes), and draw results with cv::rectangle.

    return 0;
}
