
// VADlg.h: 헤더 파일
//

#pragma once

#include "VLMInference.h"
#include "../common/SharedMemory.h"

#define WM_RENDER_FRAME (WM_APP + 2)

// CVADlg 대화 상자
class CVADlg : public CDialogEx
{
// 생성입니다.
public:
	CVADlg(CWnd* pParent = nullptr);	// 표준 생성자입니다.

// 대화 상자 데이터입니다.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_VA_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 지원입니다.


// 구현입니다.
protected:
	HICON m_hIcon;

	// 생성된 메시지 맵 함수
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg LRESULT OnRenderFrame(WPARAM, LPARAM);
	afx_msg void OnDestroy();
	afx_msg LRESULT OnVlmResult(WPARAM wParam, LPARAM lParam);
	DECLARE_MESSAGE_MAP()

public:
	CStatic m_ViewLive;
	CStatic m_ViewResult;

	VLMInference* m_pVLMInference = nullptr;
	void InitVLM();
	void InitControl();

private:
	// ── RTSPReceiver child process ────────────────────────────────────────────
	void LaunchReceiver(const std::string& url, const std::string& shmName);
	void StopReceiver();

	// ── Shared memory reader thread ───────────────────────────────────────────
	void StartShmReader(const std::string& shmName);
	void StopShmReader();
	void ShmReadLoop(const std::string& shmName);

	// ── Render thread ─────────────────────────────────────────────────────────
	void RenderLoop();
	void TriggerRedraw(); // platform seam: Windows=PostMessage, future Qt=emit signal

	void RenderToView(CStatic& view, const cv::Mat& frame);

	HANDLE                   m_hReceiverProcess = INVALID_HANDLE_VALUE;
	SharedMemory             m_shm;
	std::thread              m_shmThread;
	std::thread              m_renderThread;
	std::atomic<bool>        m_running{ false };
	cv::Mat                  m_frame;
	uint64_t                 m_frameTimestamp = 0; // guarded by m_frameMutex
	std::mutex               m_frameMutex;
	std::condition_variable  m_frameCv;
	bool                     m_frameReady = false; // guarded by m_frameMutex
	std::atomic<bool>        m_renderPending{ false };
	std::atomic<int64_t>     m_timeDiffMs{ 0 };
};
