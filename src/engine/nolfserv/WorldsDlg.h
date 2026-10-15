// WorldsDlg.h : the setup wizard's last page, CWorldsDlg: the worlds the
// server cycles through (WorldsDlg.cpp: 0x409b40 - 0x40a9ef; 0x234 bytes).
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
	CButton		m_btnRemoveAll;		// 0x08c  IDC_REMOVEALL
	CButton		m_btnRemove;		// 0x0c8  IDC_REMOVE
	CButton		m_btnAddAll;		// 0x104  IDC_ADDALL
	CButton		m_btnAdd;			// 0x140  IDC_ADD
	CComboBox	m_cbLevelFilter;	// 0x17c  IDC_LEVELFILTER
	CListCtrl	m_lcSelected;		// 0x1b8  IDC_SELECTEDWORLDS
	CListCtrl	m_lcAvailable;		// 0x1f4  IDC_AVAILABLEWORLDS
	//}}AFX_DATA

	CString		m_sWorldDir;		// 0x230  the game type's worlds, Worlds\Multi\...


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(CWorldsDlg)
	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	virtual BOOL OnWizardFinish();
	virtual LRESULT OnWizardBack();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	BOOL	UpdateControls();
	BOOL	FillAvailableList();
	void	SaveControls();
	void	UpdateButtons();

	// Generated message map functions
	//{{AFX_MSG(CWorldsDlg)
	afx_msg void OnAdd();
	afx_msg void OnAddAll();
	afx_msg void OnRemove();
	afx_msg void OnRemoveAll();
	virtual BOOL OnInitDialog();
	afx_msg void OnDblclkAvailable(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDblclkSelected(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnSelchangeLevelFilter();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_WORLDSDLG_H__NOLFSERV__INCLUDED_)
