
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
#include "../common/FrameTypes.h"


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

	SetIcon(m_hIcon, TRUE);
	SetIcon(m_hIcon, FALSE);

	Logger::Init("VA");

	if (IniHandler::Load())
	{
		const auto& rtspList = IniHandler::GetRtspList();
		Logger::Info("Loaded {} RTSP server(s)", rtspList.size());

		if (!rtspList.empty())
		{
			const std::string shmName = "RtspFrame";
			std::string url = rtspList[0].BuildUri();
			Logger::Info("Launching RTSPReceiver for {}", url);

			LaunchReceiver(url, shmName);
			StartShmReader(shmName);
			SetTimer(1, 33, nullptr); // ~30 fps render timer
		}
	}
	else
	{
		Logger::Warn("config.ini not found or failed to load");
	}

	InitVLM();

	return TRUE;
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

void CVADlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this);
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

HCURSOR CVADlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// ── RTSPReceiver process ──────────────────────────────────────────────────────

void CVADlg::LaunchReceiver(const std::string& url, const std::string& shmName)
{
	// Resolve RTSPReceiver.exe path: same directory as VA.exe
	wchar_t vaBuf[MAX_PATH];
	GetModuleFileNameW(nullptr, vaBuf, MAX_PATH);
	std::filesystem::path recvExe =
		std::filesystem::path(vaBuf).parent_path() / "RTSPReceiver.exe";

	// CreateProcessA needs a mutable char buffer for the command line
	std::string cmdLine = "\"" + recvExe.string() + "\" \""
	                    + url + "\" " + shmName;

	STARTUPINFOA si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};

	if (!CreateProcessA(nullptr, cmdLine.data(),
	                    nullptr, nullptr, FALSE, 0,
	                    nullptr, nullptr, &si, &pi))
	{
		Logger::Error("LaunchReceiver: CreateProcess failed (error {})", GetLastError());
		return;
	}

	m_hReceiverProcess = pi.hProcess;
	CloseHandle(pi.hThread);
	Logger::Info("LaunchReceiver: PID {} started", pi.dwProcessId);
}

void CVADlg::StopReceiver()
{
	if (m_hReceiverProcess == INVALID_HANDLE_VALUE) return;

	TerminateProcess(m_hReceiverProcess, 0);
	WaitForSingleObject(m_hReceiverProcess, 3000);
	CloseHandle(m_hReceiverProcess);
	m_hReceiverProcess = INVALID_HANDLE_VALUE;
	Logger::Info("StopReceiver: process terminated");
}

// ── Shared memory reader ──────────────────────────────────────────────────────

void CVADlg::StartShmReader(const std::string& shmName)
{
	StopShmReader();
	m_running = true;
	m_shmThread = std::thread([this, shmName]() { ShmReadLoop(shmName); });
}

void CVADlg::StopShmReader()
{
	m_running = false;
	if (m_shmThread.joinable())
		m_shmThread.join();
}

void CVADlg::ShmReadLoop(const std::string& shmName)
{
	// Wait until RTSPReceiver has created the shared memory segment
	while (m_running && !m_shm.Open(shmName)) {
		Logger::Info("ShmReadLoop: waiting for '{}' ...", shmName);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}

	if (!m_running) return;
	Logger::Info("ShmReadLoop: connected to '{}'", shmName);

	ShmPacket pkt{};
	while (m_running)
	{
		if (!m_shm.WaitAndPop(pkt, 33))  // 33 ms timeout, non-blocking on idle
			continue;

		const auto* h = pkt.As<ShmFrameHeader>();
		if (!h || pkt.size < sizeof(ShmFrameHeader)) continue;

		// Wrap the copied pixel data in a Mat and clone into m_frame
		cv::Mat received(h->height, h->width, CV_8UC3,
		                 pkt.data + sizeof(ShmFrameHeader));

		std::lock_guard<std::mutex> lock(m_frameMutex);
		received.copyTo(m_frame);
	}

	m_shm.Close();
}

// ── Rendering ─────────────────────────────────────────────────────────────────

void CVADlg::RenderFrame(const cv::Mat& frame)
{
	CRect rect;
	m_View.GetClientRect(&rect);
	if (rect.IsRectEmpty()) return;

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

// ── Timer (~30 fps) ───────────────────────────────────────────────────────────

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
			m_pVLMInference->Push(frame, "write text written on image");
			RenderFrame(frame);
		}
	}
	CDialogEx::OnTimer(nIDEvent);
}

// ── Shutdown ──────────────────────────────────────────────────────────────────

void CVADlg::OnDestroy()
{
	KillTimer(1);
	StopShmReader();   // joins background thread, closes m_shm
	StopReceiver();    // terminates RTSPReceiver.exe
	CDialogEx::OnDestroy();
}

// ── VLM ──────────────────────────────────────────────────────────────────────

void CVADlg::InitVLM()
{
	m_pVLMInference = new VLMInference();

	if (m_pVLMInference != nullptr)
	{
		wchar_t buf[MAX_PATH];
		GetModuleFileNameW(nullptr, buf, MAX_PATH);
		std::filesystem::path exePath(buf);

		auto modelDir = exePath.parent_path() / "model\\";

		std::string modelPath  = modelDir.string() + "llava-v1.6-mistral-7b.Q4_K_M.gguf";
		std::string mmprojPath = modelDir.string() + "mmproj-model-f16.gguf";
		m_pVLMInference->Init(modelPath, mmprojPath);
	}
}
