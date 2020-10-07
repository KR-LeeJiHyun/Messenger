
// UDPServer_thdDlg.cpp : 구현 파일
//

#include "stdafx.h"
#include "UDPServer_thd.h"
#include "UDPServer_thdDlg.h"
#include "afxdialogex.h"
#include "DataSocket.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

CCriticalSection rx_cs;
CCriticalSection tx_cs;
CCriticalSection time_cs;
CCriticalSection ack_cs;

char txbuff[30];
char rrbuf[30];

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


// CUDPServer_thdDlg 대화 상자



CUDPServer_thdDlg::CUDPServer_thdDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_UDPSERVER_THD_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CUDPServer_thdDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT2, m_tx_edit_short);
	DDX_Control(pDX, IDC_EDIT1, m_tx_edit);
	DDX_Control(pDX, IDC_EDIT3, m_rx_edit);
}

BEGIN_MESSAGE_MAP(CUDPServer_thdDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_SEND, &CUDPServer_thdDlg::OnClickedSend)
	ON_BN_CLICKED(IDC_CLOSE, &CUDPServer_thdDlg::OnClickedClose)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_FILE, &CUDPServer_thdDlg::OnBnClickedFile)
END_MESSAGE_MAP()


// CUDPServer_thdDlg 메시지 처리기

UINT RXThread(LPVOID arg)
{
	ThreadArg *pArg = (ThreadArg *)arg;
	CUDPServer_thdDlg *pDlg = (CUDPServer_thdDlg *)pArg->pDlg;
	CList<Packet, Packet&> *pRxList = pArg->pRxList;
	CList<Packet, Packet&> *pTxList = pArg->pTxList;
	CList<Packet, Packet&> *pAckList = pArg->pAckList;
	CList<Packet, Packet&> *pTempList = pArg->pTempList;
	CString fileName = _T("");
	CFile *targetFile = NULL;
	int pre_pkt_seqNum = 0;

	while (pArg->Thread_run)
	{
		POSITION pos = pRxList->GetHeadPosition();
		POSITION current_pos;

		while (pos != NULL)
		{
			if (pRxList->GetCount() == MAX_WINDOW_SIZE)
			{
				Packet ack_pkt;
				ack_pkt.role = RNR;

				ack_cs.Lock();
				pAckList->AddTail(ack_pkt);
				ack_cs.Unlock();
			}

			current_pos = pos;

			rx_cs.Lock();
			Packet rx_pkt = pRxList->GetNext(pos);
			rx_cs.Unlock();

			if (pre_pkt_seqNum == rx_pkt.seqNum) {
				pre_pkt_seqNum = (rx_pkt.seqNum + 1) % SEQ_NUM_SIZE;
				if (rx_pkt.role == MESSAGE)
				{

					CString message;
					pDlg->m_rx_edit.GetWindowText(message);
					message += rx_pkt.data;
					pDlg->m_rx_edit.SetWindowText(message);
					pDlg->m_rx_edit.LineScroll(pDlg->m_rx_edit.GetLineCount());
				}

				else if (rx_pkt.role == F_N)
				{
					fileName += rx_pkt.data;
				}

				else if (rx_pkt.role == F_O)
				{
					if (targetFile == NULL)
					{
						AfxMessageBox(_T("파일 쓰기를 시작합니다."), MB_ICONINFORMATION);
						targetFile = new CFile(fileName, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary);
					}

					if (rx_pkt.size > 0)
					{
						targetFile->Write(rx_pkt.data, rx_pkt.size);
					}

					else if (rx_pkt.size <= 0)
					{
						CFile* closeFile = targetFile;
						targetFile = NULL;
						fileName.Delete(0, fileName.GetLength());
						closeFile->Close();
						AfxMessageBox(_T("파일 쓰기를 완료했습니다."), MB_ICONINFORMATION);
					}
				}
			}

			rx_cs.Lock();
			pRxList->RemoveAt(current_pos);
			rx_cs.Unlock();

			Packet ack_pkt;
			ack_pkt.role = RR;
			ack_pkt.rr_number = (rx_pkt.seqNum + 1) % SEQ_NUM_SIZE;

			ack_cs.Lock();
			pAckList->AddTail(ack_pkt);
			ack_cs.Unlock();
		}
		Sleep(10);
	}

	return 0;
}

