// *********************************************************************** //
//
//	MODULE:		OptionsAssaultDlg (.CPP)
//
//	PURPOSE:	The setup wizard's "H.A.R.M. vs Unity Options" page: how a
//				team game ends (time, winning score, the score for the
//				intelligence), friendly fire and frag scoring. The values
//				live in the server dialog's settings (NetHost.txt); the
//				sliders' ranges and scales come from
//				Attributes\ServerOptions.txt.
//
//				Retail NolfServ.exe: 0x401f40 - 0x40277f. The check
//				boxes' handler is folded with the options page's
//				identical one (0x403820).
//
// *********************************************************************** //

#include "stdafx.h"
#include "OptionsAssaultDlg.h"
#include "WizardDlg.h"
#include "ServerDlg.h"


/////////////////////////////////////////////////////////////////////////////
// COptionsAssaultDlg property page

IMPLEMENT_DYNCREATE(COptionsAssaultDlg, CPropertyPage)

// [0x401fb0]
COptionsAssaultDlg::COptionsAssaultDlg() : CPropertyPage(COptionsAssaultDlg::IDD)
{
	//{{AFX_DATA_INIT(COptionsAssaultDlg)
	m_bFriendlyFire = FALSE;
	m_nIntelScore = 0;
	m_nTimeLimit = 0;
	m_nWinningScore = 0;
	m_bFragScoring = FALSE;
	//}}AFX_DATA_INIT

	m_psp.dwFlags &= ~PSP_HASHELP;
}

// [0x4020a0]
COptionsAssaultDlg::~COptionsAssaultDlg()
{
}

// [0x402130]
void COptionsAssaultDlg::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsAssaultDlg)
	DDX_Control(pDX, IDC_SLIDER3, m_sldWinningScore);
	DDX_Control(pDX, IDC_SLIDER2, m_sldTimeLimit);
	DDX_Control(pDX, IDC_SLIDER4, m_sldIntelScore);
	DDX_Control(pDX, IDC_ENDCONDITION, m_cbEndCondition);
	DDX_Check(pDX, IDC_FRIENDLYFIRE, m_bFriendlyFire);
	DDX_Text(pDX, IDC_INTELSCORE, m_nIntelScore);
	DDX_Text(pDX, IDC_TIMELIMIT, m_nTimeLimit);
	DDX_Text(pDX, IDC_WINNINGSCORE, m_nWinningScore);
	DDX_Check(pDX, IDC_FRAGSCORING, m_bFragScoring);
	//}}AFX_DATA_MAP
}

// [0x4021e0]: the controls from the dialog's settings, once the server
// is loaded.

BOOL COptionsAssaultDlg::UpdateControls()
{
	if (!g_pDialog || !g_pDialog->GetServerMgr()) return FALSE;

	int nGameEnd = (int)g_pDialog->GetServerSettingFloat("NetCAGameEnd");
	for (int i = 0; i < m_cbEndCondition.GetCount(); i++)
	{
		if ((int)m_cbEndCondition.GetItemData(i) == nGameEnd)
		{
			m_cbEndCondition.SetCurSel(i);
			break;
		}
	}

	if (g_pDialog->InitSlider(&m_sldTimeLimit, "NetEndTime"))
	{
		m_nTimeLimit = m_sldTimeLimit.GetPos();
	}

	if (g_pDialog->InitSlider(&m_sldWinningScore, "NetEndScore"))
	{
		m_nWinningScore = m_sldWinningScore.GetPos();
	}

	if (g_pDialog->InitSlider(&m_sldIntelScore, "NetIntelScore"))
	{
		m_nIntelScore = m_sldIntelScore.GetPos();
	}

	m_bFragScoring = (int)g_pDialog->GetServerSettingFloat("NetFragScore");
	m_bFriendlyFire = (int)g_pDialog->GetServerSettingFloat("NetFriendlyFire");

	return UpdateData(FALSE);
}

// [0x402350]: and back into the settings; the scores in the slider's
// units, times the option's scale.

