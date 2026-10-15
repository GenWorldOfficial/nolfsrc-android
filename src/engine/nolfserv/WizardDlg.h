// WizardDlg.h : the server setup wizard, CWizardDlg: a property sheet
// holding the six pages (WizardDlg.cpp: 0x409860 - 0x409b9f).
//

#if !defined(AFX_WIZARDDLG_H__NOLFSERV__INCLUDED_)
#define AFX_WIZARDDLG_H__NOLFSERV__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "SessionDlg.h"
#include "CustomDlg.h"
#include "OptionsDlg.h"
#include "OptionsDeathmatchDlg.h"
#include "OptionsAssaultDlg.h"
#include "WorldsDlg.h"

/////////////////////////////////////////////////////////////////////////////
// CWizardDlg

class CWizardDlg : public CPropertySheet
{
	DECLARE_DYNAMIC(CWizardDlg)

// Construction
public:
	CWizardDlg();						// 0x409870

// Attributes
public:
	CSessionDlg				m_SessionDlg;		// 0x088
	CCustomDlg				m_CustomDlg;		// 0x25c
	COptionsDlg				m_OptionsDlg;		// 0x2e8
	COptionsDeathmatchDlg	m_DeathmatchDlg;	// 0x570
	COptionsAssaultDlg		m_AssaultDlg;		// 0x730
	CWorldsDlg				m_WorldsDlg;		// 0x8c0

// Operations
public:
	void	SetButtons();				// 0x409ab0: Back / Next / Finish for the page shown

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CWizardDlg)
	public:
	virtual BOOL OnInitDialog();
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CWizardDlg();				// 0x409a00

	// Generated message map functions
protected:
	//{{AFX_MSG(CWizardDlg)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

// Whether the Custom folder has any .rez file (0x4597c4): the constructor
// adds the customization page only then.
extern bool g_bCustomRez;

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WIZARDDLG_H__NOLFSERV__INCLUDED_)
