// OptionsDlg.h : the wizard's "Options" page, for every game type (0x402ec0 - 0x40381f; 0x288 bytes)
//

#if !defined(AFX_OPTIONSDLG_H__NOLFSERV__INCLUDED_)
#define AFX_OPTIONSDLG_H__NOLFSERV__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "resource.h"

/////////////////////////////////////////////////////////////////////////////
// COptionsDlg dialog

class COptionsDlg : public CPropertyPage
{
	DECLARE_DYNCREATE(COptionsDlg)

// Construction
public:
	COptionsDlg();
	~COptionsDlg();

// Dialog Data
	//{{AFX_DATA(COptionsDlg)
	enum { IDD = IDD_OPTIONS };
	CEdit		m_edArmorHealth;	// 0x08c  IDC_ARMORHEALTH
	CSliderCtrl	m_sldFallDamage;	// 0x0c8  IDC_SLIDER3
	CSliderCtrl	m_sldArmorHealth;	// 0x104  IDC_SLIDER4
	CEdit		m_edFallDamage;		// 0x140  IDC_FALLDAMAGE
	CSliderCtrl	m_sldRespawnScale;	// 0x17c  IDC_SLIDER2
	CEdit		m_edRunSpeed;		// 0x1b8  IDC_RUNSPEED
	CSliderCtrl	m_sldRunSpeed;		// 0x1f4  IDC_SLIDER1
	CComboBox	m_cbDefaultWeapon;	// 0x230  IDC_DEFAULTWEAPON
	BOOL		m_bHitLocation;		// 0x26c  IDC_HITLOCATION
	BOOL		m_bWeaponsStay;		// 0x270  IDC_WEAPONSSTAY
	BOOL		m_bAudioTaunts;		// 0x274  IDC_AUDIOTAUNTS
	int			m_nRespawnScale;	// 0x278  IDC_RESPAWNSCALE
	int			m_nRunSpeed;		// 0x27c  IDC_RUNSPEED
	int			m_nFallDamage;		// 0x280  IDC_FALLDAMAGE
	int			m_nArmorHealth;		// 0x284  IDC_ARMORHEALTH
	//}}AFX_DATA


// Overrides
	// ClassWizard generate virtual function overrides
	//{{AFX_VIRTUAL(COptionsDlg)
	public:
	virtual BOOL OnSetActive();
	virtual BOOL OnKillActive();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	// Generated message map functions
	//{{AFX_MSG(COptionsDlg)
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_OPTIONSDLG_H__NOLFSERV__INCLUDED_)
