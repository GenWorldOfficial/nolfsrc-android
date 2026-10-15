// OptionsDeathmatchDlg.h : the wizard's "Deathmatch Options" page (0x402780 - 0x402ebf; 0x1c0 bytes)
//

#if !defined(AFX_OPTIONSDEATHMATCHDLG_H__NOLFSERV__INCLUDED_)
#define AFX_OPTIONSDEATHMATCHDLG_H__NOLFSERV__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// COptionsDeathmatchDlg dialog

class COptionsDeathmatchDlg : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionsDeathmatchDlg)

// Construction
public:
	COptionsDeathmatchDlg();
	~COptionsDeathmatchDlg();

// Dialog Data
	//{{AFX_DATA(COptionsDeathmatchDlg)
	enum { IDD = IDD_OPTIONS_DEATHMATCH };
	CSliderCtrl	m_sldTimeLimit;		// 0x08c  IDC_SLIDER2
	CSliderCtrl	m_sldFragLimit;		// 0x0c8  IDC_SLIDER4
	CEdit		m_edFragLimit;		// 0x104  IDC_FRAGLIMIT
	CEdit		m_edTimeLimit;		// 0x140  IDC_TIMELIMIT
	CComboBox	m_cbEndCondition;	// 0x17c  IDC_ENDCONDITION
	int			m_nFragLimit;		// 0x1b8  IDC_FRAGLIMIT
	int			m_nTimeLimit;		// 0x1bc  IDC_TIMELIMIT
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionsDeathmatchDlg)
	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionsDeathmatchDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONSDEATHMATCHDLG_H__NOLFSERV__INCLUDED_)
