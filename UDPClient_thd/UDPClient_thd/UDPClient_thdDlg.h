
// UDPClient_thdDlg.h : ��� ����
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include <afxtempl.h>

#define MAX_WINDOW_SIZE 10
#define SEQ_NUM_SIZE 20
#define DATA_SIZE 1000
class CDataSocket;

typedef TCHAR check_sum;
enum ROLE { MESSAGE, F_O, F_N, RR, RNR, POLLING};

typedef struct Packet
{
	TCHAR data[DATA_SIZE+1];
	TCHAR checkSum;
	TCHAR piggyBack;
	int seqNum;
	int size;
	ROLE role;
	int rr_number;
};

typedef struct TimeThreadArg
{
	CDialogEx *pDlg;
	int Thread_run;
	int rr_num;
	int poling_num;
}TimeThreadArg;

typedef struct ThreadArg
{
	CList<Packet, Packet&> *pTxList;
	CList<Packet, Packet&> *pRxList;
	CList<Packet, Packet&> *pTempList;
	CList<Packet, Packet&> *pAckList;
	CDialogEx *pDlg;
	int Thread_run;
	int* pTimeThread_run;
	int* pTimeThread_RR;
}ThreadArg;


// CUDPClient_thdDlg ��ȭ ����
class CUDPClient_thdDlg : public CDialogEx
{
	// �����Դϴ�.
public:
	CUDPClient_thdDlg(CWnd* pParent = NULL);	// ǥ�� �������Դϴ�.
	CDataSocket *m_pDataSocket;
	CWinThread *pTXThread, *pRxThread, *pTimerThread;
	ThreadArg argS, argR;
	TimeThreadArg argT;
	POSITION pos;
	BOOL rnr;
	int pre_sqNum;
	int tx_seqNum ;
	int pre_rr_num;
	int count;

	// ��ȭ ���� �������Դϴ�.
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_UDPCLIENT_THD_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV �����Դϴ�.


// �����Դϴ�.
protected:
	HICON m_hIcon;

	// ������ �޽��� �� �Լ�
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	CIPAddressCtrl m_ipAddr;
	CEdit m_tx_edit_short;
	CEdit m_tx_edit;
	CEdit m_rx_edit;
	afx_msg void OnClickedSend();
	afx_msg void OnClickedClose();
	void ProcessReceive(CDataSocket *pSocket, int nErrorCode);

	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnBnClickedFile();
};