UINT TXThread(LPVOID arg)
{
	ThreadArg *pArg = (ThreadArg *)arg;
	CUDPServer_thdDlg *pDlg = (CUDPServer_thdDlg *)pArg->pDlg;
	CList<Packet, Packet&> *pTxList = pArg->pTxList;
	CList<Packet, Packet&> *pRxList = pArg->pRxList;
	CList<Packet, Packet&> *pAckList = pArg->pAckList;
	CList<Packet, Packet&> *pTempList = pArg->pTempList;
	int pre_pkt_seqNum = 0;

	while (pArg->Thread_run)
	{
		POSITION pos = pTxList->GetHeadPosition();
		POSITION ack_pos = pAckList->GetHeadPosition();
		POSITION current_pos;
		POSITION ack_current_pos;

			if (ack_pos != NULL)
			{
				ack_cs.Lock();
				ack_current_pos = ack_pos;

				Packet ack_pkt = pTxList->GetNext(ack_pos);

				if (ack_pkt.role == POLLING)
				{
					pDlg->m_pDataSocket->SendTo((TCHAR*)&ack_pkt, sizeof(struct Packet), pDlg->peerPort, pDlg->peerAddr);
					sprintf(txbuff, "Polling을 보내고 있습니다.\r\n", ack_pkt.rr_number);
					OutputDebugStringA(txbuff);
					OutputDebugString(_T("\r\n"));
					pAckList->RemoveAt(ack_current_pos);
				}

				else if (ack_pkt.role == RNR)
				{
					pDlg->rnr = TRUE;

					if (pos != NULL &&pTempList->GetCount() < MAX_WINDOW_SIZE) 
					{
						tx_cs.Lock();
						current_pos = pos;

							Packet tx_pkt = pTxList->GetNext(pos);
							tx_pkt.piggyBack = 2;

							if (pre_pkt_seqNum == tx_pkt.seqNum)
							{
								pDlg->m_pDataSocket->SendTo((TCHAR*)&tx_pkt, sizeof(struct Packet), pDlg->peerPort, pDlg->peerAddr);								
								time_cs.Lock();
								*pArg->pTimeThread_run = 1;
								*pArg->pTimeThread_RR = (tx_pkt.seqNum + 1) % SEQ_NUM_SIZE;
								sprintf(txbuff, "%d 피기백 RNR 데이터를 보내고 있습니다.\r\n", tx_pkt.seqNum);
								OutputDebugStringA(txbuff);
								OutputDebugString(tx_pkt.data);
								OutputDebugString(_T("\r\n"));
								pre_pkt_seqNum = (tx_pkt.seqNum + 1) % SEQ_NUM_SIZE;
								time_cs.Unlock();
								pTxList->RemoveAt(current_pos);
								pTempList->AddTail(tx_pkt);
							}

							else 
							{
								OutputDebugString(_T("치명적인 오류 Tx_List의 순서가 바뀌었습니다.\r\n"));
							}
						tx_cs.Unlock();
					}
					
					else 
					{
						pDlg->m_pDataSocket->SendTo((TCHAR*)&ack_pkt, sizeof(struct Packet), pDlg->peerPort, pDlg->peerAddr);
						sprintf(txbuff, "RNR을 보내고 있습니다.\r\n", ack_pkt.rr_number);
						OutputDebugStringA(txbuff);
						OutputDebugString(_T("\r\n"));
					}
					pAckList->RemoveAt(ack_current_pos);
				}

				else if (ack_pkt.role == RR)
				{
					if (!pDlg->rnr)
					{
						if (pos != NULL &&pTempList->GetCount() < MAX_WINDOW_SIZE)
						{
							tx_cs.Lock();
							current_pos = pos;

							Packet tx_pkt = pTxList->GetNext(pos);
							tx_pkt.piggyBack = 1;
							tx_pkt.rr_number = ack_pkt.rr_number;

							if (pre_pkt_seqNum == tx_pkt.seqNum)
							{
								pDlg->m_pDataSocket->SendTo((TCHAR*)&tx_pkt, sizeof(struct Packet), pDlg->peerPort, pDlg->peerAddr);								
								time_cs.Lock();
								*pArg->pTimeThread_run = 1;
								*pArg->pTimeThread_RR = (tx_pkt.seqNum + 1) % SEQ_NUM_SIZE;
								sprintf(txbuff, "%d 피기백 RR %d 데이터를 보내고 있습니다.\r\n", tx_pkt.rr_number,tx_pkt.seqNum);
								OutputDebugStringA(txbuff);
								OutputDebugString(tx_pkt.data);
								OutputDebugString(_T("\r\n"));
								pre_pkt_seqNum = (tx_pkt.seqNum + 1) % SEQ_NUM_SIZE;
								time_cs.Unlock();
								pTxList->RemoveAt(current_pos);
								pTempList->AddTail(tx_pkt);
							}

							else
							{
								OutputDebugString(_T("치명적인 오류 Tx_List의 순서가 바뀌었습니다.\r\n"));
							}
							tx_cs.Unlock();
						}
						else 
						{
							pDlg->m_pDataSocket->SendTo((TCHAR*)&ack_pkt, sizeof(struct Packet), pDlg->peerPort, pDlg->peerAddr);
							sprintf(txbuff, "%d RR을 보내고 있습니다.\r\n", ack_pkt.rr_number);
							OutputDebugStringA(txbuff);
							OutputDebugString(_T("\r\n"));
						}
						pAckList->RemoveAt(ack_current_pos);
					}
				}

				else
				{
					if (pos != NULL)
					{
						tx_cs.Lock();
						current_pos = pos;

						if (pTempList->GetCount() < MAX_WINDOW_SIZE)
						{
							Packet tx_pkt = pTxList->GetNext(pos);

							if (pre_pkt_seqNum == tx_pkt.seqNum)
							{
								pDlg->m_pDataSocket->SendTo((TCHAR*)&tx_pkt, sizeof(struct Packet), pDlg->peerPort, pDlg->peerAddr);
								--pDlg->count;
								time_cs.Lock();
								*pArg->pTimeThread_run = 1;
								*pArg->pTimeThread_RR = (tx_pkt.seqNum + 1) % SEQ_NUM_SIZE;
								sprintf(txbuff, "%d 데이터를 보내고 있습니다.\r\n", tx_pkt.seqNum);
								OutputDebugStringA(txbuff);
								OutputDebugString(tx_pkt.data);
								OutputDebugString(_T("\r\n"));
								pre_pkt_seqNum = (tx_pkt.seqNum + 1) % SEQ_NUM_SIZE;
								time_cs.Unlock();
								pTxList->RemoveAt(current_pos);
								pTempList->AddTail(tx_pkt);
							}

							else
							{
								OutputDebugString(_T("치명적인 오류 Tx_List의 순서가 바뀌었습니다.\r\n"));
							}
						}
						tx_cs.Unlock();
					}

					else 
					{
						OutputDebugString(_T("치명적인 오류 Ack_List의 순서가 바뀌었습니다.\r\n"));
					}
				}
				ack_cs.Unlock();
			}

			else if (pos != NULL)
			{
				tx_cs.Lock();
				current_pos = pos;

				if (pTempList->GetCount() < MAX_WINDOW_SIZE)
				{
					Packet tx_pkt = pTxList->GetNext(pos);

					if (pre_pkt_seqNum == tx_pkt.seqNum)
					{
						pDlg->m_pDataSocket->SendTo((TCHAR*)&tx_pkt, sizeof(struct Packet), pDlg->peerPort, pDlg->peerAddr);
						--pDlg->count;
						time_cs.Lock();
						*pArg->pTimeThread_run = 1;
						*pArg->pTimeThread_RR = (tx_pkt.seqNum + 1) % SEQ_NUM_SIZE;
						sprintf(txbuff, "%d 데이터를 보내고 있습니다.\r\n", tx_pkt.seqNum);
						OutputDebugStringA(txbuff);
						OutputDebugString(tx_pkt.data);
						OutputDebugString(_T("\r\n"));
						pre_pkt_seqNum = (tx_pkt.seqNum + 1) % SEQ_NUM_SIZE;
						time_cs.Unlock();
						pTxList->RemoveAt(current_pos);
						pTempList->AddTail(tx_pkt);
					}

					else
					{
						OutputDebugString(_T("치명적인 오류 Tx_List의 순서가 바뀌었습니다.\r\n"));
					}
				}
				tx_cs.Unlock();
		}
		Sleep(500);
	}
	return 0;
}

