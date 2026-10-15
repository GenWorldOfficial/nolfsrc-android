// *********************************************************************** //
//
//	MODULE:		OptionsDlg (.CPP)
//
//	PURPOSE:	The setup wizard's "Options" page, for every game type: run
//				speed, respawn, the default weapon, weapons stay, location
//				based damage, audio taunts, fall damage and the armor's
//				health bonus. The values live in the server dialog's
//				settings (NetHost.txt); the sliders' ranges and scales come
//				from Attributes\ServerOptions.txt, the weapons from
//				Attributes\Weapons.txt. Next goes to the game type's own
//				page.
//
//				Retail NolfServ.exe: 0x402ec0 - 0x40382f. Its Next is
//				folded with the worlds page's identical Back (0x40a9b0).
//
// *********************************************************************** //

#include "stdafx.h"
#include "OptionsDlg.h"
#include "WizardDlg.h"
#include "ServerDlg.h"


/////////////////////////////////////////////////////////////////////////////
// COptionsDlg property page

IMPLEMENT_DYNCREATE(COptionsDlg, CPropertyPage)

// [0x402f30]
COptionsDlg::COptionsDlg() : CPropertyPage(COptionsDlg::IDD)
{
	m_psp.dwFlags &= ~PSP_HASHELP;

	//{{AFX_DATA_INIT(COptionsDlg)
	m_bHitLocation = FALSE;
	m_bWeaponsStay = FALSE;
	m_bAudioTaunts = FALSE;
	m_nRespawnScale = 0;
	m_nRunSpeed = 0;
	m_nFallDamage = 0;
	m_nArmorHealth = 0;
	//}}AFX_DATA_INIT
}

// [0x403090]
COptionsDlg::~COptionsDlg()
{
}

// [0x403160]
void COptionsDlg::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsDlg)
	DDX_Control(pDX, IDC_ARMORHEALTH, m_edArmorHealth);
	DDX_Control(pDX, IDC_SLIDER3, m_sldFallDamage);
	DDX_Control(pDX, IDC_SLIDER4, m_sldArmorHealth);
	DDX_Control(pDX, IDC_FALLDAMAGE, m_edFallDamage);
	DDX_Control(pDX, IDC_SLIDER2, m_sldRespawnScale);
	DDX_Control(pDX, IDC_RUNSPEED, m_edRunSpeed);
	DDX_Control(pDX, IDC_SLIDER1, m_sldRunSpeed);
	DDX_Control(pDX, IDC_DEFAULTWEAPON, m_cbDefaultWeapon);
	DDX_Check(pDX, IDC_HITLOCATION, m_bHitLocation);
	DDX_Check(pDX, IDC_WEAPONSSTAY, m_bWeaponsStay);
	DDX_Check(pDX, IDC_AUDIOTAUNTS, m_bAudioTaunts);
	DDX_Text(pDX, IDC_RESPAWNSCALE, m_nRespawnScale);
	DDX_Text(pDX, IDC_RUNSPEED, m_nRunSpeed);
	DDX_Text(pDX, IDC_FALLDAMAGE, m_nFallDamage);
	DDX_Text(pDX, IDC_ARMORHEALTH, m_nArmorHealth);
	//}}AFX_DATA_MAP
}

// [0x403280]: the controls from the dialog's settings, once the server
// is loaded.

BOOL COptionsDlg::UpdateControls()
{
	if (!g_pDialog || !g_pDialog->GetServerMgr()) return FALSE;

	int nWeapon = (int)g_pDialog->GetServerSettingFloat("NetDefaultWeapon");
	for (int i = 0; i < m_cbDefaultWeapon.GetCount(); i++)
	{
		if ((int)m_cbDefaultWeapon.GetItemData(i) == nWeapon)
		{
			m_cbDefaultWeapon.SetCurSel(i);
			break;
		}
	}

	m_bHitLocation = (int)g_pDialog->GetServerSettingFloat("NetHitLocation");
	m_bWeaponsStay = (int)g_pDialog->GetServerSettingFloat("NetWeaponsStay");
	m_bAudioTaunts = (int)g_pDialog->GetServerSettingFloat("NetAudioTaunts");

	if (g_pDialog->InitSlider(&m_sldRunSpeed, "NetRunSpeed"))
	{
		m_nRunSpeed = m_sldRunSpeed.GetPos();
	}

	if (g_pDialog->InitSlider(&m_sldRespawnScale, "NetRespawnScale"))
	{
		m_nRespawnScale = m_sldRespawnScale.GetPos();
	}

	if (g_pDialog->InitSlider(&m_sldFallDamage, "NetFallDamageScale"))
	{
		m_nFallDamage = m_sldFallDamage.GetPos();
	}

	if (g_pDialog->InitSlider(&m_sldArmorHealth, "NetArmorHealthPercent"))
	{
		m_nArmorHealth = m_sldArmorHealth.GetPos();
	}

	return UpdateData(FALSE);
}

