// SessionDlg.h : the setup wizard's first page, CSessionDlg: the session's
// name, game type, players, port, bandwidth and password
// (SessionDlg.cpp: 0x408d20 - 0x40985f; 0x1d4 bytes).
//

#if !defined(AFX_SESSIONDLG_H__NOLFSERV__INCLUDED_)
#define AFX_SESSIONDLG_H__NOLFSERV__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CSessionDlg dialog

class CSessionDlg : public CPropertyPage
{
	DECLARE_DYNCREATE(CSessionDlg)

// Construction
public:
	CSessionDlg();
	~CSessionDlg();

// Dialog Data
	//{{AFX_DATA(CSessionDlg)
	enum { IDD = IDD_SESSION };
	CEdit		m_edBandwidth;		// 0x08c  IDC_BANDWIDTH_CUSTOM
	CComboBox	m_cbBandwidth;		// 0x0c8  IDC_BANDWIDTH
	CSliderCtrl	m_sldMaxPlayers;	// 0x104  IDC_MAXPLAYERS_SLIDER
	CEdit		m_edPassword;		// 0x140  IDC_PASSWORD
	CComboBox	m_cbGameType;		// 0x17c  IDC_GAMETYPE
	UINT		m_nMaxPlayers;		// 0x1b8  IDC_MAXPLAYERS
	CString		m_sSessionName;		// 0x1bc  IDC_SESSIONNAME
	BOOL		m_bUsePassword;		// 0x1c0  IDC_USEPASSWORD
	CString		m_sPassword;		// 0x1c4  IDC_PASSWORD
	DWORD		m_nPort;			// 0x1c8  IDC_PORT
	UINT		m_nBandwidth;		// 0x1cc  IDC_BANDWIDTH_CUSTOM
	//}}AFX_DATA

	int			m_nUnused;			// 0x1d0  never touched


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CSessionDlg)
	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	BOOL	UpdateControls();
	BOOL	SaveControls();

	// Generated message map functions
	//{{AFX_MSG(CSessionDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnUsePassword();
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnSelchangeBandwidth();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SESSIONDLG_H__NOLFSERV__INCLUDED_)
