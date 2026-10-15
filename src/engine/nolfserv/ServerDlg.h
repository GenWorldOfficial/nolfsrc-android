// ServerDlg.h : the server's main dialog, CServerDlg, and the handler the
// server DLL talks back through, CGameServerAppHandler.
//
// Reconstructed from the retail NolfServ.exe (ServerDlg.cpp:
// 0x403990 - 0x408d7f). Member offsets are the retail ones (CServerDlg is
// 0x2c64 bytes; its constructor is at 0x403a30).
//

#if !defined(AFX_SERVERDLG_H__NOLFSERV__INCLUDED_)
#define AFX_SERVERDLG_H__NOLFSERV__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include <afxtempl.h>
#include "resource.h"
#include "ltbasedefs.h"
#include "server_interface.h"
#include "NetDefs.h"
#include "ButeMgr.h"


/////////////////////////////////////////////////////////////////////////////
// CGameServerAppHandler: how the server (server.dll) and the game's server
// shell (Object.lto, through ServerDE::SendToServerApp) talk to the dialog.
// Vtable 0x43d0e8.

class CGameServerAppHandler : public ServerAppHandler
{
public:
	LTRESULT ShellMessageFn(char* pInfo, uint32 nLen);		// 0x4039a0
	LTRESULT ConsoleOutputFn(char* pMsg);					// 0x4039c0
	LTRESULT OutOfMemory();									// 0x4039e0
	LTRESULT ProcessPacket(char* pData, uint32 dataLen, uint8 senderAddr[4], uint16 senderPort);	// 0x403a00
};


/////////////////////////////////////////////////////////////////////////////
// CServerDlg dialog

class CServerDlg : public CDialog
{
// Construction
public:
	CServerDlg(CWnd* pParent = NULL);	// standard constructor (0x403a30)
	~CServerDlg();						// 0x403e90

// Dialog Data
	//{{AFX_DATA(CServerDlg)
	enum { IDD = IDD_GAMESERVER };
	CButton		m_PlayerBoot;			// 0x05c  IDC_PLAYER_BOOT
	CListCtrl	m_Players;				// 0x098  IDC_PLAYERS
	CListCtrl	m_Levels;				// 0x0d4  IDC_LEVELS
	CEdit		m_edConsole;			// 0x110  IDC_CONSOLE_WINDOW
	CString		m_sServerName;			// 0x14c  IDC_SERVER_NAME
	CString		m_sServerTime;			// 0x150  IDC_SERVER_TIME
	CString		m_sTimeInLevel;			// 0x154  IDC_TIMEINLEVEL
	DWORD		m_nPeakPlayers;			// 0x158  IDC_PEAKPLAYERS
	DWORD		m_nTotalPlayers;		// 0x15c  IDC_TOTALPLAYERS
	DWORD		m_nAveragePing;			// 0x160  IDC_AVERAGEPING
	DWORD		m_nGamePlayers;			// 0x164  IDC_GAME_PLAYERS
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CServerDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CServerDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnConsoleSend();
	afx_msg void OnConsoleClear();
	afx_msg void OnCommandsNextLevel();
	afx_msg void OnCommandsSelectLevel();
	afx_msg void OnPlayersBoot();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDblclkLevels(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnStopserver();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
	// The settings: NetHost.txt's "Host" tag, kept in m_ButeMgr. Used by
	// the wizard's pages as well as the dialog.
	CString				GetSettingString(const char* szVar, const char* szDefault);	// 0x406c80
	float				GetSettingFloat(const char* szVar, float fDefault);				// 0x406d40
	void				SetSettingString(const char* szVar, const char* szValue);		// 0x406d80
	void				SetSettingFloat(const char* szVar, float fValue);				// 0x406e00

	// The server options (Attributes\ServerOptions.txt, m_lstServerOptions)
	ServerOption*		GetServerOption(const char* szVar);								// 0x4079e0
	float				GetServerOptionDefault(const char* szVar);						// 0x407a30
	float				GetServerSettingFloat(const char* szVar);						// 0x407a50
	BOOL				InitSlider(CSliderCtrl* pSlider, const char* szVar);			// 0x408700

	// The game's strings (CRes.dll)
	CString				LoadResString(UINT nStringID);									// 0x407a70

	// Lists the wizard's pages fill
	BOOL				FillWorldList(CStringList* pList, const char* szDir, BOOL bFullPath);	// 0x406e30
	BOOL				FillSelectedWorldList(CListCtrl* pList);						// 0x406fb0
	BOOL				FillWeaponList(CComboBox* pComboBox);							// 0x407b30

	ServerInterface*	GetServerMgr() { return m_pServerMgr; }


	// Member variables (offsets in the retail object). The block between
	// 0x204 and 0x1b84 is not decoded yet.
public:
	DWORD				m_dw168[6];				// 0x168
	DWORD				m_dw180;				// 0x180
	DWORD				m_dw184;				// 0x184
	DWORD				m_dw188[4];				// 0x188
	DWORD				m_dw198;				// 0x198
	DWORD				m_dw19c;				// 0x19c
	int					m_nCurLevel;			// 0x1a0
	CStringList			m_lstShellMsgs;			// 0x1a4
	DWORD				m_dw1c0[4];				// 0x1c0
	CMapWordToPtr		m_mapPlayerInfo;		// 0x1d0
	CString				m_s1ec;					// 0x1ec
	BOOL				m_b1f0;					// 0x1f0
	ServerInterface*	m_pServerMgr;			// 0x1f4
	CGameServerAppHandler m_AppHandler;			// 0x1f8
	HINSTANCE			m_hResModule;			// 0x1fc  CRes.dll, for LoadResString
	CString				m_s200;					// 0x200
	BYTE				m_aUndecoded[0x1b84 - 0x204];	// 0x204
	CTypedPtrList<CPtrList, ServerOption*> m_lstServerOptions;	// 0x1b84
	CButeMgr			m_ButeMgr;				// 0x1ba0  NetHost.txt
	HANDLE				m_hStopEvent;			// 0x2c60
};

// The one dialog (0x4597bc), set by its constructor.
extern CServerDlg* g_pDialog;


//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SERVERDLG_H__NOLFSERV__INCLUDED_)
