// *********************************************************************** //
//
//	MODULE:		SessionDlg (.CPP)
//
//	PURPOSE:	The setup wizard's first page: the session's name, the game
//				type, the most players, the port, the bandwidth the server
//				sends at, and the password. Each is a setting of the server
//				dialog's (NetHost.txt), read when the page is shown and
//				written back when it is left.
//
//				Retail NolfServ.exe: 0x408d20 - 0x40985f.
//
// *********************************************************************** //

#include "stdafx.h"
#include "SessionDlg.h"
#include "WizardDlg.h"
#include "ServerDlg.h"


// The bandwidths the list offers (IDS_BANDWIDTH_56K .. _T3), in bytes a
// second; the last entry of the list, "Custom", takes the edit box's.
// (0x44b46c)

static char* s_aszBandwidths[] =
{
	"4000",
	"16000",
	"32000",
	"1000000",
	"10000000",
};

#define BANDWIDTH_CUSTOM	5


/////////////////////////////////////////////////////////////////////////////
// CSessionDlg property page

IMPLEMENT_DYNCREATE(CSessionDlg, CPropertyPage)

// [0x408d90]
CSessionDlg::CSessionDlg() : CPropertyPage(CSessionDlg::IDD)
{
	m_psp.dwFlags &= ~PSP_HASHELP;

	//{{AFX_DATA_INIT(CSessionDlg)
	m_nMaxPlayers = 0;
	m_sSessionName = _T("");
	m_bUsePassword = FALSE;
	m_sPassword = _T("");
	m_nPort = 0;
	m_nBandwidth = 0;
	//}}AFX_DATA_INIT
}

// [0x408ed0]
CSessionDlg::~CSessionDlg()
{
}

// [0x408f90]
void CSessionDlg::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CSessionDlg)
	DDX_Control(pDX, IDC_BANDWIDTH_CUSTOM, m_edBandwidth);
	DDX_Control(pDX, IDC_BANDWIDTH, m_cbBandwidth);
	DDX_Control(pDX, IDC_MAXPLAYERS_SLIDER, m_sldMaxPlayers);
	DDX_Control(pDX, IDC_PASSWORD, m_edPassword);
	DDX_Control(pDX, IDC_GAMETYPE, m_cbGameType);
	DDX_Text(pDX, IDC_MAXPLAYERS, m_nMaxPlayers);
	DDV_MinMaxUInt(pDX, m_nMaxPlayers, 1, 16);
	DDX_Text(pDX, IDC_SESSIONNAME, m_sSessionName);
	DDV_MaxChars(pDX, m_sSessionName, 25);
	DDX_Check(pDX, IDC_USEPASSWORD, m_bUsePassword);
	DDX_Text(pDX, IDC_PASSWORD, m_sPassword);
	DDV_MaxChars(pDX, m_sPassword, 16);
	DDX_Text(pDX, IDC_PORT, m_nPort);
	DDV_MinMaxDWord(pDX, m_nPort, 1, 65535);
	DDX_Text(pDX, IDC_BANDWIDTH_CUSTOM, m_nBandwidth);
	DDV_MinMaxUInt(pDX, m_nBandwidth, 100, 10000000);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CSessionDlg, CPropertyPage)
	//{{AFX_MSG_MAP(CSessionDlg)
	ON_BN_CLICKED(IDC_USEPASSWORD, OnUsePassword)
	ON_WM_HSCROLL()
	ON_CBN_SELCHANGE(IDC_BANDWIDTH, OnSelchangeBandwidth)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSessionDlg message handlers

// [0x4090c0]: the page from the settings, once the server is loaded.

BOOL CSessionDlg::UpdateControls()
{
	if (!g_pDialog || !g_pDialog->GetServerMgr()) return FALSE;

	m_sSessionName.LoadString(IDS_GOODVSEVIL);
	m_sSessionName = g_pDialog->GetSettingString("NetSessionName", m_sSessionName);
	if (m_sSessionName.GetLength() > 25)
	{
		m_sSessionName = m_sSessionName.Left(25);
	}

	if (g_pDialog->InitSlider(&m_sldMaxPlayers, "NetMaxPlayers"))
	{
		m_nMaxPlayers = m_sldMaxPlayers.GetPos();
	}

	m_nPort = (DWORD)g_pDialog->GetSettingFloat("NetPort", 27888.0f);

	int nGameType = (int)g_pDialog->GetServerSettingFloat("NetGameType");
	m_cbGameType.SetCurSel(0);
	for (int i = 0; i < m_cbGameType.GetCount(); i++)
	{
		if (nGameType == (int)m_cbGameType.GetItemData(i))
		{
			m_cbGameType.SetCurSel(i);
			break;
		}
	}

	m_bUsePassword = (BOOL)g_pDialog->GetSettingFloat("NetUsePassword", 0.0f);
	m_sPassword = g_pDialog->GetSettingString("NetPassword", "password");
	m_edPassword.EnableWindow(m_bUsePassword);

	return UpdateData(FALSE);
}