BOOL COptionsAssaultDlg::SaveControls()
{
	if (!m_hWnd) return FALSE;
	if (!g_pDialog || !g_pDialog->GetServerMgr()) return FALSE;
	if (!UpdateData(TRUE)) return FALSE;

	int nGameEnd = m_cbEndCondition.GetItemData(m_cbEndCondition.GetCurSel());
	g_pDialog->SetSettingFloat("NetCAGameEnd", (float)nGameEnd);

	g_pDialog->SetSettingFloat("NetEndTime", (float)m_nTimeLimit);

	ServerOption* pOption = g_pDialog->GetServerOption("NetIntelScore");
	if (pOption)
	{
		g_pDialog->SetSettingFloat("NetIntelScore", (float)m_nIntelScore * pOption->fSliderScale);
	}

	pOption = g_pDialog->GetServerOption("NetEndScore");
	if (pOption)
	{
		g_pDialog->SetSettingFloat("NetEndScore", (float)m_nWinningScore * pOption->fSliderScale);
	}

	g_pDialog->SetSettingFloat("NetFragScore", (float)m_bFragScoring);
	g_pDialog->SetSettingFloat("NetFriendlyFire", (float)m_bFriendlyFire);

	return TRUE;
}


BEGIN_MESSAGE_MAP(COptionsAssaultDlg, CPropertyPage)
	//{{AFX_MSG_MAP(COptionsAssaultDlg)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_FRIENDLYFIRE, OnCheck)
	ON_BN_CLICKED(IDC_FRAGSCORING, OnCheck)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionsAssaultDlg message handlers

// [0x4024e0]: the level-ending conditions are the option's strings (in
// CRes.dll), each with its index; the option's default is selected.

BOOL COptionsAssaultDlg::OnInitDialog()
{
	CString sText;

	CPropertyPage::OnInitDialog();

	ServerOption* pOption = g_pDialog->GetServerOption("NetCAGameEnd");
	if (pOption)
	{
		for (int i = 0; i < pOption->nNumStrings; i++)
		{
			sText = g_pDialog->LoadResString(pOption->nStringId[i]);

			int nIndex = m_cbEndCondition.AddString(sText);
			if (nIndex != CB_ERR)
			{
				m_cbEndCondition.SetItemData(nIndex, i);
				if (i == (int)pOption->fDefault)
				{
					m_cbEndCondition.SetCurSel(nIndex);
				}
			}
		}
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// [0x402610]

BOOL COptionsAssaultDlg::OnSetActive()
{
	CWizardDlg* pWizard = dynamic_cast<CWizardDlg*>(GetParent());
	if (!pWizard) return FALSE;

	if (!UpdateControls()) return FALSE;

	pWizard->SetButtons();

	return CPropertyPage::OnSetActive();
}

// [0x402670]

BOOL COptionsAssaultDlg::OnKillActive()
{
	if (!SaveControls()) return FALSE;

	return CPropertyPage::OnKillActive();
}

// [0x402690]: back to the options for every game type...

LRESULT COptionsAssaultDlg::OnWizardBack()
{
	return IDD_OPTIONS;
}

// [folded into 0x402e10, the deathmatch page's]: ... and on to the worlds.

LRESULT COptionsAssaultDlg::OnWizardNext()
{
	return IDD_WORLDS;
}

// [0x4026a0]: a slider's value into its edit box.

void COptionsAssaultDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar == (CScrollBar*)&m_sldTimeLimit)
	{
		m_nTimeLimit = m_sldTimeLimit.GetPos();
		UpdateData(FALSE);
	}
	else if (pScrollBar == (CScrollBar*)&m_sldWinningScore)
	{
		m_nWinningScore = m_sldWinningScore.GetPos();
		UpdateData(FALSE);
	}
	else if (pScrollBar == (CScrollBar*)&m_sldIntelScore)
	{
		m_nIntelScore = m_sldIntelScore.GetPos();
		UpdateData(FALSE);
	}

	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
}

// [folded into 0x403820, the options page's]

void COptionsAssaultDlg::OnCheck()
{
	UpdateData();
}
