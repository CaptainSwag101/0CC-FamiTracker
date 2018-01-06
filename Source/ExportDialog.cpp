/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2014  Jonathan Liss
**
** 0CC-FamiTracker is (C) 2014-2018 HertzDevil
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.  To obtain a
** copy of the GNU Library General Public License, write to the Free
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
*/

/*
 * This is the NSF (and other types) export dialog
 *
 */

#include "ExportDialog.h"
#include <map>
#include <vector>
#include "FamiTracker.h"
#include "FamitrackerDoc.h"
#include "Compiler.h"
#include "Settings.h"
#include "MainFrm.h"
#include <optional>		// // //

// Define internal exporters
const LPCTSTR CExportDialog::DEFAULT_EXPORT_NAMES[] = {		// // //
	_T("NSF - Nintendo Sound File"),
	_T("NES - iNES ROM image"),
	_T("BIN - Raw music data"),
	_T("PRG - Clean 32kB ROM image"),
	_T("ASM - Assembly source"),
	_T("NSFe - Extended Nintendo Sound File"),		// // //
};

const exportFunc_t CExportDialog::DEFAULT_EXPORT_FUNCS[] = {
	&CExportDialog::CreateNSF,
	&CExportDialog::CreateNES,
	&CExportDialog::CreateBIN,
	&CExportDialog::CreatePRG,
	&CExportDialog::CreateASM,
	&CExportDialog::CreateNSFe,		// // //
};

const int CExportDialog::DEFAULT_EXPORTERS = 6;		// // //

// Remember last option when dialog is closed
int CExportDialog::m_iExportOption = 0;

// File filters
const LPCTSTR CExportDialog::NSF_FILTER[]   = { _T("NSF file (*.nsf)"), _T(".nsf") };
const LPCTSTR CExportDialog::NES_FILTER[]   = { _T("NES ROM image (*.nes)"), _T(".nes") };
const LPCTSTR CExportDialog::RAW_FILTER[]   = { _T("Raw song data (*.bin)"), _T(".bin") };
const LPCTSTR CExportDialog::DPCMS_FILTER[] = { _T("DPCM sample bank (*.bin)"), _T(".bin") };
const LPCTSTR CExportDialog::PRG_FILTER[]   = { _T("NES program bank (*.prg)"), _T(".prg") };
const LPCTSTR CExportDialog::ASM_FILTER[]	  = { _T("Assembly text (*.asm)"), _T(".asm") };
const LPCTSTR CExportDialog::NSFE_FILTER[]  = { _T("NSFe file (*.nsfe)"), _T(".nsfe") };		// // //

namespace {		// // //

std::optional<CString> GetSavePath(const CString &initFName, const CString &initPath, const CString &FilterName, const CString &FilterExt) {
	CString filter = LoadDefaultFilter(FilterName, FilterExt);
	CFileDialog FileDialog(FALSE, FilterExt, initFName, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, filter);
	FileDialog.m_pOFN->lpstrInitialDir = initPath;

	if (FileDialog.DoModal() != IDOK)
		return std::nullopt;

	return FileDialog.GetPathName();
}

} // namespace

// Compiler logger

class CEditLog : public CCompilerLog
{
public:
	CEditLog(CWnd *pEdit) : m_pEdit(static_cast<CEdit*>(pEdit)) {};
	void WriteLog(LPCTSTR text);
	void Clear();
private:
	CEdit *m_pEdit;
};

void CEditLog::WriteLog(LPCTSTR text)
{
	int Len = m_pEdit->GetWindowTextLength();
	m_pEdit->SetSel(Len, Len, 0);
	m_pEdit->ReplaceSel(text, 0);
	m_pEdit->RedrawWindow();
}

void CEditLog::Clear()
{
	m_pEdit->SetWindowText(_T(""));
	m_pEdit->RedrawWindow();
}

// CExportDialog dialog

IMPLEMENT_DYNAMIC(CExportDialog, CDialog)
CExportDialog::CExportDialog(CWnd* pParent /*=NULL*/)
	: CDialog(CExportDialog::IDD, pParent)
{
}

CExportDialog::~CExportDialog()
{
}

void CExportDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CExportDialog, CDialog)
	ON_BN_CLICKED(IDC_CLOSE, OnBnClickedClose)
	ON_BN_CLICKED(IDC_EXPORT, &CExportDialog::OnBnClickedExport)
	ON_BN_CLICKED(IDC_PLAY, OnBnClickedPlay)
END_MESSAGE_MAP()


// CExportDialog message handlers

void CExportDialog::OnBnClickedClose()
{
	EndDialog(0);
}

