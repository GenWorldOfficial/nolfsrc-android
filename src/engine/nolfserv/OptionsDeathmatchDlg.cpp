// *********************************************************************** //
//
//	MODULE:		OptionsDeathmatchDlg (.CPP)
//
//	PURPOSE:	The setup wizard's "Deathmatch Options" page: how a level
//				of deathmatch ends (a frag limit, a time limit, either or
//				both). The values live in the server dialog's settings
//				(NetHost.txt); the sliders' ranges and scales come from
//				Attributes\ServerOptions.txt.
//
//				Retail NolfServ.exe: 0x402780 - 0x402ebf.
//
// *********************************************************************** //

#include "stdafx.h"
#include "OptionsDeathmatchDlg.h"
#include "WizardDlg.h"
#include "ServerDlg.h"


/////////////////////////////////////////////////////////////////////////////
// COptionsDeathmatchDlg property page

IMPLEMENT_DYNCREATE(COptionsDeathmatchDlg, CPropertyPage)

// [0x4027f0]
COptionsDeathmatchDlg::COptionsDeathmatchDlg() : CPropertyPage(COptionsDeathmatchDlg::IDD)
{
	//{{AFX_DATA_INIT(COptionsDeathmatchDlg)
	m_nFragLimit = 0;
	m_nTimeLimit = 0;
	//}}AFX_DATA_INIT

	m_psp.dwFlags &= ~PSP_HASHELP;
}

// [0x4028f0]
COptionsDeathmatchDlg::~COptionsDeathmatchDlg()
{
}

// [0x402990]
void COptionsDeathmatchDlg::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COptionsDeathmatchDlg)
	DDX_Control(pDX, IDC_SLIDER2, m_sldTimeLimit);
	DDX_Control(pDX, IDC_SLIDER4, m_sldFragLimit);
	DDX_Control(pDX, IDC_FRAGLIMIT, m_edFragLimit);
	DDX_Control(pDX, IDC_TIMELIMIT, m_edTimeLimit);
	DDX_Control(pDX, IDC_ENDCONDITION, m_cbEndCondition);
	DDX_Text(pDX, IDC_FRAGLIMIT, m_nFragLimit);
	DDX_Text(pDX, IDC_TIMELIMIT, m_nTimeLimit);
	//}}AFX_DATA_MAP
}

// [0x402a20]: the controls from the dialog's settings, once the server
// is loaded.

BOOL COptionsDeathmatchDlg::UpdateControls()
{
	if (!g_pDialog || !g_pDialog->GetServerMgr()) return FALSE;

	int nGameEnd = (int)g_pDialog->GetServerSettingFloat("NetDMGameEnd");
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

	if (g_pDialog->InitSlider(&m_sldFragLimit, "NetEndFrags"))
	{
		m_nFragLimit = m_sldFragLimit.GetPos();
	}

	return UpdateData(FALSE);
}

// [0x402b30]: and back into the settings, in the slider's units times the
// option's scale.

BOOL COptionsDeathmatchDlg::SaveControls()
{
	if (!m_hWnd) return FALSE;
	if (!g_pDialog || !g_pDialog->GetServerMgr()) return FALSE;
	if (!UpdateData(TRUE)) return FALSE;

	int nGameEnd = m_cbEndCondition.GetItemData(m_cbEndCondition.GetCurSel());
	g_pDialog->SetSettingFloat("NetDMGameEnd", (float)nGameEnd);

	ServerOption* pOption = g_pDialog->GetServerOption("NetEndFrags");
	if (pOption)
	{
		g_pDialog->SetSettingFloat("NetEndFrags", (float)m_nFragLimit * pOption->fSliderScale);
	}

	pOption = g_pDialog->GetServerOption("NetEndTime");
	if (pOption)
	{
		g_pDialog->SetSettingFloat("NetEndTime", (float)m_nTimeLimit * pOption->fSliderScale);
	}

	return TRUE;
}


BEGIN_MESSAGE_MAP(COptionsDeathmatchDlg, CPropertyPage)
	//{{AFX_MSG_MAP(COptionsDeathmatchDlg)
	ON_WM_HSCROLL()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COptionsDeathmatchDlg message handlers

// [0x402c60]: the level-ending conditions are the option's strings (in
// CRes.dll), each with its index; the option's default is selected.

BOOL COptionsDeathmatchDlg::OnInitDialog()
{
	CString sText;

	CPropertyPage::OnInitDialog();

	ServerOption* pOption = g_pDialog->GetServerOption("NetDMGameEnd");
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

// [0x402d90]

BOOL COptionsDeathmatchDlg::OnSetActive()
{
	CWizardDlg* pWizard = dynamic_cast<CWizardDlg*>(GetParent());
	if (!pWizard) return FALSE;

	if (!UpdateControls()) return FALSE;

	pWizard->SetButtons();

	return CPropertyPage::OnSetActive();
}

// [0x402df0]

BOOL COptionsDeathmatchDlg::OnKillActive()
{
	if (!SaveControls()) return FALSE;

	return CPropertyPage::OnKillActive();
}

// [folded into 0x402690, the team page's]: back to the options for every
// game type...

LRESULT COptionsDeathmatchDlg::OnWizardBack()
{
	return IDD_OPTIONS;
}

// [0x402e10]: ... and on to the worlds.

LRESULT COptionsDeathmatchDlg::OnWizardNext()
{
	return IDD_WORLDS;
}

// [0x402e20]: a slider's value into its edit box.

void COptionsDeathmatchDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (pScrollBar == (CScrollBar*)&m_sldTimeLimit)
	{
		m_nTimeLimit = m_sldTimeLimit.GetPos();
		UpdateData(FALSE);
	}
	else if (pScrollBar == (CScrollBar*)&m_sldFragLimit)
	{
		m_nFragLimit = m_sldFragLimit.GetPos();
		UpdateData(FALSE);
	}

	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
}
