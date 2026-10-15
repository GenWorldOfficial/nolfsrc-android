// ServerDlg.h : the server's main dialog, CServerDlg, and the handler the
// server DLL talks back through, CGameServerAppHandler.
//
// Reconstructed from the retail NolfServ.exe (ServerDlg.cpp:
// 0x403990 - 0x408d1f). Member offsets are the retail ones (CServerDlg is
// 0x2c64 bytes; its constructor is at 0x403a30). The layout is the one
// AvP2's AVP2Serv (a descendant of this server, in Monolith's AvP2 source
// release) still has, less its splash screen and image list member.
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
#include <strstrea.h>
#include "NetDefs.h"
#include "ButeMgr.h"


/////////////////////////////////////////////////////////////////////////////
// CPlayerInfo: one player, as the game's server shell reports it
// (SERVERAPP_ADDCLIENT / SHELLUPDATE). 0x2c bytes.

struct CPlayerInfo
{
	CPlayerInfo( )
	{
		m_nClientId = -1;
		m_szPlayerHandle[0] = '\0';
		m_nFrags = 0;
		m_timeOnServer.GetCurrentTime( );	// sic: the result is dropped
	}

	CPlayerInfo( const CPlayerInfo &playerInfo )
	{
		CopyData( playerInfo );
	}

	CPlayerInfo &operator=( const CPlayerInfo &playerInfo )
	{
		CopyData( playerInfo );
		return *this;
	}

	void CopyData( const CPlayerInfo &playerInfo )
	{
		m_nClientId = playerInfo.m_nClientId;
		strcpy( m_szPlayerHandle, playerInfo.m_szPlayerHandle );
		m_nFrags = playerInfo.m_nFrags;
		m_timeOnServer = playerInfo.m_timeOnServer;
	}

	int			m_nClientId;			// 0x00
	char		m_szPlayerHandle[30];	// 0x04
	int			m_nFrags;				// 0x24
	CTime		m_timeOnServer;			// 0x28
};


/////////////////////////////////////////////////////////////////////////////
// CGameServerAppHandler: how the server (server.dll) and the game's server
// shell (Object.lto, through ServerDE::SendToServerApp) talk to the dialog.
// Vtable 0x43d0e8.

class CGameServerAppHandler : public ServerAppHandler
{
public:
	LTRESULT ShellMessageFn(char* pInfo, uint32 nLen);
	LTRESULT ConsoleOutputFn(char* pMsg);
	LTRESULT OutOfMemory();
	LTRESULT ProcessPacket(char* pData, uint32 dataLen, uint8 senderAddr[4], uint16 senderPort);
};


/////////////////////////////////////////////////////////////////////////////
// CServerDlg dialog