UINT TimerThread(LPVOID arg)
{
	TimeThreadArg *pArg = (TimeThreadArg *)arg;
	CUDPServer_thdDlg *pDlg = (CUDPServer_thdDlg*)pArg->pDlg;

	while (true)
	{
		if (pArg->Thread_run == 1)
		{
			pDlg->SetTimer(pArg->rr_num, 3000000000000000, NULL);
			time_cs.Lock();
			pArg->Thread_run = 3;
			time_cs.Unlock();
		}
		else if (pArg->Thread_run == 2)
		{
			pDlg->SetTimer(10, 3000, NULL);
			time_cs.Lock();
			pArg->poling_num = 1;
			pArg->Thread_run = 3;
			time_cs.Unlock();
		}
		else if (pArg->Thread_run == 0) break;

		else continue;
	}

	return 0;
}

BOOL CUDPServer_thdDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 시스템 메뉴에 "정보..." 메뉴 항목을 추가합니다.

	// IDM_ABOUTBOX는 시스템 명령 범위에 있어야 합니다.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
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

	// TODO: 여기에 추가 초기화 작업을 추가합니다.
	/* 변수 선언 및 초기화 */
	peerAddr = _T("");
	peerPort = NULL;
	CList<Packet, Packet&> *pTxList = new CList<Packet, Packet&>;
	CList<Packet, Packet&> *pRxList = new CList<Packet, Packet&>;
	CList<Packet, Packet&> *pAckList = new CList<Packet, Packet&>;
	CList<Packet, Packet&> *pTempList = new CList<Packet, Packet&>;

	argR.pTxList = pTxList;
	argR.pRxList = pRxList;
	argR.pAckList = pAckList;
	argR.pTempList = pTempList;
	argR.pDlg = this;
	argR.Thread_run = 1;


	argS.pTxList = pTxList;
	argS.pRxList = pRxList;
	argS.pAckList = pAckList;
	argS.pTempList = pTempList;
	argS.pDlg = this;
	argS.Thread_run = 1;
	argS.pTimeThread_run = &argT.Thread_run;
	argS.pTimeThread_RR = &argT.rr_num;

	argT.pDlg = this;
	argT.Thread_run = 3;
	argT.poling_num = 0;

	pos = NULL;
	rnr = FALSE;
	tx_seqNum = 0;
	pre_rr_num = 1;
	count = 0;

	WSADATA wsa;
	int error_code;

	/* 윈속 초기화 */
	if ((error_code = WSAStartup(MAKEWORD(2, 2), &wsa)) != 0)
	{
		TCHAR buffer[256];
		FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error_code,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, 256, NULL);
		AfxMessageBox(buffer, MB_ICONERROR);
	}

	m_pDataSocket = NULL;
	m_pDataSocket = new CDataSocket(this);
	if (m_pDataSocket->Create(8000, SOCK_DGRAM))
	{
		AfxMessageBox(_T("서버를 시작합니다."), MB_ICONINFORMATION);
		pTXThread = AfxBeginThread(TXThread, (LPVOID)&argS);
		pRxThread = AfxBeginThread(RXThread, (LPVOID)&argR);
		pTimerThread = AfxBeginThread(TimerThread, (LPVOID)&argT);

		return TRUE;
	}

	AfxMessageBox(_T("이미 실행 중인 서버가 있습니다.") _T("\n프로그램을 종료합니다."), MB_ICONERROR);

	return FALSE;  // 포커스를 컨트롤에 설정하지 않으면 TRUE를 반환합니다.
}

