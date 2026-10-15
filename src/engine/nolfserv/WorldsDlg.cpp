// *********************************************************************** //
//
//	MODULE:		WorldsDlg (.CPP)
//
//	PURPOSE:	The setup wizard's last page: the worlds the server cycles
//				through. The left list is every world of the game type's
//				directory (Worlds\Multi\DeathMatch or ...\AssaultMap); the
//				right list is the selection, at most 50 worlds, written to
//				the settings as NetLevel<n> / NetNumLevels (deathmatch) or
//				NetCALevel<n> / NetCANumLevels (H.A.R.M. vs UNITY).
//
//				Retail NolfServ.exe: 0x409b40 - 0x40a9ef.
//
// *********************************************************************** //

#include "stdafx.h"
#include "WorldsDlg.h"
#include "WizardDlg.h"
#include "ServerDlg.h"


#define MAX_SELECTED_WORLDS		50

// The level filter's choices (its combo box is hidden: always "Retail")
#define FILTER_RETAIL			0
#define FILTER_CUSTOM			1


/////////////////////////////////////////////////////////////////////////////
// CWorldsDlg property page

IMPLEMENT_DYNCREATE(CWorldsDlg, CPropertyPage)

// [0x409bb0]
CWorldsDlg::CWorldsDlg() : CPropertyPage(CWorldsDlg::IDD)
{
	//{{AFX_DATA_INIT(CWorldsDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_psp.dwFlags &= ~PSP_HASHELP;
}

// [0x409ce0]
CWorldsDlg::~CWorldsDlg()
{
}

// [0x409db0]
void CWorldsDlg::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CWorldsDlg)
	DDX_Control(pDX, IDC_REMOVEALL, m_btnRemoveAll);
	DDX_Control(pDX, IDC_REMOVE, m_btnRemove);
	DDX_Control(pDX, IDC_ADDALL, m_btnAddAll);
	DDX_Control(pDX, IDC_ADD, m_btnAdd);
	DDX_Control(pDX, IDC_LEVELFILTER, m_cbLevelFilter);
	DDX_Control(pDX, IDC_SELECTEDWORLDS, m_lcSelected);
	DDX_Control(pDX, IDC_AVAILABLEWORLDS, m_lcAvailable);
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CWorldsDlg, CPropertyPage)
	//{{AFX_MSG_MAP(CWorldsDlg)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_BN_CLICKED(IDC_ADDALL, OnAddAll)
	ON_BN_CLICKED(IDC_REMOVE, OnRemove)
	ON_BN_CLICKED(IDC_REMOVEALL, OnRemoveAll)
	ON_NOTIFY(NM_DBLCLK, IDC_AVAILABLEWORLDS, OnDblclkAvailable)
	ON_NOTIFY(NM_DBLCLK, IDC_SELECTEDWORLDS, OnDblclkSelected)
	ON_CBN_SELCHANGE(IDC_LEVELFILTER, OnSelchangeLevelFilter)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CWorldsDlg message handlers

// [0x409e50]: the selected worlds of the left list, to the right.

void CWorldsDlg::OnAdd()
{
	if (m_lcSelected.GetItemCount() >= MAX_SELECTED_WORLDS)
	{
		m_btnAdd.EnableWindow(FALSE);
		m_btnAddAll.EnableWindow(FALSE);
		return;
	}

	BOOL bChanged = FALSE;

	POSITION pos = m_lcAvailable.GetFirstSelectedItemPosition();
	while (pos)
	{
		int nItem = m_lcAvailable.GetNextSelectedItem(pos);

		int nIndex = m_lcSelected.InsertItem(LVIF_TEXT, m_lcSelected.GetItemCount(), m_lcAvailable.GetItemText(nItem, 0), 0, 0, 0, 0);
		if (nIndex < 0) break;

		m_lcSelected.SetItem(nIndex, 0, LVIF_PARAM, NULL, 0, 0, 0, m_cbLevelFilter.GetItemData(m_cbLevelFilter.GetCurSel()));

		bChanged = TRUE;
	}

	UpdateButtons();

	if (bChanged)
	{
		SetModified(TRUE);
	}
}

// [0x409fd0]: all of them, up to the limit.

