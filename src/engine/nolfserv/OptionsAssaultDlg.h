// OptionsAssaultDlg.h : the wizard's "H.A.R.M. vs Unity Options" page (0x401f40 - 0x40277f; 0x190 bytes)
//

#if !defined(AFX_OPTIONSASSAULTDLG_H__NOLFSERV__INCLUDED_)
#define AFX_OPTIONSASSAULTDLG_H__NOLFSERV__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// COptionsAssaultDlg dialog

class COptionsAssaultDlg : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionsAssaultDlg)

// Construction
public:
	COptionsAssaultDlg();
	~COptionsAssaultDlg();

// Dialog Data
	//{{AFX_DATA(COptionsAssaultDlg)
	enum { IDD = IDD_OPTIONS_ASSAULT };
	CSliderCtrl	m_sldWinningScore;	// 0x08c  IDC_SLIDER3
	CSliderCtrl	m_sldTimeLimit;		// 0x0c8  IDC_SLIDER2
	CSliderCtrl	m_sldIntelScore;	// 0x104  IDC_SLIDER4
	CComboBox	m_cbEndCondition;	// 0x140  IDC_ENDCONDITION
	BOOL		m_bFriendlyFire;	// 0x17c  IDC_FRIENDLYFIRE
	int			m_nIntelScore;		// 0x180  IDC_INTELSCORE
	int			m_nTimeLimit;		// 0x184  IDC_TIMELIMIT
	int			m_nWinningScore;	// 0x188  IDC_WINNINGSCORE
	BOOL		m_bFragScoring;		// 0x18c  IDC_FRAGSCORING
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionsAssaultDlg)
	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionsAssaultDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONSASSAULTDLG_H__NOLFSERV__INCLUDED_)
