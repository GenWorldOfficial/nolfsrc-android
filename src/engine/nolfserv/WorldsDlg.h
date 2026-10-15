// WorldsDlg.h : the wizard's "Worlds" page: the levels the server cycles through (0x409b40 - 0x40aa0f; 0x234 bytes)
//

#if !defined(AFX_WORLDSDLG_H__NOLFSERV__INCLUDED_)
#define AFX_WORLDSDLG_H__NOLFSERV__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// CWorldsDlg dialog

class CWorldsDlg : public CPropertyPage
{
	DECLARE_DYNCREATE(CWorldsDlg)

// Construction
public:
	CWorldsDlg();
	~CWorldsDlg();

// Dialog Data
	//{{AFX_DATA(CWorldsDlg)
	enum { IDD = IDD_WORLDS };
	CButton	m_btnRemoveAll;		// 0x08c  IDC_REMOVEALL
	CButton	m_btnRemove;		// 0x0c8  IDC_REMOVE
	CButton	m_btnAddAll;		// 0x104  IDC_ADDALL
	CButton	m_btnAdd;			// 0x140  IDC_ADD
	CComboBox	m_cbLevelFilter;	// 0x17c  IDC_LEVELFILTER
	CListCtrl	m_lcSelected;		// 0x1b8  IDC_SELECTEDWORLDS
	CListCtrl	m_lcAvailable;		// 0x1f4  IDC_AVAILABLEWORLDS
	int			m_n230;				// 0x230
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CWorldsDlg)
	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(CWorldsDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WORLDSDLG_H__NOLFSERV__INCLUDED_)