// [0x403440]: and back into the settings; the sliders' values in their
// units times the option's scale.

BOOL COptionsDlg::SaveControls()
{
	if (!m_hWnd) return FALSE;
	if (!g_pDialog || !g_pDialog->GetServerMgr()) return FALSE;
	if (!UpdateData(TRUE)) return FALSE;

	g_pDialog->SetSettingFloat("NetDefaultWeapon", (float)m_cbDefaultWeapon.GetItemData(m_cbDefaultWeapon.GetCurSel()));
	g_pDialog->SetSettingFloat("NetHitLocation", (float)m_bHitLocation);
	g_pDialog->SetSettingFloat("NetWeaponsStay", (float)m_bWeaponsStay);
	g_pDialog->SetSettingFloat("NetAudioTaunts", (float)m_bAudioTaunts);

	ServerOption* pOption = g_pDialog->GetServerOption("NetRunSpeed");
	if (pOption)
	{
		g_pDialog->SetSettingFloat("NetRunSpeed", (float)m_nRunSpeed * pOption->fSliderScale);
	}

	pOption = g_pDialog->GetServerOption("NetRespawnScale");
	if (pOption)
	{
		g_pDialog->SetSettingFloat("NetRespawnScale", (float)m_nRespawnScale * pOption->fSliderScale);
	}

	pOption = g_pDialog->GetServerOption("NetFallDamageScale");
	if (pOption)
	{
		g_pDialog->SetSettingFloat("NetFallDamageScale", (float)m_nFallDamage * pOption->fSliderScale);
	}

	pOption = g_pDialog->GetServerOption("NetArmorHealthPercent");
	if (pOption)
	{
		g_pDialog->SetSettingFloat("NetArmorHealthPercent", (float)m_nArmorHealth * pOption->fSliderScale);
	}

	return TRUE;
}


BEGIN_MESSAGE_MAP(COptionsDlg, CPropertyPage)
	//{{AFX_MSG_MAP(COptionsDlg)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_WEAPONSSTAY, OnCheck)
	ON_BN_CLICKED(IDC_HITLOCATION, OnCheck)
	ON_BN_CLICKED(IDC_AUDIOTAUNTS, OnCheck)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionsDlg message handlers

// [0x403610]: "None" (item data 0), then the weapons.

BOOL COptionsDlg::OnInitDialog()
{
	CString sText;

	CPropertyPage::OnInitDialog();

	sText.LoadString(IDS_NONE);
	int nIndex = m_cbDefaultWeapon.AddString(sText);
	if (nIndex != CB_ERR)
	{
		m_cbDefaultWeapon.SetItemData(nIndex, 0);
	}

	g_pDialog->FillWeaponList(&m_cbDefaultWeapon);

	m_cbDefaultWeapon.SetCurSel(0);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// [0x4036d0]

BOOL COptionsDlg::OnSetActive()
{
	CWizardDlg* pWizard = dynamic_cast<CWizardDlg*>(GetParent());
	if (!pWizard) return FALSE;

	if (!UpdateControls()) return FALSE;

	pWizard->SetButtons();

	return CPropertyPage::OnSetActive();
}

// [0x403730]

BOOL COptionsDlg::OnKillActive()
{
	if (!SaveControls()) return FALSE;

	return CPropertyPage::OnKillActive();
}

// [folded into 0x40a9b0, the worlds page's Back]: on to the game type's
// own options.

LRESULT COptionsDlg::OnWizardNext()
{
	switch ((int)g_pDialog->GetServerSettingFloat("NetGameType"))
	{
		case 1: return IDD_OPTIONS_ASSAULT;
		case 2: return IDD_OPTIONS_DEATHMATCH;
	}

	return CPropertyPage::OnWizardNext();
}

// [0x403750]: a slider's value into its edit box.

void COptionsDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar == (CScrollBar*)&m_sldRunSpeed)
	{
		m_nRunSpeed = m_sldRunSpeed.GetPos();
		UpdateData(FALSE);
	}
	else if (pScrollBar == (CScrollBar*)&m_sldRespawnScale)
	{
		m_nRespawnScale = m_sldRespawnScale.GetPos();
		UpdateData(FALSE);
	}
	else if (pScrollBar == (CScrollBar*)&m_sldFallDamage)
	{
		m_nFallDamage = m_sldFallDamage.GetPos();
		UpdateData(FALSE);
	}
	else if (pScrollBar == (CScrollBar*)&m_sldArmorHealth)
	{
		m_nArmorHealth = m_sldArmorHealth.GetPos();
		UpdateData(FALSE);
	}

	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
}

// [0x403820]: the check boxes' state into the members (the team page's
// identical handler is folded into this one).

void COptionsDlg::OnCheck()
{
	UpdateData();
}