class CServerDlg : public CDialog
{
// Construction
public:
	CServerDlg(CWnd* pParent = NULL);	// standard constructor
	~CServerDlg();

// Dialog Data
	//{{AFX_DATA(CServerDlg)
	enum { IDD = IDD_GAMESERVER };
	CButton		m_PlayerBoot;			// 0x05c
	CListCtrl	m_Players;				// 0x098
	CListCtrl	m_Levels;				// 0x0d4
	CEdit		m_edConsole;			// 0x110
	CString		m_sServerName;			// 0x14c
	CString		m_sServerTime;			// 0x150
	CString		m_sTimeInLevel;			// 0x154
	DWORD		m_nPeakPlayers;			// 0x158
	DWORD		m_nTotalPlayers;		// 0x15c
	DWORD		m_nAveragePing;			// 0x160
	DWORD		m_nGamePlayers;			// 0x164
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CServerDlg)
	public:
	virtual BOOL DestroyWindow();
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
	virtual void OnCancel();
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnDblclkLevels(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnStopserver();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()


	// Member functions...

public:

	BOOL				IsLoaded() { return( m_pServerMgr != NULL ); }
	BOOL				IsRunning() { return( m_pServerMgr != NULL && m_hThread != NULL ); }

	void				WriteConsoleString(LPCTSTR pMsg, ...);
	void				WriteConsoleString(int nStringID);

	CRITICAL_SECTION*	GetCS() {return &m_CS;}

	ServerInterface*	GetServerMgr() { return m_pServerMgr; }

	// Messages handlers from the shell.  Called within the shell's thread.
	void				OnShellMessage(char* pMsg, uint32 nLen);
	void				OnShellConsoleOutput(char* pMsg);
	void				OnShellFatalError(int nStringId);
	void				OnShellProcessNetPacket(char* sData, DWORD nLen, BYTE senderAddr[4], DWORD senderPort);

	// The settings: NetHost.txt's "Host" tag, kept in m_ButeMgr. Used by
	// the wizard's pages as well as the dialog.
	CString				GetSettingString(const char* szVar, const char* szDefault);
	float				GetSettingFloat(const char* szVar, float fDefault);
	void				SetSettingString(const char* szVar, const char* szValue);
	void				SetSettingFloat(const char* szVar, float fValue);

	// The lists the wizard's pages fill
	BOOL				FillWorldList(CStringList* pList, const char* szDir, BOOL bFullPath);
	BOOL				FillSelectedWorldList(CListCtrl* pList);

	// The server options (Attributes\ServerOptions.txt, m_lstServerOptions)
	ServerOption*		GetServerOption(const char* szVar);
	float				GetServerOptionDefault(const char* szVar);
	float				GetServerSettingFloat(const char* szVar);

	// The game's strings (CRes.dll)
	CString				LoadResString(UINT nStringID);

	BOOL				FillWeaponList(CComboBox* pComboBox);
	BOOL				InitSlider(CSliderCtrl* pSlider, const char* szVar);

private:

	static void			ThreadUpdate( void *pParam );

	// Update function called by thread.
	void				ServerUpdate();

	// Message handlers from the shell.  Called within the shell's thread.
	void				OnShellAddClient( istrstream &msg );
	void				OnShellRemoveClient( istrstream &msg );
	void				OnShellUpdate( istrstream &msg );
	void				OnShellPreLoadWorld( istrstream &msg );
	void				OnShellPostLoadWorld( istrstream &msg );

	BOOL				SetService( );
	BOOL				DoHost( );
	BOOL				ConfirmStop( );
	void				UpdateUI();

	BOOL				AddResources( );
	BOOL				LoadResourceModule( );
	BOOL				LoadServerOptions( );
	BOOL				LoadServer( );
	BOOL				RunServer( );
	BOOL				StopServer( );
	BOOL				SelectLevel( int nLevelIndex );

	CString				FormatTime( CTimeSpan &timeSpan );

	// Parses the command line's -nowiz and -Net<setting> <value> options.
	BOOL				ParseCommandLine( BOOL* pbWizard );


	// Member variables...

private:
	CRITICAL_SECTION	m_CS;					// 0x168  So we don't switch levels and update at the same time.
	BOOL				m_bCSInitted;			// 0x180

	BOOL				m_bFirstShow;			// 0x184

	CTime				m_serverStartTime;		// 0x188
	CTimeSpan			m_serverRunTime;		// 0x18c
	CTime				m_levelStartTime;		// 0x190
	CTimeSpan			m_levelRunTime;			// 0x194
	HANDLE				m_hThread;				// 0x198

	int					m_nCurLevel;			// 0x19c

	// These parameters are directly updated by the server thread.
	int					m_nCurLevel_Shell;		// 0x1a0
	CStringList			m_lstShellMsgs_Shell;	// 0x1a4
	BOOL				m_bStopServer_Shell;	// 0x1c0
	BOOL				m_bPlayerUpdate_Shell;	// 0x1c4
	BOOL				m_bChangingLevels_Shell;	// 0x1c8
	BOOL				m_bChangedLevel_Shell;	// 0x1cc
	CMapWordToPtr		m_mapPlayerInfo_Shell;	// 0x1d0

	CString				m_sFullTcpIpAddress;	// 0x1ec
	BOOL				m_bConfirmExit;			// 0x1f0

	ServerInterface *	m_pServerMgr;			// 0x1f4
	CGameServerAppHandler m_AppHandler;			// 0x1f8

	HINSTANCE			m_hResModule;			// 0x1fc  a temporary copy of CRes.dll
	CString				m_sResModuleFile;		// 0x200  ... and its file name

	NetGame				m_NetGame;				// 0x204  never used

	CTypedPtrList<CPtrList, ServerOption*> m_lstServerOptions;	// 0x1b84
	CButeMgr			m_ButeMgr;				// 0x1ba0  NetHost.txt

	// Event which signals that the server should stop
	HANDLE				m_hStopEvent;			// 0x2c60
};

extern CServerDlg *g_pDialog;


//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SERVERDLG_H__NOLFSERV__INCLUDED_)