void CUDPServer_thdDlg::OnSysCommand(UINT nID, LPARAM lParam)
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
//  아래 코드가 필요합니다.  문서/뷰 모델을 사용하는 MFC 응용 프로그램의 경우에는
//  프레임워크에서 이 작업을 자동으로 수행합니다.

void CUDPServer_thdDlg::OnPaint()
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
HCURSOR CUDPServer_thdDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CUDPServer_thdDlg::ProcessReceive(CDataSocket * pSocket, int nErrorCode)
{
	/* 변수 선언 */
	TCHAR pBuf[sizeof(Packet) + 1]; // 수신 버퍼
	TCHAR error = 0;
	Packet *rx_pkt;
	int nbytes;
	int len;

	/* 패킷 받기 */
	nbytes = pSocket->ReceiveFrom(pBuf, sizeof(Packet), peerAddr, peerPort);
	pBuf[nbytes] = '\0';
	rx_pkt = (Packet *)pBuf;

	/* 패킷 종류에 따라 처리 */
	if (rx_pkt->role == MESSAGE || rx_pkt->role == F_O || rx_pkt->role == F_N)
	{
		/* 체크섬 오류 검출 */
		int reapt;

		if (rx_pkt->role == F_O) reapt = rx_pkt->size / sizeof(TCHAR);

		else reapt = rx_pkt->size;

		for (int i = 0; i < reapt; ++i)
		{
			error += rx_pkt->data[i];
		}

		error += rx_pkt->checkSum;
		error = ~error;

		/* 에러 발생  */
		if (error != 0)
		{
			MessageBox(_T("데이터가 깨졌습니다."), _T("알림"), MB_ICONERROR);
			return;
		}

		if (argR.pRxList->GetCount() < MAX_WINDOW_SIZE)
		{
			rx_cs.Lock();
			argR.pRxList->AddTail(*rx_pkt);
			rx_cs.Unlock();
		}
		else OutputDebugString(_T("재전송을 무시합니다.\r\n"));

	}

	if ((rx_pkt->role == RR || rx_pkt->piggyBack == 1) && rx_pkt->rr_number == pre_rr_num)
	{
		this->KillTimer(rx_pkt->rr_number);
		if (argT.poling_num == 1)
		{
			this->KillTimer(10);
			time_cs.Lock();
			argT.poling_num = 0;
			time_cs.Unlock();
		}

		pre_rr_num = (rx_pkt->rr_number + 1) % SEQ_NUM_SIZE;

		if (rx_pkt->piggyBack == 1)
		{
			OutputDebugString(_T("피기백"));
		}


		if (argS.pTempList->GetCount() != 0)
		{
			tx_cs.Lock();
			argS.pTempList->RemoveHead();
			OutputDebugString(_T("RR을 받았습니다.\r\n"));
			OutputDebugString(_T("\r\n"));
			tx_cs.Unlock();
		}
	}
		else if (rx_pkt->role == RNR || rx_pkt->piggyBack == 2)
		{
			for (int i = 0; i<SEQ_NUM_SIZE; ++i)
				this->KillTimer(i);
			time_cs.Lock();
			argT.Thread_run = 2;
			argT.poling_num = 1;
			time_cs.Unlock();
			OutputDebugString(_T("RNR을 받았습니다.\r\n"));
			OutputDebugString(_T("\r\n"));
		}

		else if (rx_pkt->role == POLLING)
		{

			if (argR.pRxList->GetCount() < MAX_WINDOW_SIZE)
			{
				ack_cs.Lock();
				rnr = FALSE;
				ack_cs.Unlock();
			}

			else
			{
				Packet ack_pkt;
				ack_pkt.role = RNR;
				ack_cs.Lock();
				argS.pAckList->AddHead(ack_pkt);
				ack_cs.Unlock();
			}
		}
}

