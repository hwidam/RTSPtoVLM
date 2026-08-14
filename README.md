# RTSPtoVLM
https://github.com/hwidam/RTSPtoVLM
- 2026.06.21 ~
- 2026.07.01 ~ 2026.07.16 Vacation
- ~ 2026.08.10 Project Halt
- 2026.08.14 Project Resume

## 1. Overview
Receive RTSP stream data, decode, apply VLM, extract Data from Image
- build a working skeleton in the overview stage for testing
- then decouple the processes to enhance each component individually.

## 2. Architecture
```
+----------------------------------+        +------------------------------------------+
|         RTSPReceiver.exe         |        |                 VA.exe                   |
|                                  |        |                                          |
|  RtspSource                      |        |  ShmReadLoop (background thread)         |
|   +- avformat / avcodec          |  IPC   |   +- WaitAndPop (33 ms)                  |
|      (FFmpeg 8.1.1)              |------->|      +- cv::Mat reconstruct              |
|  FrameDecoder                    |        |      +- YOLOInference::Push (every frame)|
|   +- sws_scale -> BGR24          |        |           +- detections of interest?     |
|  FramePublisher                  |        |              +- yes -> VLMInference::Push|
|   +- SharedMemory::Push          |        |                                          |
+----------------------------------+        |  OnTimer (~30 fps)                       |
                                            |   +- RenderFrame (StretchDIBits + boxes) |
                                            +------------------------------------------+
         IPC: Windows Named Shared Memory (ring buffer, mutex + event)
         Payload: ShmFrameHeader + BGR24 pixels  (defined in common.dll)
```
- **RTSPReceiver** is launched as a child process by VA on startup (`CreateProcess`)
- **SharedMemory** ring buffer lives in `common.dll`; both processes link against it
- **ShmFrameHeader** (width, height, stride, format, pts, timestamp) prefixes each frame payload
- **YOLOInference** runs on every reconstructed frame and acts as a gate in front of the VLM: only frames containing a detection class of interest (configurable) are forwarded to `VLMInference::Push`, so the heavier VLM pass isn't run on empty/uninteresting frames
- Detection boxes from YOLOInference are drawn onto the frame in `RenderFrame`, independent of whether that frame was forwarded to the VLM

## 3. Tech Stack
### Language
- C++
### Framework
- MFC
### Libraries
- opencv 4.13.0( https://github.com/opencv/opencv )
- llama.cpp( https://github.com/ggml-org/llama.cpp )
- FFmpeg 8.1.1
### OpenSources
- spdlog( https://github.com/gabime/spdlog )
- simpleini( https://github.com/brofield/simpleini )
### Development Tools
- Git (Git Extensions)
- Visual Studio 2022
- Visual Studio Code
- ChatGPT / Claude: Used as development assistants for debugging, code review, and documentation improvement