BOOL CExportDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	CFrameWnd *pFrameWnd = static_cast<CFrameWnd*>(GetParent());
	CFamiTrackerDoc *pDoc = static_cast<CFamiTrackerDoc*>(pFrameWnd->GetActiveDocument());

	// Check PAL button if it's a PAL song
	if (pDoc->GetMachine() == PAL) {
		CheckDlgButton(IDC_NTSC, 0);
		CheckDlgButton(IDC_PAL, 1);
		CheckDlgButton(IDC_DUAL, 0);
	}
	else {
		CheckDlgButton(IDC_NTSC, 1);
		CheckDlgButton(IDC_PAL, 0);
		CheckDlgButton(IDC_DUAL, 0);
	}

	SetDlgItemText(IDC_NAME, pDoc->GetModuleName().data());		// // //
	SetDlgItemText(IDC_ARTIST, pDoc->GetModuleArtist().data());
	SetDlgItemText(IDC_COPYRIGHT, pDoc->GetModuleCopyright().data());

	// Fill the export box
	CComboBox *pTypeBox = static_cast<CComboBox*>(GetDlgItem(IDC_TYPE));

	// Add built in exporters
	for (int i = 0; i < DEFAULT_EXPORTERS; ++i)
		pTypeBox->AddString(DEFAULT_EXPORT_NAMES[i]);

	// // //

	// Set default selection
	pTypeBox->SetCurSel(m_iExportOption);

#ifdef _DEBUG
	GetDlgItem(IDC_PLAY)->ShowWindow(SW_SHOW);
#endif

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CExportDialog::OnBnClickedExport()
{
	CComboBox *pTypeCombo = static_cast<CComboBox*>(GetDlgItem(IDC_TYPE));
	CString ItemText;

	m_iExportOption = pTypeCombo->GetCurSel();
	pTypeCombo->GetLBText(m_iExportOption, ItemText);

	// Check built in exporters
	for (int i = 0; i < DEFAULT_EXPORTERS; ++i) {
		if (!ItemText.Compare(DEFAULT_EXPORT_NAMES[i])) {
			(this->*DEFAULT_EXPORT_FUNCS[i])();
			return;
		}
	}
}

void CExportDialog::CreateNSF()
{
	CFamiTrackerDoc *pDoc = CFamiTrackerDoc::GetDoc();

	if (auto path = GetSavePath(pDoc->GetFileTitle(), theApp.GetSettings()->GetPath(PATH_NSF), NSF_FILTER[0], NSF_FILTER[1])) {		// // //
		CWaitCursor wait;

		// Collect header info
		CString Name, Artist, Copyright;
		GetDlgItemText(IDC_NAME, Name);
		GetDlgItemText(IDC_ARTIST, Artist);
		GetDlgItemText(IDC_COPYRIGHT, Copyright);
		pDoc->SetModuleName((LPCTSTR)Name);
		pDoc->SetModuleArtist((LPCTSTR)Artist);
		pDoc->SetModuleCopyright((LPCTSTR)Copyright);

		int MachineType = 0;
		if (IsDlgButtonChecked(IDC_NTSC) == BST_CHECKED)
			MachineType = 0;
		else if (IsDlgButtonChecked(IDC_PAL) == BST_CHECKED)
			MachineType = 1;
		else if (IsDlgButtonChecked(IDC_DUAL) == BST_CHECKED)
			MachineType = 2;

		CCompiler Compiler(*pDoc, std::make_unique<CEditLog>(GetDlgItem(IDC_OUTPUT)));
		Compiler.ExportNSF(*path, MachineType);
		theApp.GetSettings()->SetPath(*path, PATH_NSF);
	}
}

void CExportDialog::CreateNSFe()		// // //
{
	CFamiTrackerDoc *pDoc = CFamiTrackerDoc::GetDoc();

	if (auto path = GetSavePath(pDoc->GetFileTitle(), theApp.GetSettings()->GetPath(PATH_NSF), NSFE_FILTER[0], NSFE_FILTER[1])) {		// // //
		CWaitCursor wait;

		// Collect header info
		CString Name, Artist, Copyright;
		GetDlgItemText(IDC_NAME, Name);
		GetDlgItemText(IDC_ARTIST, Artist);
		GetDlgItemText(IDC_COPYRIGHT, Copyright);
		pDoc->SetModuleName((LPCTSTR)Name);
		pDoc->SetModuleArtist((LPCTSTR)Artist);
		pDoc->SetModuleCopyright((LPCTSTR)Copyright);

		int MachineType = 0;
		if (IsDlgButtonChecked(IDC_NTSC) == BST_CHECKED)
			MachineType = 0;
		else if (IsDlgButtonChecked(IDC_PAL) == BST_CHECKED)
			MachineType = 1;
		else if (IsDlgButtonChecked(IDC_DUAL) == BST_CHECKED)
			MachineType = 2;

		CCompiler Compiler(*pDoc, std::make_unique<CEditLog>(GetDlgItem(IDC_OUTPUT)));
		Compiler.ExportNSFE(*path, MachineType);
		theApp.GetSettings()->SetPath(*path, PATH_NSF);
	}
}