void CWorldsDlg::OnAddAll()
{
	BOOL bChanged = FALSE;

	int nItem = m_lcAvailable.GetNextItem(-1, LVNI_ALL);
	while (nItem >= 0)
	{
		if (m_lcSelected.GetItemCount() >= MAX_SELECTED_WORLDS)
		{
			m_btnAdd.EnableWindow(FALSE);
			m_btnAddAll.EnableWindow(FALSE);
			break;
		}

		int nIndex = m_lcSelected.InsertItem(LVIF_TEXT, m_lcSelected.GetItemCount(), m_lcAvailable.GetItemText(nItem, 0), 0, 0, 0, 0);
		if (nIndex < 0) break;

		m_lcSelected.SetItem(nIndex, 0, LVIF_PARAM, NULL, 0, 0, 0, m_cbLevelFilter.GetItemData(m_cbLevelFilter.GetCurSel()));

		nItem = m_lcAvailable.GetNextItem(nItem, LVNI_ALL);
		bChanged = TRUE;
	}

	UpdateButtons();

	if (bChanged)
	{
		SetModified(TRUE);
	}
}

// [0x40a150]: the selected worlds of the right list go.

void CWorldsDlg::OnRemove()
{
	int aItems[1024];

	POSITION pos = m_lcSelected.GetFirstSelectedItemPosition();
	int nItems = 0;
	while (pos && nItems < 1024)
	{
		int nItem = m_lcSelected.GetNextSelectedItem(pos);
		if (nItem >= 0)
		{
			aItems[nItems++] = nItem;
		}
	}

	if (nItems)
	{
		SetModified(TRUE);
	}

	while (nItems--)
	{
		m_lcSelected.DeleteItem(aItems[nItems]);
	}

	if (m_lcSelected.GetItemCount() <= MAX_SELECTED_WORLDS)
	{
		m_btnAdd.EnableWindow(TRUE);
		m_btnAddAll.EnableWindow(TRUE);
	}

	UpdateButtons();
}

// [0x40a240]

void CWorldsDlg::OnRemoveAll()
{
	if (m_lcSelected.GetItemCount())
	{
		SetModified(TRUE);
	}

	m_lcSelected.DeleteAllItems();

	m_btnAdd.EnableWindow(TRUE);
	m_btnAddAll.EnableWindow(TRUE);

	UpdateButtons();
}

// [0x40a2a0]: the page from the settings: the game type picks the worlds'
// directory.

BOOL CWorldsDlg::UpdateControls()
{
	switch ((int)g_pDialog->GetSettingFloat("NetGameType", 2.0f))
	{
		case COOPERATIVE_ASSAULT:
			m_sWorldDir = "Worlds\\Multi\\AssaultMap";
			break;

		case DEATHMATCH:
			m_sWorldDir = "Worlds\\Multi\\DeathMatch";
			break;
	}

	FillAvailableList();
	g_pDialog->FillSelectedWorldList(&m_lcSelected);

	return UpdateData(FALSE);
}

// [0x40a300]: the left list, from the server's file list.

BOOL CWorldsDlg::FillAvailableList()
{
	CStringList lstWorlds;
	CString sWorld;

	m_lcAvailable.DeleteAllItems();

	switch (m_cbLevelFilter.GetItemData(m_cbLevelFilter.GetCurSel()))
	{
		case FILTER_RETAIL:
			g_pDialog->FillWorldList(&lstWorlds, m_sWorldDir, FALSE);
			break;

		case FILTER_CUSTOM:
			g_pDialog->FillWorldList(&lstWorlds, "", TRUE);
			break;
	}

	POSITION pos = lstWorlds.GetHeadPosition();
	while (pos)
	{
		sWorld = lstWorlds.GetNext(pos);
		m_lcAvailable.InsertItem(LVIF_TEXT, m_lcAvailable.GetItemCount(), sWorld, 0, 0, 0, 0);
	}

	return TRUE;
}

// [0x40a430]: and the settings from the page: the count, and each world
// with its directory.

void CWorldsDlg::SaveControls()
{
	CString sFormat;
	CString sVar;
	CString sWorld;
	CString sPath;

	int nWorlds = m_lcSelected.GetItemCount();

	switch ((int)g_pDialog->GetSettingFloat("NetGameType", 2.0f))
	{
		case COOPERATIVE_ASSAULT:
			sFormat = "NetCALevel%i";
			g_pDialog->SetSettingFloat("NetCANumLevels", (float)nWorlds);
			break;

		case DEATHMATCH:
			sFormat = "NetLevel%i";
			g_pDialog->SetSettingFloat("NetNumLevels", (float)nWorlds);
			break;
	}

	for (int i = 0; i < nWorlds; i++)
	{
		sVar.Format(sFormat, i);
		sWorld = m_lcSelected.GetItemText(i, 0);

		switch (m_lcSelected.GetItemData(i))
		{
			case FILTER_RETAIL:
				sPath.Format("%s\\%s", m_sWorldDir, sWorld);
				break;

			case FILTER_CUSTOM:
				sPath = sWorld;
				break;
		}

		if (!sPath.IsEmpty())
		{
			g_pDialog->SetSettingString(sVar, sPath);
		}
	}
}

