
// VADlg.cpp: 구현 파일
//

#include "pch.h"
#include "framework.h"
#include "VA.h"
#include "VADlg.h"
#include "afxdialogex.h"

#include <filesystem>

#include "../common/Logger.h"
#include "../common/iniHandler.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// 응용 프로그램 정보에 사용되는 CAboutDlg 대화 상자입니다.

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 지원입니다.

// 구현입니다.
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CVADlg 대화 상자



CVADlg::CVADlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_VA_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CVADlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_VIEW, m_View);
}

BEGIN_MESSAGE_MAP(CVADlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CVADlg 메시지 처리기

BOOL CVADlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 이 대화 상자의 아이콘을 설정합니다.  응용 프로그램의 주 창이 대화 상자가 아닐 경우에는
	//  프레임워크가 이 작업을 자동으로 수행합니다.
	SetIcon(m_hIcon, TRUE);			// 큰 아이콘을 설정합니다.
	SetIcon(m_hIcon, FALSE);		// 작은 아이콘을 설정합니다.

	//ShowWindow(SW_MAXIMIZE);

	//ShowWindow(SW_MINIMIZE);

	// TODO: 여기에 추가 초기화 작업을 추가합니다.
	Logger::Init("VA");

	if (IniHandler::Load())
	{
		const auto& rtspList = IniHandler::GetRtspList();
		Logger::Info("Loaded {} RTSP server(s)", rtspList.size());

		if (!rtspList.empty())
		{
			std::string url = rtspList[0].BuildUri();
			Logger::Info("Connecting to {}", url);
			StartCapture(url);
			SetTimer(1, 33, nullptr); // ~30 fps
		}
	}
	else
	{
		Logger::Warn("config.ini not found or failed to load");
	}

	InitVLM();

	return TRUE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CVADlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 대화 상자에 최소화 단추를 추가할 경우 아이콘을 그리려면
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 애플리케이션의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CVADlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 그리기를 위한 디바이스 컨텍스트입니다.

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 클라이언트 사각형에서 아이콘을 가운데에 맞춥니다.
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 아이콘을 그립니다.
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// 사용자가 최소화된 창을 끄는 동안에 커서가 표시되도록 시스템에서
//  이 함수를 호출합니다.
HCURSOR CVADlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CVADlg::InitVLM()
{
	m_pVLMInference = new VLMInference();

	if (m_pVLMInference != nullptr)
	{
		wchar_t buf[MAX_PATH];
		GetModuleFileNameW(nullptr, buf, MAX_PATH);
		std::filesystem::path exePath(buf);

		// <exe dir>/model/
		auto modelDir = exePath.parent_path() / "model\\";

		std::string modelPath = modelDir.string() + "llava-v1.6-mistral-7b.Q4_K_M.gguf";
		std::string mmprojPath = modelDir.string() + "mmproj-model-f16.gguf";
		m_pVLMInference->Init(modelPath, mmprojPath);
	}
}

void CVADlg::StartCapture(const std::string& url)
{
	StopCapture();
	m_running = true;
	m_captureThread = std::thread([this, url]() { CaptureLoop(url); });
}

void CVADlg::StopCapture()
{
	m_running = false;
	if (m_captureThread.joinable())
		m_captureThread.join();
}

void CVADlg::CaptureLoop(const std::string& url)
{
	// ── Step 1: verify FFmpeg is in this OpenCV build ─────────────────────
	std::string buildInfo = cv::getBuildInformation();
	bool hasFFmpeg = buildInfo.find("FFMPEG:                      YES") != std::string::npos;
	Logger::Info("OpenCV {} | FFMPEG backend: {}", CV_VERSION, hasFFmpeg ? "YES" : "NO");

	if (!hasFFmpeg)
	{
		Logger::Error("OpenCV was built without FFMPEG — RTSP not supported. Rebuild with -DWITH_FFMPEG=ON");
		return;
	}

	// ── Step 2: force TCP transport via environment variable ─────────────
	_putenv_s("OPENCV_FFMPEG_CAPTURE_OPTIONS", "rtsp_transport;tcp");

	cv::VideoCapture cap;
	cv::Mat frame;

	while (m_running)
	{
		Logger::Info("Connecting to RTSP stream ...");
		cap.open(url, cv::CAP_FFMPEG);

		if (!cap.isOpened())
		{
			Logger::Error("Failed to open stream — retrying in 5s");
			for (int i = 0; i < 50 && m_running; ++i)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		Logger::Info("Stream opened | {}x{} @ {:.1f}fps",
			(int)cap.get(cv::CAP_PROP_FRAME_WIDTH),
			(int)cap.get(cv::CAP_PROP_FRAME_HEIGHT),
			cap.get(cv::CAP_PROP_FPS));

		while (m_running)
		{
			if (!cap.read(frame) || frame.empty())
			{
				Logger::Warn("Frame read failed — reconnecting ...");
				break;
			}
			std::lock_guard<std::mutex> lock(m_frameMutex);
			cv::swap(m_frame, frame);
		}

		cap.release();
	}
}

void CVADlg::RenderFrame(const cv::Mat& frame)
{
	CRect rect;
	m_View.GetClientRect(&rect);
	if (rect.IsRectEmpty())
		return;

	// OpenCV Mat is BGR — same byte order Windows DIB expects, no conversion needed
	const cv::Mat& src = frame.isContinuous() ? frame : frame.clone();

	BITMAPINFOHEADER bi{};
	bi.biSize        = sizeof(bi);
	bi.biWidth       = src.cols;
	bi.biHeight      = -src.rows; // negative = top-down
	bi.biPlanes      = 1;
	bi.biBitCount    = 24;
	bi.biCompression = BI_RGB;

	CDC* pDC = m_View.GetDC();
	StretchDIBits(pDC->GetSafeHdc(),
		0, 0, rect.Width(), rect.Height(),
		0, 0, src.cols, src.rows,
		src.data, reinterpret_cast<BITMAPINFO*>(&bi),
		DIB_RGB_COLORS, SRCCOPY);
	m_View.ReleaseDC(pDC);
}

void CVADlg::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == 1)
	{
		cv::Mat frame;
		{
			std::lock_guard<std::mutex> lock(m_frameMutex);
			if (!m_frame.empty())
				m_frame.copyTo(frame);
		}
		if (!frame.empty())
		{
			//here
			m_pVLMInference->Push(frame, "write text written on image");
			RenderFrame(frame);
		}
	}
	CDialogEx::OnTimer(nIDEvent);
}

void CVADlg::OnDestroy()
{
	KillTimer(1);
	StopCapture();
	CDialogEx::OnDestroy();
}

