// *********************************************************************** //
//
//	MODULE:		CustomDlg (.CPP)
//
//	PURPOSE:	The setup wizard's "Customization Options" page: which .rez
//				files of the Custom folder the server loads, remembered in
//				the registry. The wizard shows it only when the Custom
//				folder has a .rez file in it (WizardDlg.cpp). Its list
//				handling is the launcher's, src/engine/launcher/
//				dlgcustom.cpp, which NolfServ.exe compiles too
//				(_NOLFSERV); this file owns the globals it works on.
//
//				Retail NolfServ.exe: 0x401000 - 0x40159f.
//
// *********************************************************************** //

#include "stdafx.h"
#include "regmgr.h"
#include "CustomDlg.h"
#include "WizardDlg.h"
#include "ServerDlg.h"


// Globals...  (their initializers: 0x401000, 0x401040)

CRegMgr		g_RegMgr;				// 0x4594d8
CStringList	g_lstRezFiles;			// 0x4594b8
int			g_bSaveRezFiles;		// 0x4594b0


// The launcher's dlgcustom.cpp...

int		FillAvailableRezList(HWND hList);
int		FillSelectedRezList(HWND hList);
BOOL	AddSelectedRez(HWND hAvail, HWND hSel);
BOOL	RemoveSelectedRez(HWND hAvail, HWND hSel);
void	UpdateRezButtons(HWND hWnd);
int		SaveRezList(HWND hList);
void	SaveRezFiles(HWND hWnd);
void	LoadRezFiles();


/////////////////////////////////////////////////////////////////////////////
// CCustomDlg property page

IMPLEMENT_DYNCREATE(CCustomDlg, CPropertyPage)

// [0x401100]
CCustomDlg::CCustomDlg() : CPropertyPage(CCustomDlg::IDD)
{
	//{{AFX_DATA_INIT(CCustomDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_psp.dwFlags &= ~PSP_HASHELP;
}

// [0x401180]
CCustomDlg::~CCustomDlg()
{
}

// [0x401190]
void CCustomDlg::DoDataExchange(CDataExchange* pDX)
{
	CPropertyPage::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CCustomDlg)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CCustomDlg, CPropertyPage)
	//{{AFX_MSG_MAP(CCustomDlg)
	ON_BN_CLICKED(IDC_ADD, OnAdd)
	ON_BN_CLICKED(IDC_REMOVE, OnRemove)
	ON_BN_CLICKED(IDC_CUSTOM_ALWAYS, OnAlways)
	ON_LBN_SELCHANGE(IDC_CUSTOM_SELECTED, OnSelchange)
	ON_LBN_SELCHANGE(IDC_CUSTOM_AVAILABLE, OnSelchange)
	ON_LBN_DBLCLK(IDC_CUSTOM_SELECTED, OnRemove)
	ON_LBN_DBLCLK(IDC_CUSTOM_AVAILABLE, OnAdd)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CCustomDlg message handlers

// [0x4011b0]: the page's controls from the dialog's settings -- here only
// the DDX, and only once the server is loaded.

BOOL CCustomDlg::UpdateControls()
{
	if (g_pDialog && g_pDialog->GetServerMgr())
	{
		return UpdateData(FALSE);
	}

	return FALSE;
}

// [0x4011d0]: and back.

BOOL CCustomDlg::SaveControls()
{
	if (!m_hWnd) return FALSE;

	return UpdateData(TRUE) != 0;
}

// [0x4011f0]

BOOL CCustomDlg::OnKillActive()
{
	if (!SaveControls()) return FALSE;

	return CPropertyPage::OnKillActive();
}

// [0x401210]

BOOL CCustomDlg::OnSetActive()
{
	CWizardDlg* pWizard = dynamic_cast<CWizardDlg*>(GetParent());
	if (!pWizard) return FALSE;

	if (!UpdateControls()) return FALSE;

	pWizard->SetButtons();

	return CPropertyPage::OnSetActive();
}

// [0x401270]

BOOL CCustomDlg::OnInitDialog()
{
	CPropertyPage::OnInitDialog();

	CString sApp;
	sApp.LoadString(IDS_APPREGNAME);

	if (!g_RegMgr.Init("Monolith Productions", sApp, "1.0", NULL, HKEY_LOCAL_MACHINE, NULL))
	{
		return FALSE;
	}

	LoadRezFiles();

	if (FillAvailableRezList(GetDlgItem(IDC_CUSTOM_AVAILABLE)->GetSafeHwnd()) <= 0)
	{
		// No .rez file after all: skip the page.

		CWizardDlg* pWizard = dynamic_cast<CWizardDlg*>(GetParent());
		if (!pWizard) return FALSE;

		pWizard->SetActivePage(2);
	}
	else
	{
		FillSelectedRezList(GetDlgItem(IDC_CUSTOM_SELECTED)->GetSafeHwnd());
		UpdateRezButtons(GetSafeHwnd());
		CheckDlgButton(IDC_CUSTOM_ALWAYS, g_bSaveRezFiles != 0);
	}

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// [0x4013f0]

void CCustomDlg::OnAdd()
{
	AddSelectedRez(GetDlgItem(IDC_CUSTOM_AVAILABLE)->GetSafeHwnd(), GetDlgItem(IDC_CUSTOM_SELECTED)->GetSafeHwnd());
	UpdateRezButtons(GetSafeHwnd());
}

// [0x401450]

void CCustomDlg::OnSelchange()
{
	UpdateRezButtons(GetSafeHwnd());
}

// [0x401470]

void CCustomDlg::OnRemove()
{
	RemoveSelectedRez(GetDlgItem(IDC_CUSTOM_AVAILABLE)->GetSafeHwnd(), GetDlgItem(IDC_CUSTOM_SELECTED)->GetSafeHwnd());
	UpdateRezButtons(GetSafeHwnd());
}

// [0x4014d0]: save the choice, and give the server the files: Custom\<name>
// for each, at most 256.

LRESULT CCustomDlg::OnWizardNext()
{
	SaveRezList(GetDlgItem(IDC_CUSTOM_SELECTED)->GetSafeHwnd());
	SaveRezFiles(GetSafeHwnd());

	char* aRezFiles[256];
	int nRezFiles = 0;

	POSITION pos = g_lstRezFiles.GetHeadPosition();
	while (nRezFiles < 256)
	{
		if (!pos) break;

		aRezFiles[nRezFiles] = new char[256];
		sprintf(aRezFiles[nRezFiles++], "Custom\\%s", (LPCSTR)g_lstRezFiles.GetNext(pos));
	}

	if (nRezFiles > 0)
	{
		g_pDialog->GetServerMgr()->AddResources(aRezFiles, nRezFiles);
	}

	while (nRezFiles--)
	{
		delete [] aRezFiles[nRezFiles];
	}

	return CPropertyPage::OnWizardNext();
}

// [0x40aa00, the image's one empty void function]: the check box is read
// when the page is left.

void CCustomDlg::OnAlways()
{
}
