// CustomDlg.h : the wizard's "Customization Options" page, CCustomDlg: which
// .rez files of the Custom folder the server loads. The list handling is
// the launcher's (src/engine/launcher/dlgcustom.cpp, compiled in here too).
//

#if !defined(AFX_CUSTOMDLG_H__NOLFSERV__INCLUDED_)
#define AFX_CUSTOMDLG_H__NOLFSERV__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CCustomDlg dialog

class CCustomDlg : public CPropertyPage
{
	DECLARE_DYNCREATE(CCustomDlg)

// Construction
public:
	CCustomDlg();
	~CCustomDlg();

// Dialog Data
	//{{AFX_DATA(CCustomDlg)
	enum { IDD = IDD_CUSTOM };
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CCustomDlg)
	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	virtual LRESULT OnWizardNext();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	BOOL	UpdateControls();
	BOOL	SaveControls();

	// Generated message map functions
	//{{AFX_MSG(CCustomDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnAdd();
	afx_msg void OnRemove();
	afx_msg void OnAlways();
	afx_msg void OnSelchange();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CUSTOMDLG_H__NOLFSERV__INCLUDED_)
