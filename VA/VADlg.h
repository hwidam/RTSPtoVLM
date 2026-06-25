
// VADlg.h: 헤더 파일
//

#pragma once

#include "VLMInference.h"

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
	afx_msg void OnTimer(UINT_PTR nIDEvent);
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
	void StartCapture(const std::string& url);
	void StopCapture();
	void CaptureLoop(const std::string& url);
	void RenderToView(CStatic& view, const cv::Mat& frame);
	void RenderFrame(const cv::Mat& frame);

	std::thread        m_captureThread;
	std::atomic<bool>  m_running{ false };
	cv::Mat            m_frame;
	std::mutex         m_frameMutex;
public:
};