void CExportDialog::CreateNES()
{
	CFamiTrackerDoc *pDoc = CFamiTrackerDoc::GetDoc();

	if (auto path = GetSavePath(pDoc->GetFileTitle(), theApp.GetSettings()->GetPath(PATH_NSF), NES_FILTER[0], NES_FILTER[1])) {		// // //
		CWaitCursor wait;

		CCompiler Compiler(*pDoc, std::make_unique<CEditLog>(GetDlgItem(IDC_OUTPUT)));
		Compiler.ExportNES(*path, IsDlgButtonChecked(IDC_PAL) == BST_CHECKED);
		theApp.GetSettings()->SetPath(*path, PATH_NSF);
	}
}

void CExportDialog::CreateBIN()
{
	if (auto path = GetSavePath(_T("music.bin"), theApp.GetSettings()->GetPath(PATH_NSF), RAW_FILTER[0], RAW_FILTER[1])) {		// // //
		CString SampleDir = *path;		// // //

		const CString DEFAULT_SAMPLE_NAME = _T("samples.bin");		// // //

		CFamiTrackerDoc *pDoc = CFamiTrackerDoc::GetDoc();
		if (pDoc->GetSampleCount() > 0) {
			if (auto sampPath = GetSavePath(DEFAULT_SAMPLE_NAME, *path, DPCMS_FILTER[0], DPCMS_FILTER[1]))
				SampleDir = *sampPath;
			else
				return;
		}
		else {
			int Pos = SampleDir.ReverseFind(_T('\\'));
			ASSERT(Pos != -1);
			SampleDir = SampleDir.Left(Pos + 1) + DEFAULT_SAMPLE_NAME;
			if (PathFileExists(SampleDir)) {
				CString msg;
				AfxFormatString1(msg, IDS_EXPORT_SAMPLES_FILE, DEFAULT_SAMPLE_NAME);
				if (AfxMessageBox(msg, MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDNO)
					return;
			}
		}

		// Display wait cursor
		CWaitCursor wait;

		CCompiler Compiler(*pDoc, std::make_unique<CEditLog>(GetDlgItem(IDC_OUTPUT)));
		Compiler.ExportBIN(*path, SampleDir);
		theApp.GetSettings()->SetPath(*path, PATH_NSF);
	}
}

void CExportDialog::CreatePRG()
{
	if (auto path = GetSavePath(_T("music.prg"), theApp.GetSettings()->GetPath(PATH_NSF), PRG_FILTER[0], PRG_FILTER[1])) {		// // //
		CWaitCursor wait;

		CFamiTrackerDoc *pDoc = CFamiTrackerDoc::GetDoc();
		CCompiler Compiler(*pDoc, std::make_unique<CEditLog>(GetDlgItem(IDC_OUTPUT)));
		Compiler.ExportPRG(*path, IsDlgButtonChecked(IDC_PAL) == BST_CHECKED);
		theApp.GetSettings()->SetPath(*path, PATH_NSF);
	}
}

void CExportDialog::CreateASM()
{
	if (auto path = GetSavePath(_T("music.asm"), theApp.GetSettings()->GetPath(PATH_NSF), ASM_FILTER[0], ASM_FILTER[1])) {		// // //
		CWaitCursor wait;

		CFamiTrackerDoc *pDoc = CFamiTrackerDoc::GetDoc();
		CCompiler Compiler(*pDoc, std::make_unique<CEditLog>(GetDlgItem(IDC_OUTPUT)));
		Compiler.ExportASM(*path);
		theApp.GetSettings()->SetPath(*path, PATH_NSF);
	}
}

void CExportDialog::OnBnClickedPlay()
{
#ifdef _DEBUG

//	if (m_strFile.GetLength() == 0)
//		return;

	const char *file = "d:\\test.nsf";		// // //

	CFamiTrackerDoc *pDoc = CFamiTrackerDoc::GetDoc();
	CCompiler Compiler(*pDoc, std::make_unique<CEditLog>(GetDlgItem(IDC_OUTPUT)));
	Compiler.ExportNSF(file, IsDlgButtonChecked(IDC_PAL) == BST_CHECKED);

	// Play exported file (available in debug)
	ShellExecute(NULL, _T("open"), file, NULL, NULL, SW_SHOWNORMAL);

#endif
}