TCHAR calCheckSum(TCHAR * data, int len) {
	TCHAR result = 0;
	for (int i = 0; i < len; ++i) {
		result += data[i];
	}
	result = ~result;
	return result;
}

void CUDPServer_thdDlg::OnClickedSend()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	CString tx_message;

	/* 보낼 메시지 받아오기 */
	m_tx_edit_short.GetWindowText(tx_message);
	tx_message += _T("\r\n");
	m_tx_edit_short.SetWindowTextW(_T(""));
	m_tx_edit_short.SetFocus();

	/* 보낸 메시지 tx_edit에 출력 */
	int len = m_tx_edit.GetWindowTextLengthW();
	m_tx_edit.SetSel(len, len);
	m_tx_edit.ReplaceSel(tx_message);
	m_tx_edit.LineScroll(m_tx_edit.GetLineCount());

	int k = 0;

	/* 데이터를 나눠서 패킷으로 만들어 리스트에 추가 */
	while (tx_message.GetLength() > 0) {
		Packet *tx_pkt = new Packet;
		tx_pkt->role = MESSAGE;
		for (int i = 0; i < DATA_SIZE; ++i) {
			tx_pkt->data[i] = tx_message[i];
			k = i;
			if (tx_pkt->data[i] == '\0') break;
		}
		tx_message.Delete(0, k + 1);
		tx_pkt->seqNum = tx_seqNum;
		tx_seqNum = (tx_seqNum + 1) % SEQ_NUM_SIZE;
		tx_pkt->data[k + 1] = '\0';
		tx_pkt->size = k + 2;
		tx_pkt->checkSum = calCheckSum(tx_pkt->data, tx_pkt->size);
		tx_pkt->piggyBack = 0;
		tx_pkt->rr_number = -1;

		tx_cs.Lock();
		argS.pTxList->AddTail(*tx_pkt);
		tx_cs.Unlock();
	}
}