// [0x4092f0]: and the settings from the page; the bandwidth goes to the
// server at once, as a console command.

BOOL CSessionDlg::SaveControls()
{
	if (!m_hWnd) return FALSE;

	if (!UpdateData(TRUE)) return FALSE;

	if (m_sSessionName.IsEmpty())
	{
		m_sSessionName.LoadString(IDS_GOODVSEVIL);
	}

	g_pDialog->SetSettingString("NetSessionName", m_sSessionName);
	g_pDialog->SetSettingFloat("NetGameType", (float)m_cbGameType.GetItemData(m_cbGameType.GetCurSel()));
	g_pDialog->SetSettingFloat("NetUsePassword", (float)m_bUsePassword);
	g_pDialog->SetSettingString("NetPassword", m_sPassword);
	g_pDialog->SetSettingFloat("NetMaxPlayers", (float)m_nMaxPlayers);
	g_pDialog->SetSettingFloat("NetPort", (float)m_nPort);
	g_pDialog->SetSettingFloat("SendBandwidth", (float)m_nBandwidth);

	char szCommand[1024];
	sprintf(szCommand, "SendBandwidth %d", m_nBandwidth);
	g_pDialog->GetServerMgr()->RunConsoleString(szCommand);

	g_pDialog->SetSettingFloat("SendBandwidthSetting", (float)m_cbBandwidth.GetCurSel());

	return TRUE;
}

// [0x4094b0]

BOOL CSessionDlg::OnInitDialog()
{
	CString sText;

	CPropertyPage::OnInitDialog();

	sText.LoadString(IDS_GAMETYPE_DEATHMATCH);
	int nIndex = m_cbGameType.AddString(sText);
	if (nIndex != CB_ERR)
	{
		m_cbGameType.SetItemData(nIndex, DEATHMATCH);
	}

	sText.LoadString(IDS_GAMETYPE_ASSAULT);
	nIndex = m_cbGameType.AddString(sText);
	if (nIndex != CB_ERR)
	{
		m_cbGameType.SetItemData(nIndex, COOPERATIVE_ASSAULT);
	}

	CString sBandwidth;
	sBandwidth.LoadString(IDS_BANDWIDTH_56K);
	m_cbBandwidth.AddString(sBandwidth);
	sBandwidth.LoadString(IDS_BANDWIDTH_CABLE);
	m_cbBandwidth.AddString(sBandwidth);
	sBandwidth.LoadString(IDS_BANDWIDTH_DSL);
	m_cbBandwidth.AddString(sBandwidth);
	sBandwidth.LoadString(IDS_BANDWIDTH_T1);
	m_cbBandwidth.AddString(sBandwidth);
	sBandwidth.LoadString(IDS_BANDWIDTH_T3);
	m_cbBandwidth.AddString(sBandwidth);
	sBandwidth.LoadString(IDS_FILTER_CUSTOM);
	m_cbBandwidth.AddString(sBandwidth);

	int nSetting = (int)g_pDialog->GetSettingFloat("SendBandwidthSetting", 0.0f);
	m_nBandwidth = (UINT)g_pDialog->GetSettingFloat("SendBandwidth", 0.0f);
	m_cbBandwidth.SetCurSel(nSetting);
	OnSelchangeBandwidth();

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// [0x4096c0]

BOOL CSessionDlg::OnSetActive()
{
	CWizardDlg* pWizard = dynamic_cast<CWizardDlg*>(GetParent());
	if (!pWizard) return FALSE;

	if (!UpdateControls()) return FALSE;

	pWizard->SetButtons();

	return CPropertyPage::OnSetActive();
}

// [0x409720]

BOOL CSessionDlg::OnKillActive()
{
	if (!SaveControls()) return FALSE;

	return CPropertyPage::OnKillActive();
}

// [0x409740]

void CSessionDlg::OnUsePassword()
{
	UpdateData(TRUE);
	m_edPassword.EnableWindow(m_bUsePassword);
}

// [0x409760]: the slider moves the number beside it.

void CSessionDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	UpdateData(TRUE);

	if ((CWnd*)pScrollBar == &m_sldMaxPlayers)
	{
		m_nMaxPlayers = m_sldMaxPlayers.GetPos();
		UpdateData(FALSE);
	}

	CPropertyPage::OnHScroll(nSBCode, nPos, pScrollBar);
}

// [0x4097c0]: a listed bandwidth fills in the edit box, read-only;
// "Custom" lets it be typed.

void CSessionDlg::OnSelchangeBandwidth()
{
	if (m_cbBandwidth.GetCurSel() != BANDWIDTH_CUSTOM)
	{
		m_edBandwidth.SetReadOnly(TRUE);
		m_edBandwidth.SetWindowText(s_aszBandwidths[m_cbBandwidth.GetCurSel()]);
		m_nBandwidth = (UINT)atof(s_aszBandwidths[m_cbBandwidth.GetCurSel()]);
	}
	else
	{
		m_edBandwidth.SetReadOnly(FALSE);
	}
}