// [0x40a5f0]

BOOL CWorldsDlg::OnInitDialog()
{
	CString sText;

	CPropertyPage::OnInitDialog();

	sText.LoadString(IDS_COLUMNHEADER_WORLD);
	m_lcAvailable.InsertColumn(0, sText);
	m_lcAvailable.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);
	m_lcSelected.InsertColumn(0, sText);
	m_lcSelected.SetColumnWidth(0, LVSCW_AUTOSIZE_USEHEADER);

	sText.LoadString(IDS_FILTER_RETAIL);
	int nIndex = m_cbLevelFilter.AddString(sText);
	if (nIndex != CB_ERR)
	{
		m_cbLevelFilter.SetItemData(nIndex, FILTER_RETAIL);
	}

	sText.LoadString(IDS_FILTER_CUSTOM);
	nIndex = m_cbLevelFilter.AddString(sText);
	if (nIndex != CB_ERR)
	{
		m_cbLevelFilter.SetItemData(nIndex, FILTER_CUSTOM);
	}

	m_cbLevelFilter.SetCurSel(0);

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// [0x40a740]

BOOL CWorldsDlg::OnSetActive()
{
	UpdateControls();
	UpdateButtons();

	return CPropertyPage::OnSetActive();
}

// [0x40a760]

BOOL CWorldsDlg::OnKillActive()
{
	SaveControls();

	return CPropertyPage::OnKillActive();
}

// [0x40a780]: the wizard's buttons; no Finish without a world.

void CWorldsDlg::UpdateButtons()
{
	CWizardDlg* pWizard = dynamic_cast<CWizardDlg*>(GetParent());
	if (!pWizard) return;

	pWizard->SetButtons();

	if (m_lcSelected.GetItemCount() == 0)
	{
		pWizard->SetWizardButtons(PSWIZB_BACK);
	}
}

// [0x40a7f0]: a double click adds the world.

void CWorldsDlg::OnDblclkAvailable(NMHDR* pNMHDR, LRESULT* pResult)
{
	BOOL bChanged = FALSE;

	POSITION pos = m_lcAvailable.GetFirstSelectedItemPosition();
	if (pos)
	{
		int nItem = m_lcAvailable.GetNextSelectedItem(pos);

		int nIndex = m_lcSelected.InsertItem(LVIF_TEXT, m_lcSelected.GetItemCount(), m_lcAvailable.GetItemText(nItem, 0), 0, 0, 0, 0);
		if (nIndex >= 0)
		{
			bChanged = TRUE;
		}
	}

	UpdateButtons();

	if (bChanged)
	{
		SetModified(TRUE);
	}

	*pResult = 0;
}

// [0x40a8f0]: and removes it.

void CWorldsDlg::OnDblclkSelected(NMHDR* pNMHDR, LRESULT* pResult)
{
	BOOL bChanged = FALSE;

	POSITION pos = m_lcSelected.GetFirstSelectedItemPosition();
	if (pos)
	{
		int nItem = m_lcSelected.GetNextSelectedItem(pos);
		if (nItem >= 0)
		{
			m_lcSelected.DeleteItem(nItem);
			bChanged = TRUE;
		}
	}

	UpdateButtons();

	if (bChanged)
	{
		SetModified(TRUE);
	}

	*pResult = 0;
}

// [0x40a980]

void CWorldsDlg::OnSelchangeLevelFilter()
{
	FillAvailableList();
}

// [0x40a990]

BOOL CWorldsDlg::OnWizardFinish()
{
	SaveControls();

	return CPropertyPage::OnWizardFinish();
}

// [0x40a9b0]: back to the game type's options page. (COptionsDlg's
// OnWizardNext is the same code, and the linker keeps this one for both.)

LRESULT CWorldsDlg::OnWizardBack()
{
	switch ((int)g_pDialog->GetServerSettingFloat("NetGameType"))
	{
		case COOPERATIVE_ASSAULT:
			return IDD_OPTIONS_ASSAULT;

		case DEATHMATCH:
			return IDD_OPTIONS_DEATHMATCH;
	}

	return CPropertyPage::OnWizardBack();
}