void CUDPServer_thdDlg::OnClickedClose()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	if (m_pDataSocket == NULL)
	{
		AfxMessageBox(_T("이미 접속 종료"));
	}
	else
	{
		/* 스레드 중지 시키기 */
		argS.Thread_run = 0;
		argR.Thread_run = 0;

		/* 데이터 소켓 제거 */
		m_pDataSocket->Close();
		delete m_pDataSocket;
		m_pDataSocket = NULL;
	}
}


void CUDPServer_thdDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 여기에 메시지 처리기 코드를 추가 및/또는 기본값을 호출합니다.
	if (nIDEvent == 10) // 폴링 타이머 처리
	{
		Packet poll_ack;
		poll_ack.role = POLLING;

		tx_cs.Lock();
		argS.pAckList->AddTail (poll_ack);
		tx_cs.Unlock();
		OutputDebugString(_T("폴링타이머 실행 \r\n"));
	}

	else
	{
		for (int i = 0; i < SEQ_NUM_SIZE; ++i)
			KillTimer(i);
		if (count != 0 && argT.poling_num == 0)
		{

			tx_cs.Lock();
			pos = argS.pTxList->GetHeadPosition();
			pre_sqNum = argS.pTxList->GetHead().seqNum;
			count = 0;
			tx_cs.Unlock();
			OutputDebugString(_T("재전송타이머 실행 \r\n"));
		}
	}
	CDialogEx::OnTimer(nIDEvent);
}


void CUDPServer_thdDlg::OnBnClickedFile()
{
	// TODO: 여기에 컨트롤 알림 처리기 코드를 추가합니다.
	static TCHAR BASED_CODE Filter[] = _T("이미지 파일(*.bmp, *.gif, *.jpg) |*.gif;*.jpg;*.bmp |문서파일(*.hwp, *.txt, *.docx)|*.hwp;*.txt;*.docx|모든파일(*.*)|*.*|");
	CFileDialog fDlg(TRUE, _T("*.jpg"), _T("사진"), OFN_HIDEREADONLY, Filter);
	int k = 0;
	if (IDOK == fDlg.DoModal()) {
		CString pathName = fDlg.GetPathName();
		CFile sendFile;
		sendFile.Open(pathName, CFile::modeRead | CFile::typeBinary);
		int NameLength = pathName.GetLength();

		TCHAR *strName = new TCHAR[NameLength];
		CString strFileName;
		strFileName = sendFile.GetFileName();
		strName = strFileName.GetBuffer(NameLength);

		while (strFileName.GetLength() > 0) {
			Packet *tx_pkt = new Packet;
			tx_pkt->role = F_N;
			for (int i = 0; i < DATA_SIZE; ++i) {
				//tx_pkt->data[i] = pathName[i];
				tx_pkt->data[i] = strName[i];
				k = i;
				if (tx_pkt->data[i] == '\0') break;
			}
			strFileName.Delete(0, k + 1);

			tx_pkt->seqNum = tx_seqNum;
			tx_seqNum = (tx_seqNum + 1) % SEQ_NUM_SIZE;
			tx_pkt->data[k + 1] = '\0';
			tx_pkt->size = k + 2;
			tx_pkt->checkSum = calCheckSum(tx_pkt->data, tx_pkt->size);
			tx_pkt->piggyBack = 0;
			tx_pkt->rr_number = -1;

			tx_cs.Lock();
			argS.pTxList->AddTail(*tx_pkt);
			tx_cs.Unlock();
		}

		DWORD dwRead;

		do
		{
			Packet *tx_pkt = new Packet;
			tx_pkt->role = F_O;
			dwRead = sendFile.Read((tx_pkt->data), (DATA_SIZE + 1) * sizeof(TCHAR));
			tx_pkt->size = dwRead;
			tx_pkt->checkSum = calCheckSum(tx_pkt->data, (tx_pkt->size) / sizeof(TCHAR));
			tx_pkt->seqNum = tx_seqNum;
			tx_seqNum = (tx_seqNum + 1) % SEQ_NUM_SIZE;
			tx_pkt->piggyBack = 0;
			tx_pkt->rr_number = -1;

			tx_cs.Lock();
			argS.pTxList->AddTail(*tx_pkt);
			tx_cs.Unlock();
		} while (dwRead > 0);
	}
}
