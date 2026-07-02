# RTSPtoVLM
https://github.com/hwidam/RTSPtoVLM
- 2026.06.21 ~
- 2026.07.01 ~ 2026.07.16 Vacation
## 1. Overview
Receive RTSP stream data, decode, apply VLM, extract Data from Image
- build a working skeleton in the overview stage for testing
- then decouple the processes to enhance each component individually.

## 2. Architecture
```
+----------------------------------+        +----------------------------------+
|         RTSPReceiver.exe         |        |             VA.exe               |
|                                  |        |                                  |
|  RtspSource                      |        |  ShmReadLoop (background thread) |
|   +- avformat / avcodec          |  IPC   |   +- WaitAndPop (33 ms)          |
|      (FFmpeg 8.1.1)              |------->|      +- cv::Mat reconstruct      |
|  FrameDecoder                    |        |                                  |
|   +- sws_scale -> BGR24          |        |  OnTimer (~30 fps)               |
|  FramePublisher                  |        |   +- RenderFrame (StretchDIBits) |
|   +- SharedMemory::Push          |        |   +- VLMInference::Push          |
+----------------------------------+        +----------------------------------+
         IPC: Windows Named Shared Memory (ring buffer, mutex + event)
         Payload: ShmFrameHeader + BGR24 pixels  (defined in common.dll)
```
- **RTSPReceiver** is launched as a child process by VA on startup (`CreateProcess`)
- **SharedMemory** ring buffer lives in `common.dll`; both processes link against it
- **ShmFrameHeader** (width, height, stride, format, pts, timestamp) prefixes each frame payload

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
