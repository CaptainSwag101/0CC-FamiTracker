/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2014  Jonathan Liss
**
** 0CC-FamiTracker is (C) 2014-2017 HertzDevil
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

 Document file version changes

 Ver 4.0
  - Header block, added song names

 Ver 3.0
  - Sequences are stored in the way they are represented in the instrument editor
  - Added separate speed and tempo settings
  - Changed automatic portamento to 3xx and added 1xx & 2xx portamento

 Ver 2.1
  - Made some additions to support multiple effect columns and prepared for more channels
  - Made some speed adjustments, increase speed effects by one if it's below 20

 Ver 2.0
  - Files are small

*/

#include "FamiTrackerDoc.h"
#include <memory>		// // //
#include <algorithm>
#include <vector>		// // //
#include <string>		// // //
#include <array>		// // //
#include <unordered_map>		// // //
#include "FamiTracker.h"
#include "Instrument.h"		// // //
#include "SeqInstrument.h"		// // //
#include "Instrument2A03.h"		// // //
#include "InstrumentVRC6.h"		// // // for error messages only
#include "InstrumentN163.h"		// // // for error messages only
#include "InstrumentS5B.h"		// // // for error messages only
#include "ModuleException.h"		// // //
#include "TrackerChannel.h"
#include "DocumentFile.h"
#include "SoundGen.h"
#include "SequenceCollection.h"		// // //
#include "SequenceManager.h"		// // //
#include "DSampleManager.h"			// // //
#include "InstrumentManager.h"		// // //
#include "Bookmark.h"		// // //
#include "BookmarkCollection.h"		// // //
#include "BookmarkManager.h"		// // //
#include "APU/APU.h"
#include "SimpleFile.h"		// // //
//#include "SongView.h"		// // //
#include "ChannelMap.h"		// // //
#include "FamiTrackerDocIO.h"		// // // TODO: remove
#include "Groove.h"		// // //
#include "SongData.h"		// // //

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Make 1 channel default since 8 sounds bad
const int	CFamiTrackerDoc::DEFAULT_NAMCO_CHANS = 1;

const bool	CFamiTrackerDoc::DEFAULT_LINEAR_PITCH = false;

// File I/O constants
static const char *FILE_HEADER				= "FamiTracker Module";
static const char *FILE_BLOCK_PARAMS		= "PARAMS";
static const char *FILE_BLOCK_INFO			= "INFO";
static const char *FILE_BLOCK_INSTRUMENTS	= "INSTRUMENTS";
static const char *FILE_BLOCK_SEQUENCES		= "SEQUENCES";
static const char *FILE_BLOCK_FRAMES		= "FRAMES";
static const char *FILE_BLOCK_PATTERNS		= "PATTERNS";
static const char *FILE_BLOCK_DSAMPLES		= "DPCM SAMPLES";
static const char *FILE_BLOCK_HEADER		= "HEADER";
static const char *FILE_BLOCK_COMMENTS		= "COMMENTS";

// VRC6
static const char *FILE_BLOCK_SEQUENCES_VRC6 = "SEQUENCES_VRC6";

// N163
static const char *FILE_BLOCK_SEQUENCES_N163 = "SEQUENCES_N163";
static const char *FILE_BLOCK_SEQUENCES_N106 = "SEQUENCES_N106";

// Sunsoft
static const char *FILE_BLOCK_SEQUENCES_S5B = "SEQUENCES_S5B";

// // // 0CC-FamiTracker specific
const char *FILE_BLOCK_DETUNETABLES			= "DETUNETABLES";
const char *FILE_BLOCK_GROOVES				= "GROOVES";
const char *FILE_BLOCK_BOOKMARKS			= "BOOKMARKS";
const char *FILE_BLOCK_PARAMS_EXTRA			= "PARAMS_EXTRA";

/* 
	Instrument version history
	 * 2.1 - Release points for sequences in 2A03 & VRC6
	 * 2.2 - FDS volume sequences goes from 0-31 instead of 0-15
	 * 2.3 - Support for release points & extra setting in sequences, 2A03 & VRC6
	 * 2.4 - DPCM delta counter setting
*/

// File blocks

enum {
	FB_INSTRUMENTS,
	FB_SEQUENCES,
	FB_PATTERN_ROWS,
	FB_PATTERNS,
	FB_SPEED,
	FB_CHANNELS,
	FB_DSAMPLES,
	FB_EOF,
	FB_MACHINE,
	FB_ENGINESPEED,
	FB_SONGNAME,
	FB_SONGARTIST,
	FB_SONGCOPYRIGHT
};

// // // helper function for effect conversion
typedef std::array<effect_t, EF_COUNT> EffTable;
std::pair<EffTable, EffTable> MakeEffectConversion(std::initializer_list<std::pair<effect_t, effect_t>> List)
{
	EffTable forward, backward;
	for (int i = 0; i < EF_COUNT; ++i)
		forward[i] = backward[i] = static_cast<effect_t>(i);
	for (const auto &p : List) {
		forward[p.first] = p.second;
		backward[p.second] = p.first;
	}
	return std::make_pair(forward, backward);
}

static const auto EFF_CONVERSION_050 = MakeEffectConversion({
//	{EF_SUNSOFT_ENV_LO,		EF_SUNSOFT_ENV_TYPE},
//	{EF_SUNSOFT_ENV_TYPE,	EF_SUNSOFT_ENV_LO},
	{EF_SUNSOFT_NOISE,		EF_NOTE_RELEASE},
	{EF_VRC7_PORT,			EF_GROOVE},
	{EF_VRC7_WRITE,			EF_TRANSPOSE},
	{EF_NOTE_RELEASE,		EF_N163_WAVE_BUFFER},
	{EF_GROOVE,				EF_FDS_VOLUME},
	{EF_TRANSPOSE,			EF_FDS_MOD_BIAS},
	{EF_N163_WAVE_BUFFER,	EF_SUNSOFT_NOISE},
	{EF_FDS_VOLUME,			EF_VRC7_PORT},
	{EF_FDS_MOD_BIAS,		EF_VRC7_WRITE},
});

//
// CFamiTrackerDoc
//

IMPLEMENT_DYNCREATE(CFamiTrackerDoc, CDocument)

BEGIN_MESSAGE_MAP(CFamiTrackerDoc, CDocument)
	ON_COMMAND(ID_FILE_SAVE_AS, OnFileSaveAs)
	ON_COMMAND(ID_FILE_SAVE, OnFileSave)
END_MESSAGE_MAP()

// CFamiTrackerDoc construction/destruction

CFamiTrackerDoc::CFamiTrackerDoc() : 
	m_bFileLoaded(false), 
	m_bFileLoadFailed(false), 
	m_iNamcoChannels(0),		// // //
	m_bDisplayComment(false),
	m_pChannelMap(std::make_unique<CChannelMap>()),		// // //
	m_pInstrumentManager(std::make_unique<CInstrumentManager>(this)),
	m_pBookmarkManager(std::make_unique<CBookmarkManager>(MAX_TRACKS))
{
	// Initialize document object

	ResetDetuneTables();		// // //

	// Register this object to the sound generator
	if (CSoundGen *pSoundGen = theApp.GetSoundGenerator())
		pSoundGen->AssignDocument(this);

	AllocateSong(0);		// // //
}

CFamiTrackerDoc::~CFamiTrackerDoc()
{
	// // //
}

//
// Static functions
//

CFamiTrackerDoc *CFamiTrackerDoc::GetDoc()
{
	CFrameWnd *pFrame = static_cast<CFrameWnd*>(AfxGetApp()->m_pMainWnd);
	ASSERT_VALID(pFrame);

	return static_cast<CFamiTrackerDoc*>(pFrame->GetActiveDocument());
}

// Synchronization
BOOL CFamiTrackerDoc::LockDocument() const
{
	return m_csDocumentLock.Lock();
}

BOOL CFamiTrackerDoc::LockDocument(DWORD dwTimeout) const
{
	return m_csDocumentLock.Lock(dwTimeout);
}

BOOL CFamiTrackerDoc::UnlockDocument() const
{
	return m_csDocumentLock.Unlock();
}

//
// Overrides
//

BOOL CFamiTrackerDoc::OnNewDocument()
{
	// Called by the GUI to create a new file

	// This calls DeleteContents
	if (!CDocument::OnNewDocument())
		return FALSE;

	CreateEmpty();

	return TRUE;
}

BOOL CFamiTrackerDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	// This function is called by the GUI to load a file

	//DeleteContents();
	theApp.GetSoundGenerator()->ResetDumpInstrument();
	theApp.GetSoundGenerator()->SetRecordChannel(-1);		// // //

	m_csDocumentLock.Lock();

	// Load file
	if (!OpenDocument(lpszPathName)) {
		// Loading failed, create empty document
		m_csDocumentLock.Unlock();
		/*
		DeleteContents();
		CreateEmpty();
		for (int i = UPDATE_TRACK; i <= UPDATE_COLUMNS; ++i)		// // // test
			UpdateAllViews(NULL, i);
		*/
		// and tell doctemplate that loading failed
		return FALSE;
	}

	m_csDocumentLock.Unlock();

	// Update main frame
	ApplyExpansionChip();

#ifdef AUTOSAVE
	SetupAutoSave();
#endif

	// Remove modified flag
	SetModifiedFlag(FALSE);

	SetExceededFlag(FALSE);		// // //

	return TRUE;
}

BOOL CFamiTrackerDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
#ifdef DISABLE_SAVE		// // //
	static_cast<CFrameWnd*>(AfxGetMainWnd())->SetMessageText(IDS_DISABLE_SAVE);
	return FALSE;
#endif

	// This function is called by the GUI to save the file

	if (!IsFileLoaded())
		return FALSE;

	// File backup, now performed on save instead of open
	if ((m_bForceBackup || theApp.GetSettings()->General.bBackups) && !m_bBackupDone) {
		CString BakName;
		BakName.Format(_T("%s.bak"), lpszPathName);
		CopyFile(lpszPathName, BakName.GetBuffer(), FALSE);
		m_bBackupDone = true;
	}

	if (!SaveDocument(lpszPathName))
		return FALSE;

	// Reset modified flag
	SetModifiedFlag(FALSE);

	SetExceededFlag(FALSE);		// // //

	return TRUE;
}

void CFamiTrackerDoc::OnCloseDocument()
{	
	// Document object is about to be deleted

	// Remove itself from sound generator
	CSoundGen *pSoundGen = theApp.GetSoundGenerator();

	if (pSoundGen)
		pSoundGen->RemoveDocument();

	CDocument::OnCloseDocument();
}

void CFamiTrackerDoc::DeleteContents()
{
	// Current document is being unloaded, clear and reset variables and memory
	// Delete everything because the current object is being reused in SDI

	// Make sure player is stopped
	theApp.StopPlayerAndWait();

	m_csDocumentLock.Lock();

	// Mark file as unloaded
	m_bFileLoaded = false;
	m_bForceBackup = false;
	m_bBackupDone = true;	// No backup on new modules

	UpdateAllViews(NULL, UPDATE_CLOSE);	// TODO remove

	// Delete all patterns
	m_pTracks.clear();		// // //

	// // // Grooves
	for (auto &x : m_pGrooveTable)		// // //
		x.reset();

	m_pInstrumentManager->ClearAll();		// // //
	m_pBookmarkManager->ClearAll();		// // //

	// Clear song info
	SetModuleName("");		// // //
	SetModuleArtist("");
	SetModuleCopyright("");

	// Reset variables to default
	m_iMachine			 = DEFAULT_MACHINE_TYPE;
	m_iEngineSpeed		 = 0;
	m_iExpansionChip	 = SNDCHIP_NONE;
	m_iVibratoStyle		 = VIBRATO_OLD;
	m_bLinearPitch		 = DEFAULT_LINEAR_PITCH;
	m_iChannelsAvailable = CHANNELS_DEFAULT;
	m_iSpeedSplitPoint	 = DEFAULT_SPEED_SPLIT_POINT;
	m_iDetuneSemitone	 = 0;		// // // 050B
	m_iDetuneCent		 = 0;		// // // 050B

	m_vHighlight = CSongData::DEFAULT_HIGHLIGHT;		// // //

	ResetDetuneTables();		// // //

	// Used for loading older files
	m_vTmpSequences.clear();		// // //

	// Auto save
#ifdef AUTOSAVE
	ClearAutoSave();
#endif

	SetComment("", false);		// // //

	// // // Allocate first song
	AllocateSong(0);

	// Remove modified flag
	SetModifiedFlag(FALSE);
	SetExceededFlag(FALSE);		// // //

	m_csDocumentLock.Unlock();

	CDocument::DeleteContents();
}

void CFamiTrackerDoc::SetModifiedFlag(BOOL bModified)
{
	// Trigger auto-save in 10 seconds
#ifdef AUTOSAVE
	if (bModified)
		m_iAutoSaveCounter = 10;
#endif

	BOOL bWasModified = IsModified();
	CDocument::SetModifiedFlag(bModified);
	
	CFrameWnd *pFrameWnd = dynamic_cast<CFrameWnd*>(theApp.m_pMainWnd);
	if (pFrameWnd != NULL) {
		if (pFrameWnd->GetActiveDocument() == this && bWasModified != bModified) {
			pFrameWnd->OnUpdateFrameTitle(TRUE);
		}
	}
}

void CFamiTrackerDoc::CreateEmpty()
{
	m_csDocumentLock.Lock();

	DeleteContents();		// // //

	// Auto-select new style vibrato for new modules
	m_iVibratoStyle = VIBRATO_NEW;
	m_bLinearPitch = DEFAULT_LINEAR_PITCH;

	m_iNamcoChannels = 0;		// // //

	// and select 2A03 only
	SelectExpansionChip(SNDCHIP_NONE);

#ifdef AUTOSAVE
	SetupAutoSave();
#endif

	SetModifiedFlag(FALSE);
	SetExceededFlag(FALSE);		// // //

	// Document is avaliable
	m_bFileLoaded = true;

	m_csDocumentLock.Unlock();

	theApp.GetSoundGenerator()->DocumentPropertiesChanged(this);
}

//
// Messages
//

void CFamiTrackerDoc::OnFileSave()
{
#ifdef DISABLE_SAVE		// // //
	static_cast<CFrameWnd*>(AfxGetMainWnd())->SetMessageText(IDS_DISABLE_SAVE);
	return;
#endif

	if (GetPathName().GetLength() == 0)
		OnFileSaveAs();
	else
		CDocument::OnFileSave();
}

void CFamiTrackerDoc::OnFileSaveAs()
{
#ifdef DISABLE_SAVE		// // //
	static_cast<CFrameWnd*>(AfxGetMainWnd())->SetMessageText(IDS_DISABLE_SAVE);
	return;
#endif

	// Overloaded in order to save the ftm-path
	CString newName = GetPathName();
	
	if (!AfxGetApp()->DoPromptFileName(newName, AFX_IDS_SAVEFILE, OFN_HIDEREADONLY | OFN_PATHMUSTEXIST, FALSE, NULL))
		return;

	theApp.GetSettings()->SetPath(newName, PATH_FTM);
	
	DoSave(newName);
}

// CFamiTrackerDoc serialization (never used)

void CFamiTrackerDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}


// CFamiTrackerDoc diagnostics

#ifdef _DEBUG
void CFamiTrackerDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CFamiTrackerDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// CFamiTrackerDoc commands

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File load / save routines
////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Functions for compability with older file versions

void CFamiTrackerDoc::ReorderSequences()
{
	int Slots[SEQ_COUNT] = {0, 0, 0, 0, 0};
	int Indices[MAX_SEQUENCES][SEQ_COUNT];

	memset(Indices, 0xFF, MAX_SEQUENCES * SEQ_COUNT * sizeof(int));

	// Organize sequences
	for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
		if (auto pInst = std::dynamic_pointer_cast<CInstrument2A03>(m_pInstrumentManager->GetInstrument(i))) {		// // //
			for (int j = 0; j < SEQ_COUNT; ++j) {
				if (pInst->GetSeqEnable(j)) {
					int Index = pInst->GetSeqIndex(j);
					if (Indices[Index][j] >= 0 && Indices[Index][j] != -1) {
						pInst->SetSeqIndex(j, Indices[Index][j]);
					}
					else {
						COldSequence &Seq = m_vTmpSequences[Index];		// // //
						if (j == SEQ_VOLUME)
							for (unsigned int k = 0; k < Seq.GetLength(); ++k)
								Seq.Value[k] = std::max(std::min<int>(Seq.Value[k], 15), 0);
						else if (j == SEQ_DUTYCYCLE)
							for (unsigned int k = 0; k < Seq.GetLength(); ++k)
								Seq.Value[k] = std::max(std::min<int>(Seq.Value[k], 3), 0);
						Indices[Index][j] = Slots[j];
						pInst->SetSeqIndex(j, Slots[j]);
						m_pInstrumentManager->SetSequence(INST_2A03, j, Slots[j]++, Seq.Convert(j).release());
					}
				}
				else
					pInst->SetSeqIndex(j, 0);
			}
		}
	}
}

template <module_error_level_t l>
void CFamiTrackerDoc::AssertFileData(bool Cond, std::string Msg) const
{
	if (l <= theApp.GetSettings()->Version.iErrorLevel && !Cond) {
		CModuleException *e = m_pCurrentDocument ? m_pCurrentDocument->GetException() : new CModuleException();
		e->AppendError(Msg);
		e->Raise();
	}
}

/*** File format description ***

0000: "FamiTracker Module"					id string
000x: Version								int, version number
000x: Start of blocks

 {FILE_BLOCK_PARAMS, 2}
  Expansion chip							char
  Channels									int
  Machine type								int
  Engine speed								int

 {FILE_BLOCK_INFO, 1}
  Song name									string, 32 bytes
  Artist name								string, 32 bytes
  Copyright									string, 32 bytes

000x: End of blocks
000x: "END"						End of file

********************************/


////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Document store functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CFamiTrackerDoc::SaveDocument(LPCTSTR lpszPathName) const
{
	CDocumentFile DocumentFile;
	m_pCurrentDocument = &DocumentFile;		// // //
	CFileException ex;
	TCHAR TempPath[MAX_PATH], TempFile[MAX_PATH];

	// First write to a temp file (if saving fails, the original is not destroyed)
	GetTempPath(MAX_PATH, TempPath);
	GetTempFileName(TempPath, _T("FTM"), 0, TempFile);

	if (!DocumentFile.Open(TempFile, CFile::modeWrite | CFile::modeCreate, &ex)) {
		// Could not open file
		TCHAR szCause[255];
		CString strFormatted;
		ex.GetErrorMessage(szCause, 255);
		AfxFormatString1(strFormatted, IDS_SAVE_FILE_ERROR, szCause);
		AfxMessageBox(strFormatted, MB_OK | MB_ICONERROR);
		m_pCurrentDocument = nullptr;		// // //
		return FALSE;
	}

	if (!CFamiTrackerDocIO {DocumentFile}.Save(*this)) {		// // //
		// The save process failed, delete temp file
		DocumentFile.Close();
		DeleteFile(TempFile);
		// Display error
		CString	ErrorMsg;
		ErrorMsg.LoadString(IDS_SAVE_ERROR);
		AfxMessageBox(ErrorMsg, MB_OK | MB_ICONERROR);
		m_pCurrentDocument = nullptr;		// // //
		return FALSE;
	}

	ULONGLONG FileSize = DocumentFile.GetLength();

	DocumentFile.Close();
	m_pCurrentDocument = nullptr;		// // //

	// Save old creation date
	HANDLE hOldFile;
	FILETIME creationTime;

	hOldFile = CreateFile(lpszPathName, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	GetFileTime(hOldFile, &creationTime, NULL, NULL);
	CloseHandle(hOldFile);

	// Everything is done and the program cannot crash at this point
	// Replace the original
	if (!MoveFileEx(TempFile, lpszPathName, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
		// Display message if saving failed
		AfxDebugBreak();		// // //
		TCHAR *lpMsgBuf;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);
		CString	strFormatted;
		AfxFormatString1(strFormatted, IDS_SAVE_FILE_ERROR, lpMsgBuf);
		AfxMessageBox(strFormatted, MB_OK | MB_ICONERROR);
		LocalFree(lpMsgBuf);
		// Remove temp file
		DeleteFile(TempFile);
		return FALSE;
	}

	// Restore creation date
	hOldFile = CreateFile(lpszPathName, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	SetFileTime(hOldFile, &creationTime, NULL, NULL);
	CloseHandle(hOldFile);

	// Todo: avoid calling the main window from document class
	if (CFrameWnd *pMainFrame = static_cast<CFrameWnd*>(AfxGetMainWnd())) {		// // //
		CString text;
		AfxFormatString1(text, IDS_FILE_SAVED, std::to_string(FileSize).c_str());		// // //
		pMainFrame->SetMessageText(text);
	}

	return TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Document load functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL CFamiTrackerDoc::OpenDocument(LPCTSTR lpszPathName)
{
	m_bFileLoadFailed = true;

	CFileException ex;
	CDocumentFile  OpenFile;

	// Open file
	if (!OpenFile.Open(lpszPathName, CFile::modeRead | CFile::shareDenyWrite, &ex)) {
		TCHAR   szCause[1024];		// // //
		CString strFormatted;
		ex.GetErrorMessage(szCause, sizeof(szCause));
		strFormatted = _T("Could not open file.\n\n");
		strFormatted += szCause;
		AfxMessageBox(strFormatted);
		//OnNewDocument();
		return FALSE;
	}

	// Check if empty file
	if (OpenFile.GetLength() == 0) {
		// Setup default settings
		CreateEmpty();
		return TRUE;
	}
	
	m_pCurrentDocument = &OpenFile;		// // // closure
	try {		// // //
		// Read header ID and version
		OpenFile.ValidateFile();

		m_iFileVersion = OpenFile.GetFileVersion();
		DeleteContents();		// // //

		if (m_iFileVersion < 0x0200U) {
			if (!OpenDocumentOld(&OpenFile))
				OpenFile.RaiseModuleException("General error");

			// Create a backup of this file, since it's an old version 
			// and something might go wrong when converting
			m_bForceBackup = true;
		}
		else {
			if (!OpenDocumentNew(OpenFile))
				OpenFile.RaiseModuleException("General error");

			// Backup if files was of an older version
			m_bForceBackup = m_iFileVersion < CDocumentFile::FILE_VER;
		}
	}
	catch (CModuleException *e) {
		AfxMessageBox(e->GetErrorString().c_str(), MB_ICONERROR);
		m_pCurrentDocument = nullptr;		// // //
		delete e;
		return FALSE;
	}

	m_pCurrentDocument = nullptr;		// // //

	// File is loaded
	m_bFileLoaded = true;
	m_bFileLoadFailed = false;
	m_bBackupDone = false;		// // //

	theApp.GetSoundGenerator()->DocumentPropertiesChanged(this);

	return TRUE;
}

/**
 * This function reads the old obsolete file version. 
 */
BOOL CFamiTrackerDoc::OpenDocumentOld(CFile *pOpenFile)
{
	unsigned int i, c, ReadCount, FileBlock;

	FileBlock = 0;

	// Only single track files
	auto &Song = GetSongData(0);

	m_iVibratoStyle = VIBRATO_OLD;
	m_bLinearPitch = false;

	// // // local structs
	struct {
		char Name[256];
		bool Free;
		int	 ModEnable[SEQ_COUNT];
		int	 ModIndex[SEQ_COUNT];
		int	 AssignedSample;				// For DPCM
	} ImportedInstruments;
	struct {
		char Length[64];
		char Value[64];
		unsigned int Count;
	} ImportedSequence;
	struct {
		char *SampleData;
		int	 SampleSize;
		char Name[256];
	} ImportedDSample;
	struct {
		int	Note;
		int	Octave;
		int	Vol;
		int	Instrument;
		int	ExtraStuff1;
		int	ExtraStuff2;
	} ImportedNote;

	while (FileBlock != FB_EOF) {
		if (pOpenFile->Read(&FileBlock, sizeof(int)) == 0)
			FileBlock = FB_EOF;

		unsigned int Speed, FrameCount, Pattern, PatternLength;
		char pBuf[METADATA_FIELD_LENGTH] = { };		// // //

		switch (FileBlock) {
			case FB_CHANNELS:
				pOpenFile->Read(&m_iChannelsAvailable, sizeof(int));
				break;

			case FB_SPEED:
				pOpenFile->Read(&Speed, sizeof(int));
				Song.SetSongSpeed(Speed + 1);
				break;

			case FB_MACHINE:
				pOpenFile->Read(&m_iMachine, sizeof(int));				
				break;

			case FB_ENGINESPEED:
				pOpenFile->Read(&m_iEngineSpeed, sizeof(int));
				break;

			case FB_INSTRUMENTS:
				pOpenFile->Read(&ReadCount, sizeof(int));
				if (ReadCount > MAX_INSTRUMENTS)
					ReadCount = MAX_INSTRUMENTS - 1;
				for (i = 0; i < ReadCount; i++) {
					pOpenFile->Read(&ImportedInstruments, sizeof(ImportedInstruments));
					if (ImportedInstruments.Free == false) {
						auto pInst = std::make_unique<CInstrument2A03>();
						for (int j = 0; j < SEQ_COUNT; j++) {
							pInst->SetSeqEnable(j, ImportedInstruments.ModEnable[j]);
							pInst->SetSeqIndex(j, ImportedInstruments.ModIndex[j]);
						}
						pInst->SetName(ImportedInstruments.Name);

						if (ImportedInstruments.AssignedSample > 0) {
							int Pitch = 0;
							for (int y = 0; y < 6; y++) {
								for (int x = 0; x < 12; x++) {
									pInst->SetSampleIndex(y, x, ImportedInstruments.AssignedSample);
									pInst->SetSamplePitch(y, x, Pitch);
									Pitch = (Pitch + 1) % 16;
								}
							}
						}

						m_pInstrumentManager->InsertInstrument(i, std::move(pInst));		// // //
					}
				}
				break;

			case FB_SEQUENCES:
				pOpenFile->Read(&ReadCount, sizeof(int));
				for (i = 0; i < ReadCount; i++) {
					COldSequence Seq;
					pOpenFile->Read(&ImportedSequence, sizeof(ImportedSequence));
					if (ImportedSequence.Count > 0 && ImportedSequence.Count < MAX_SEQUENCE_ITEMS)
						for (unsigned int i = 0; i < ImportedSequence.Count; ++i)		// // //
							Seq.AddItem(ImportedSequence.Length[i], ImportedSequence.Value[i]);
					m_vTmpSequences.push_back(Seq);		// // //
				}
				break;

			case FB_PATTERN_ROWS:
				pOpenFile->Read(&FrameCount, sizeof(int));
				Song.SetFrameCount(FrameCount);
				for (c = 0; c < FrameCount; c++) {
					for (i = 0; i < m_iChannelsAvailable; i++) {
						pOpenFile->Read(&Pattern, sizeof(int));
						Song.SetFramePattern(c, i, Pattern);
					}
				}
				break;

			case FB_PATTERNS:
				pOpenFile->Read(&ReadCount, sizeof(int));
				pOpenFile->Read(&PatternLength, sizeof(int));
				Song.SetPatternLength(PatternLength);
				for (unsigned int x = 0; x < m_iChannelsAvailable; x++) {
					for (c = 0; c < ReadCount; c++) {
						for (i = 0; i < PatternLength; i++) {
							pOpenFile->Read(&ImportedNote, sizeof(ImportedNote));
							if (ImportedNote.ExtraStuff1 == EF_PORTAOFF) {
								ImportedNote.ExtraStuff1 = EF_PORTAMENTO;
								ImportedNote.ExtraStuff2 = 0;
							}
							else if (ImportedNote.ExtraStuff1 == EF_PORTAMENTO) {
								if (ImportedNote.ExtraStuff2 < 0xFF)
									ImportedNote.ExtraStuff2++;
							}
							auto &Note = Song.GetPatternData(x, c, i);		// // //
							Note.EffNumber[0]	= static_cast<effect_t>(ImportedNote.ExtraStuff1);
							Note.EffParam[0]	= ImportedNote.ExtraStuff2;
							Note.Instrument	= ImportedNote.Instrument;
							Note.Note			= ImportedNote.Note;
							Note.Octave		= ImportedNote.Octave;
							Note.Vol			= 0;
							if (Note.Note == 0)
								Note.Instrument = MAX_INSTRUMENTS;
							if (Note.Vol == 0)
								Note.Vol = MAX_VOLUME;
							if (Note.EffNumber[0] < EF_COUNT)		// // //
								Note.EffNumber[0] = EFF_CONVERSION_050.first[Note.EffNumber[0]];
						}
					}
				}
				break;

			case FB_DSAMPLES:
				pOpenFile->Read(&ReadCount, sizeof(int));
				for (i = 0; i < ReadCount; i++) {
					pOpenFile->Read(&ImportedDSample, sizeof(ImportedDSample));
					if (ImportedDSample.SampleSize != 0 && ImportedDSample.SampleSize < 0x4000) {
						ImportedDSample.SampleData = new char[ImportedDSample.SampleSize];
						pOpenFile->Read(ImportedDSample.SampleData, ImportedDSample.SampleSize);
					}
					else
						ImportedDSample.SampleData = NULL;
					CDSample *pSamp = new CDSample();		// // //
					pSamp->SetName(ImportedDSample.Name);
					pSamp->SetData(ImportedDSample.SampleSize, ImportedDSample.SampleData);
					SetSample(i, pSamp);
				}
				break;

			case FB_SONGNAME:
				pOpenFile->Read(pBuf, METADATA_FIELD_LENGTH);		// // //
				SetModuleName(pBuf);
				break;

			case FB_SONGARTIST:
				pOpenFile->Read(pBuf, METADATA_FIELD_LENGTH);
				SetModuleArtist(pBuf);
				break;
		
			case FB_SONGCOPYRIGHT:
				pOpenFile->Read(pBuf, METADATA_FIELD_LENGTH);
				SetModuleCopyright(pBuf);
				break;
			
			default:
				FileBlock = FB_EOF;
		}
	}

	SetupChannels(m_iExpansionChip);

	ReorderSequences();

	// De-allocate memory
	m_vTmpSequences.clear();		// // //

	pOpenFile->Close();

	return TRUE;
}

/**
 *  This function opens the most recent file version
 *
 */
BOOL CFamiTrackerDoc::OpenDocumentNew(CDocumentFile &DocumentFile)
{
	using map_t = std::unordered_map<std::string, void (CFamiTrackerDoc::*)(CDocumentFile *, const int)>;
	static const auto FTM_READ_FUNC = map_t {
		{FILE_BLOCK_PARAMS,			&CFamiTrackerDoc::ReadBlock_Parameters},
		{FILE_BLOCK_INFO,			&CFamiTrackerDoc::ReadBlock_SongInfo},
		{FILE_BLOCK_HEADER,			&CFamiTrackerDoc::ReadBlock_Header},
		{FILE_BLOCK_INSTRUMENTS,	&CFamiTrackerDoc::ReadBlock_Instruments},
		{FILE_BLOCK_SEQUENCES,		&CFamiTrackerDoc::ReadBlock_Sequences},
		{FILE_BLOCK_FRAMES,			&CFamiTrackerDoc::ReadBlock_Frames},
		{FILE_BLOCK_PATTERNS,		&CFamiTrackerDoc::ReadBlock_Patterns},
		{FILE_BLOCK_DSAMPLES,		&CFamiTrackerDoc::ReadBlock_DSamples},
		{FILE_BLOCK_COMMENTS,		&CFamiTrackerDoc::ReadBlock_Comments},
		{FILE_BLOCK_SEQUENCES_VRC6,	&CFamiTrackerDoc::ReadBlock_SequencesVRC6},
		{FILE_BLOCK_SEQUENCES_N163,	&CFamiTrackerDoc::ReadBlock_SequencesN163},
		{FILE_BLOCK_SEQUENCES_N106,	&CFamiTrackerDoc::ReadBlock_SequencesN163},	// Backward compatibility
		{FILE_BLOCK_SEQUENCES_S5B,	&CFamiTrackerDoc::ReadBlock_SequencesS5B},	// // //
		{FILE_BLOCK_PARAMS_EXTRA,	&CFamiTrackerDoc::ReadBlock_ParamsExtra},	// // //
		{FILE_BLOCK_DETUNETABLES,	&CFamiTrackerDoc::ReadBlock_DetuneTables},	// // //
		{FILE_BLOCK_GROOVES,		&CFamiTrackerDoc::ReadBlock_Grooves},		// // //
		{FILE_BLOCK_BOOKMARKS,		&CFamiTrackerDoc::ReadBlock_Bookmarks},		// // //
	};

#ifdef _DEBUG
	int _msgs_ = 0;
#endif

#ifdef TRANSPOSE_FDS
	m_bAdjustFDSArpeggio = false;
#endif

	if (m_iFileVersion < 0x0210) {
		// This has to be done for older files
		AllocateSong(0);
	}

	// Read all blocks
	bool ErrorFlag = false;
	while (!DocumentFile.Finished() && !ErrorFlag) {
		ErrorFlag = DocumentFile.ReadBlock();
		const char *BlockID = DocumentFile.GetBlockHeaderID();
		if (!strcmp(BlockID, "END")) break;

		try {
			CALL_MEMBER_FN(this, FTM_READ_FUNC.at(BlockID))(&DocumentFile, DocumentFile.GetBlockVersion());		// // //
		}
		catch (std::out_of_range) {
		// This shouldn't show up in release (debug only)
#ifdef _DEBUG
			if (++_msgs_ < 5)
				AfxMessageBox(_T("Unknown file block!"));
#endif
			if (DocumentFile.IsFileIncomplete())
				ErrorFlag = true;
		}
	}

	DocumentFile.Close();

	if (ErrorFlag) {
		AfxMessageBox(IDS_FILE_LOAD_ERROR, MB_ICONERROR);
		DeleteContents();
		return FALSE;
	}

	if (m_iFileVersion <= 0x0201) {
		ReorderSequences();
		// De-allocate memory
		m_vTmpSequences.clear();		// // //
	}

#ifdef TRANSPOSE_FDS
	if (m_bAdjustFDSArpeggio) {
		int Channel = GetChannelIndex(CHANID_FDS);
		if (Channel != -1) {
			for (unsigned int t = 0; t < GetTrackCount(); ++t) for (int p = 0; p < MAX_PATTERN; ++p) for (int r = 0; r < MAX_PATTERN_LENGTH; ++r) {
				stChanNote Note = GetDataAtPattern(t, p, Channel, r);		// // //
				if (Note.Note >= NOTE_C && Note.Note <= NOTE_B) {
					int Trsp = MIDI_NOTE(Note.Octave, Note.Note) + NOTE_RANGE * 2;
					Trsp = Trsp >= NOTE_COUNT ? NOTE_COUNT - 1 : Trsp;
					Note.Note = GET_NOTE(Trsp);
					Note.Octave = GET_OCTAVE(Trsp);
					SetDataAtPattern(t, p, Channel, r, Note);		// // //
				}
			}
		}
		for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
			if (GetInstrumentType(i) == INST_FDS) {
				CSequence *pSeq = std::static_pointer_cast<CSeqInstrument>(GetInstrument(i))->GetSequence(SEQ_ARPEGGIO);
				if (pSeq != nullptr && pSeq->GetItemCount() > 0 && pSeq->GetSetting() == SETTING_ARP_FIXED)
					for (unsigned int j = 0; j < pSeq->GetItemCount(); ++j) {
						int Trsp = pSeq->GetItem(j) + NOTE_RANGE * 2;
						pSeq->SetItem(j, Trsp >= NOTE_COUNT ? NOTE_COUNT - 1 : Trsp);
					}
			}
		}
	}
#endif

	return TRUE;
}

#define DEFAULT_READ(NAME) \
	void CFamiTrackerDoc::ReadBlock_ ## NAME(CDocumentFile *pDocFile, const int Version) { \
		CFamiTrackerDocIO {*pDocFile}.Load ## NAME(*this, Version); \
	}

DEFAULT_READ(SongInfo)
DEFAULT_READ(Header)
DEFAULT_READ(Instruments)
DEFAULT_READ(Frames)
DEFAULT_READ(DSamples)
DEFAULT_READ(Comments)
DEFAULT_READ(SequencesVRC6)
DEFAULT_READ(SequencesN163)
DEFAULT_READ(SequencesS5B)
DEFAULT_READ(ParamsExtra)
DEFAULT_READ(DetuneTables)
DEFAULT_READ(Grooves)
DEFAULT_READ(Bookmarks)

void CFamiTrackerDoc::ReadBlock_Parameters(CDocumentFile *pDocFile, const int Version)
{
	// Get first track for module versions that require that
	auto &Song = GetSongData(0);

	unsigned Expansion = SNDCHIP_NONE;		// // //

	if (Version == 1)
		Song.SetSongSpeed(pDocFile->GetBlockInt());
	else
		Expansion = pDocFile->GetBlockChar();

	m_iChannelsAvailable = AssertRange(pDocFile->GetBlockInt(), 1, MAX_CHANNELS, "Channel count");		// // //
	AssertRange<MODULE_ERROR_OFFICIAL>(static_cast<int>(m_iChannelsAvailable), 1, MAX_CHANNELS - 1, "Channel count");

	SetMachine(static_cast<machine_t>(pDocFile->GetBlockInt()));
	AssertFileData(m_iMachine == NTSC || m_iMachine == PAL, "Unknown machine");

	if (Version >= 7) {		// // // 050B
		switch (pDocFile->GetBlockInt()) {
		case 1:
			SetEngineSpeed(static_cast<int>(1000000. / pDocFile->GetBlockInt() + .5));
			break;
		case 0: case 2:
		default:
			pDocFile->GetBlockInt();
			SetEngineSpeed(0);
		}
	}
	else
		SetEngineSpeed(pDocFile->GetBlockInt());

	if (Version > 2)
		SetVibratoStyle(pDocFile->GetBlockInt() ? VIBRATO_NEW : VIBRATO_OLD);		// // //
	else
		SetVibratoStyle(VIBRATO_OLD);

	// TODO read m_bLinearPitch
	if (Version >= 9) {		// // // 050B
		bool SweepReset = pDocFile->GetBlockInt() != 0;
	}

	SetHighlight(CSongData::DEFAULT_HIGHLIGHT);		// // //

	if (Version > 3 && Version <= 6) {		// // // 050B
		stHighlight hl;
		hl.First = pDocFile->GetBlockInt();
		hl.Second = pDocFile->GetBlockInt();
		SetHighlight(hl);
	}

	// This is strange. Sometimes expansion chip is set to 0xFF in files
	if (m_iChannelsAvailable == 5)
		Expansion = SNDCHIP_NONE;

	if (m_iFileVersion == 0x0200) {
		int Speed = Song.GetSongSpeed();
		if (Speed < 20)
			Song.SetSongSpeed(Speed + 1);
	}

	if (Version == 1) {
		if (Song.GetSongSpeed() > 19) {
			Song.SetSongTempo(Song.GetSongSpeed());
			Song.SetSongSpeed(6);
		}
		else {
			Song.SetSongTempo(m_iMachine == NTSC ? DEFAULT_TEMPO_NTSC : DEFAULT_TEMPO_PAL);
		}
	}

	// Read namco channel count
	if (Version >= 5 && (Expansion & SNDCHIP_N163))
		m_iNamcoChannels = AssertRange(pDocFile->GetBlockInt(), 1, 8, "N163 channel count");
	else		// // //
		m_iNamcoChannels = 0;

	// Determine if new or old split point is preferred
	SetSpeedSplitPoint(Version >= 6 ? pDocFile->GetBlockInt() : OLD_SPEED_SPLIT_POINT);

	AssertRange<MODULE_ERROR_STRICT>(Expansion, 0u, 0x3Fu, "Expansion chip flag");

	if (Version >= 8) {		// // // 050B
		int semitones = pDocFile->GetBlockChar();
		SetTuning(semitones, pDocFile->GetBlockChar());
	}

	SetupChannels(Expansion);
}

void CFamiTrackerDoc::ReadBlock_Sequences(CDocumentFile *pDocFile, const int Version)
{
	unsigned int Count = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES * SEQ_COUNT, "2A03 sequence count");
	AssertRange<MODULE_ERROR_OFFICIAL>(Count, 0U, static_cast<unsigned>(MAX_SEQUENCES * SEQ_COUNT - 1), "2A03 sequence count");		// // //

	if (Version == 1) {
		for (unsigned int i = 0; i < Count; ++i) {
			COldSequence Seq;
			unsigned int Index = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES - 1, "Sequence index");
			unsigned int SeqCount = static_cast<unsigned char>(pDocFile->GetBlockChar());
			AssertRange(SeqCount, 0U, static_cast<unsigned>(MAX_SEQUENCE_ITEMS - 1), "Sequence item count");
			for (unsigned int j = 0; j < SeqCount; ++j) {
				char Value = pDocFile->GetBlockChar();
				Seq.AddItem(pDocFile->GetBlockChar(), Value);
			}
			m_vTmpSequences.push_back(Seq);		// // //
		}
	}
	else if (Version == 2) {
		for (unsigned int i = 0; i < Count; ++i) {
			COldSequence Seq;		// // //
			unsigned int Index = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES - 1, "Sequence index");
			unsigned int Type = AssertRange(pDocFile->GetBlockInt(), 0, SEQ_COUNT - 1, "Sequence type");
			unsigned int SeqCount = static_cast<unsigned char>(pDocFile->GetBlockChar());
			AssertRange(SeqCount, 0U, static_cast<unsigned>(MAX_SEQUENCE_ITEMS - 1), "Sequence item count");
			for (unsigned int j = 0; j < SeqCount; ++j) {
				char Value = pDocFile->GetBlockChar();
				Seq.AddItem(pDocFile->GetBlockChar(), Value);
			}
			m_pInstrumentManager->SetSequence(INST_2A03, Type, Index, Seq.Convert(Type).release());		// // //
		}
	}
	else if (Version >= 3) {
		CSequenceManager *pManager = GetSequenceManager(INST_2A03);		// // //
		int Indices[MAX_SEQUENCES * SEQ_COUNT];
		int Types[MAX_SEQUENCES * SEQ_COUNT];

		for (unsigned int i = 0; i < Count; ++i) {
			unsigned int Index = Indices[i] = AssertRange(pDocFile->GetBlockInt(), 0, MAX_SEQUENCES - 1, "Sequence index");
			unsigned int Type = Types[i] = AssertRange(pDocFile->GetBlockInt(), 0, SEQ_COUNT - 1, "Sequence type");
			try {
				unsigned char SeqCount = pDocFile->GetBlockChar();
				// AssertRange(SeqCount, 0, MAX_SEQUENCE_ITEMS, "Sequence item count");
				auto pSeq = std::make_unique<CSequence>();
				pSeq->SetItemCount(SeqCount < MAX_SEQUENCE_ITEMS ? SeqCount : MAX_SEQUENCE_ITEMS);

				unsigned int LoopPoint = AssertRange<MODULE_ERROR_STRICT>(
					pDocFile->GetBlockInt(), -1, static_cast<int>(SeqCount), "Sequence loop point");
				// Work-around for some older files
				if (LoopPoint != SeqCount)
					pSeq->SetLoopPoint(LoopPoint);

				if (Version == 4) {
					int ReleasePoint = pDocFile->GetBlockInt();
					int Settings = pDocFile->GetBlockInt();
					pSeq->SetReleasePoint(AssertRange<MODULE_ERROR_STRICT>(
						ReleasePoint, -1, static_cast<int>(SeqCount) - 1, "Sequence release point"));
					pSeq->SetSetting(static_cast<seq_setting_t>(Settings));		// // //
				}

				for (int j = 0; j < SeqCount; ++j) {
					char Value = pDocFile->GetBlockChar();
					if (j < MAX_SEQUENCE_ITEMS)		// // //
						pSeq->SetItem(j, Value);
				}
				pManager->GetCollection(Type)->SetSequence(Index, pSeq.release());
			}
			catch (CModuleException *e) {
				e->AppendError("At 2A03 %s sequence %d,", CInstrument2A03::SEQUENCE_NAME[Type], Index);
				throw;
			}
		}

		if (Version == 5) {
			// Version 5 saved the release points incorrectly, this is fixed in ver 6
			for (unsigned int i = 0; i < MAX_SEQUENCES; ++i) {
				for (int j = 0; j < SEQ_COUNT; ++j) try {
					int ReleasePoint = pDocFile->GetBlockInt();
					int Settings = pDocFile->GetBlockInt();
					CSequence *pSeq = pManager->GetCollection(j)->GetSequence(i);
					int Length = pSeq->GetItemCount();
					if (Length > 0) {
						pSeq->SetReleasePoint(AssertRange<MODULE_ERROR_STRICT>(
							ReleasePoint, -1, Length - 1, "Sequence release point"));
						pSeq->SetSetting(static_cast<seq_setting_t>(Settings));		// // //
					}
				}
				catch (CModuleException *e) {
					e->AppendError("At 2A03 %s sequence %d,", CInstrument2A03::SEQUENCE_NAME[j], i);
					throw;
				}
			}
		}
		else if (Version >= 6) {
			// Read release points correctly stored
			for (unsigned int i = 0; i < Count; ++i) try {
				CSequence *pSeq = pManager->GetCollection(Types[i])->GetSequence(Indices[i]);
				pSeq->SetReleasePoint(AssertRange<MODULE_ERROR_STRICT>(
					pDocFile->GetBlockInt(), -1, static_cast<int>(pSeq->GetItemCount()) - 1, "Sequence release point"));
				pSeq->SetSetting(static_cast<seq_setting_t>(pDocFile->GetBlockInt()));		// // //
			}
			catch (CModuleException *e) {
				e->AppendError("At 2A03 %s sequence %d,", CInstrument2A03::SEQUENCE_NAME[Types[i]], Indices[i]);
				throw;
			}
		}
	}
}

void CFamiTrackerDoc::ReadBlock_Patterns(CDocumentFile *pDocFile, const int Version)
{
#ifdef TRANSPOSE_FDS
	m_bAdjustFDSArpeggio = Version < 5;
#endif

	if (Version == 1) {
		int PatternLen = AssertRange(pDocFile->GetBlockInt(), 0, MAX_PATTERN_LENGTH, "Pattern data count");
		auto &Song = GetSongData(0);
		Song.SetPatternLength(PatternLen);
	}

	while (!pDocFile->BlockDone()) {
		unsigned Track;
		if (Version > 1)
			Track = AssertRange(pDocFile->GetBlockInt(), 0, static_cast<int>(MAX_TRACKS) - 1, "Pattern track index");
		else if (Version == 1)
			Track = 0;

		unsigned Channel = AssertRange(pDocFile->GetBlockInt(), 0, MAX_CHANNELS - 1, "Pattern channel index");
		unsigned Pattern = AssertRange(pDocFile->GetBlockInt(), 0, MAX_PATTERN - 1, "Pattern index");
		unsigned Items	= AssertRange(pDocFile->GetBlockInt(), 0, MAX_PATTERN_LENGTH, "Pattern data count");

		auto &Song = GetSongData(Track);

		for (unsigned i = 0; i < Items; ++i) try {
			unsigned Row;
			if (m_iFileVersion == 0x0200 || Version >= 6)
				Row = static_cast<unsigned char>(pDocFile->GetBlockChar());
			else
				Row = AssertRange(pDocFile->GetBlockInt(), 0, 0xFF, "Row index");		// // //

			try {
				stChanNote Note;		// // //

				Note.Note = AssertRange<MODULE_ERROR_STRICT>(		// // //
					pDocFile->GetBlockChar(), NONE, ECHO, "Note value");
				Note.Octave = AssertRange<MODULE_ERROR_STRICT>(
					pDocFile->GetBlockChar(), 0, OCTAVE_RANGE - 1, "Octave value");
				int Inst = static_cast<unsigned char>(pDocFile->GetBlockChar());
				if (Inst != HOLD_INSTRUMENT)		// // // 050B
					AssertRange<MODULE_ERROR_STRICT>(Inst, 0, m_pInstrumentManager->MAX_INSTRUMENTS, "Instrument index");
				Note.Instrument = Inst;
				Note.Vol = AssertRange<MODULE_ERROR_STRICT>(
					pDocFile->GetBlockChar(), 0, MAX_VOLUME, "Channel volume");

				int FX = m_iFileVersion == 0x200 ? 1 : Version >= 6 ? MAX_EFFECT_COLUMNS :
						 (Song.GetEffectColumnCount(Channel) + 1);		// // // 050B
				for (int n = 0; n < FX; ++n) try {
					unsigned char EffectNumber = pDocFile->GetBlockChar();
					if (Note.EffNumber[n] = static_cast<effect_t>(EffectNumber)) {
						AssertRange<MODULE_ERROR_STRICT>(EffectNumber, EF_NONE, EF_COUNT - 1, "Effect index");
						unsigned char EffectParam = pDocFile->GetBlockChar();
						if (Version < 3) {
							if (EffectNumber == EF_PORTAOFF) {
								EffectNumber = EF_PORTAMENTO;
								EffectParam = 0;
							}
							else if (EffectNumber == EF_PORTAMENTO) {
								if (EffectParam < 0xFF)
									EffectParam++;
							}
						}
						Note.EffParam[n] = EffectParam; // skip on no effect
					}
					else if (Version < 6)
						pDocFile->GetBlockChar(); // unused blank parameter
				}
				catch (CModuleException *e) {
					e->AppendError("At effect column fx%d,", n + 1);
					throw;
				}

	//			if (Note.Vol > MAX_VOLUME)
	//				Note.Vol &= 0x0F;

				// Specific for version 2.0
				if (m_iFileVersion == 0x0200) {

					if (Note.EffNumber[0] == EF_SPEED && Note.EffParam[0] < 20)
						Note.EffParam[0]++;

					if (Note.Vol == 0)
						Note.Vol = MAX_VOLUME;
					else {
						Note.Vol--;
						Note.Vol &= 0x0F;
					}

					if (Note.Note == 0)
						Note.Instrument = MAX_INSTRUMENTS;
				}

				if (ExpansionEnabled(SNDCHIP_N163) && GetChipType(Channel) == SNDCHIP_N163) {		// // //
					for (int n = 0; n < MAX_EFFECT_COLUMNS; ++n)
						if (Note.EffNumber[n] == EF_SAMPLE_OFFSET)
							Note.EffNumber[n] = EF_N163_WAVE_BUFFER;
				}

				if (Version == 3) {
					// Fix for VRC7 portamento
					if (ExpansionEnabled(SNDCHIP_VRC7) && Channel > 4) {
						for (int n = 0; n < MAX_EFFECT_COLUMNS; ++n) {
							switch (Note.EffNumber[n]) {
							case EF_PORTA_DOWN:
								Note.EffNumber[n] = EF_PORTA_UP;
								break;
							case EF_PORTA_UP:
								Note.EffNumber[n] = EF_PORTA_DOWN;
								break;
							}
						}
					}
					// FDS pitch effect fix
					else if (ExpansionEnabled(SNDCHIP_FDS) && GetChannelType(Channel) == CHANID_FDS) {
						for (int n = 0; n < MAX_EFFECT_COLUMNS; ++n) {
							switch (Note.EffNumber[n]) {
							case EF_PITCH:
								if (Note.EffParam[n] != 0x80)
									Note.EffParam[n] = (0x100 - Note.EffParam[n]) & 0xFF;
								break;
							}
						}
					}
				}

				if (m_iFileVersion < 0x450) {		// // // 050B
					for (auto &x : Note.EffNumber)
						if (x < EF_COUNT)
							x = EFF_CONVERSION_050.first[x];
				}
				/*
				if (Version < 6) {
					// Noise pitch slide fix
					if (GetChannelType(Channel) == CHANID_NOISE) {
						for (int n = 0; n < MAX_EFFECT_COLUMNS; ++n) {
							switch (Note.EffNumber[n]) {
								case EF_PORTA_DOWN:
									Note.EffNumber[n] = EF_PORTA_UP;
									Note.EffParam[n] = Note.EffParam[n] << 4;
									break;
								case EF_PORTA_UP:
									Note.EffNumber[n] = EF_PORTA_DOWN;
									Note.EffParam[n] = Note.EffParam[n] << 4;
									break;
								case EF_PORTAMENTO:
									Note.EffParam[n] = Note.EffParam[n] << 4;
									break;
								case EF_SLIDE_UP:
									Note.EffParam[n] = Note.EffParam[n] + 0x70;
									break;
								case EF_SLIDE_DOWN:
									Note.EffParam[n] = Note.EffParam[n] + 0x70;
									break;
							}
						}
					}
				}
				*/

				Song.SetPatternData(Channel, Pattern, Row, Note);		// // //
			}
			catch (CModuleException *e) {
				e->AppendError("At row %02X,", Row);
				throw;
			}
		}
		catch (CModuleException *e) {
			e->AppendError("At pattern %02X, channel %d, track %d,", Pattern, Channel, Track + 1);
			throw;
		}
	}
}

// FTM import ////

CFamiTrackerDoc *CFamiTrackerDoc::LoadImportFile(LPCTSTR lpszPathName) const
{
	// Import a module as new subtunes
	CFamiTrackerDoc *pImported = new CFamiTrackerDoc();

	pImported->DeleteContents();

	// Load into a new document
	if (!pImported->OpenDocument(lpszPathName))
		SAFE_RELEASE(pImported);

	return pImported;
}

bool CFamiTrackerDoc::ImportInstruments(CFamiTrackerDoc *pImported, int *pInstTable)
{
	// Copy instruments to current module
	//
	// pInstTable must point to an int array of size MAX_INSTRUMENTS
	//

	int SamplesTable[MAX_DSAMPLES];
	int SequenceTable2A03[MAX_SEQUENCES][SEQ_COUNT];
	int SequenceTableVRC6[MAX_SEQUENCES][SEQ_COUNT];
	int SequenceTableN163[MAX_SEQUENCES][SEQ_COUNT];
	int SequenceTableS5B[MAX_SEQUENCES][SEQ_COUNT];		// // //

	memset(SamplesTable, 0, sizeof(int) * MAX_DSAMPLES);
	memset(SequenceTable2A03, 0, sizeof(int) * MAX_SEQUENCES * SEQ_COUNT);
	memset(SequenceTableVRC6, 0, sizeof(int) * MAX_SEQUENCES * SEQ_COUNT);
	memset(SequenceTableN163, 0, sizeof(int) * MAX_SEQUENCES * SEQ_COUNT);
	memset(SequenceTableS5B, 0, sizeof(int) * MAX_SEQUENCES * SEQ_COUNT);		// // //

	// Check instrument count
	if (GetInstrumentCount() + pImported->GetInstrumentCount() > MAX_INSTRUMENTS) {
		// Out of instrument slots
		AfxMessageBox(IDS_IMPORT_INSTRUMENT_COUNT, MB_ICONERROR);
		return false;
	}

	static const inst_type_t inst[] = {INST_2A03, INST_VRC6, INST_N163, INST_S5B};		// // //
	static const uint8_t chip[] = {SNDCHIP_NONE, SNDCHIP_VRC6, SNDCHIP_N163, SNDCHIP_S5B};
	int (*seqTable[])[SEQ_COUNT] = {SequenceTable2A03, SequenceTableVRC6, SequenceTableN163, SequenceTableS5B};

	// Copy sequences
	for (size_t i = 0; i < sizeof(chip); i++) for (int t = 0; t < SEQ_COUNT; ++t) {
		if (GetSequenceCount(inst[i], t) + pImported->GetSequenceCount(inst[i], t) > MAX_SEQUENCES) {		// // //
			AfxMessageBox(IDS_IMPORT_SEQUENCE_COUNT, MB_ICONERROR);
			return false;
		}
		for (unsigned int s = 0; s < MAX_SEQUENCES; ++s) if (pImported->GetSequenceItemCount(inst[i], s, t) > 0) {
			CSequence *pImportSeq = pImported->GetSequence(inst[i], s, t);
			int index = -1;
			for (unsigned j = 0; j < MAX_SEQUENCES; ++j) {
				if (GetSequenceItemCount(inst[i], j, t)) continue;
				// TODO: continue if blank sequence is used by some instrument
				CSequence *pSeq = GetSequence(inst[i], j, t);
				pSeq->Copy(pImportSeq);
				// Save a reference to this sequence
				seqTable[i][s][t] = j;
				break;
			}
		}
	}

	bool bOutOfSampleSpace = false;

	// Copy DPCM samples
	for (int i = 0; i < MAX_DSAMPLES; ++i) {
		if (const CDSample *pImportDSample = pImported->GetSample(i)) {		// // //
			int Index = GetFreeSampleSlot();
			if (Index != -1) {
				CDSample *pDSample = new CDSample(*pImportDSample);		// // //
				SetSample(Index, pDSample);
				// Save a reference to this DPCM sample
				SamplesTable[i] = Index;
			}
			else
				bOutOfSampleSpace = true;
		}
	}

	if (bOutOfSampleSpace) {
		// Out of sample space
		AfxMessageBox(IDS_IMPORT_SAMPLE_SLOTS, MB_ICONEXCLAMATION);
		return false;
	}

	// Copy instruments
	for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
		if (pImported->IsInstrumentUsed(i)) {
			auto pInst = std::unique_ptr<CInstrument>(pImported->GetInstrument(i)->Clone());		// // //

			// Update references
			if (auto pSeq = dynamic_cast<CSeqInstrument *>(pInst.get())) {
				for (int t = 0; t < SEQ_COUNT; ++t)
					if (pSeq->GetSeqEnable(t)) {
						for (size_t j = 0; j < sizeof(chip); j++)
							if (inst[j] == pInst->GetType()) {
								pSeq->SetSeqIndex(t, seqTable[j][pSeq->GetSeqIndex(t)][t]);
								break;
							}
					}
				// Update DPCM samples
				if (auto p2A03 = dynamic_cast<CInstrument2A03 *>(pSeq))
					for (int o = 0; o < OCTAVE_RANGE; ++o) for (int n = 0; n < NOTE_RANGE; ++n) {
						int Sample = p2A03->GetSampleIndex(o, n);
						if (Sample != 0)
							p2A03->SetSampleIndex(o, n, SamplesTable[Sample - 1] + 1);
					}
			}

			int Index = GetFreeInstrumentIndex();
			AddInstrument(std::move(pInst), Index);		// // //
			// Save a reference to this instrument
			pInstTable[i] = Index;
		}
	}

	return true;
}

bool CFamiTrackerDoc::ImportGrooves(CFamiTrackerDoc *pImported, int *pGrooveMap)		// // //
{
	int Index = 0;
	for (int i = 0; i < MAX_GROOVE; i++) {
		if (pImported->GetGroove(i) != NULL) {
			while (GetGroove(Index) != NULL) Index++;
			if (Index >= MAX_GROOVE) {
				AfxMessageBox(IDS_IMPORT_GROOVE_SLOTS, MB_ICONEXCLAMATION);
				return false;
			}
			pGrooveMap[i] = Index;
			m_pGrooveTable[Index] = std::make_unique<CGroove>();
			m_pGrooveTable[Index]->Copy(pImported->GetGroove(i));
		}
	}

	return true;
}

bool CFamiTrackerDoc::ImportDetune(CFamiTrackerDoc *pImported)		// // //
{
	for (int i = 0; i < 6; i++) for (int j = 0; j < NOTE_COUNT; j++)
		m_iDetuneTable[i][j] = pImported->GetDetuneOffset(i, j);

	theApp.GetSoundGenerator()->LoadMachineSettings();		// // //
	return true;
}

bool CFamiTrackerDoc::ImportTrack(int Track, CFamiTrackerDoc *pImported, int *pInstTable, int *pGrooveMap)		// // //
{
	// Import a selected track from specified source document

	int NewTrack = AddTrack();

	if (NewTrack == -1)
		return false;

	// Copy parameters
	SetPatternLength(NewTrack, pImported->GetPatternLength(Track));
	SetFrameCount(NewTrack, pImported->GetFrameCount(Track));
	SetSongTempo(NewTrack, pImported->GetSongTempo(Track));
	SetSongGroove(NewTrack, pImported->GetSongGroove(Track));
	if (GetSongGroove(NewTrack))
		SetSongSpeed(NewTrack, pGrooveMap[pImported->GetSongSpeed(Track)]);
	else
		SetSongSpeed(NewTrack, pImported->GetSongSpeed(Track));

	// Copy track name
	SetTrackTitle(NewTrack, pImported->GetTrackTitle(Track));

	// Copy frames
	for (unsigned int f = 0; f < pImported->GetFrameCount(Track); ++f) {
		for (unsigned int c = 0; c < GetAvailableChannels(); ++c) {
			SetPatternAtFrame(NewTrack, f, c, pImported->GetPatternAtFrame(Track, f, c));
		}
	}

	// // // Copy bookmarks
	m_pBookmarkManager->SetCollection(NewTrack, pImported->GetBookmarkManager()->PopCollection(Track));

	// Copy patterns
	for (unsigned int p = 0; p < MAX_PATTERN; ++p) {
		for (unsigned int c = 0; c < GetAvailableChannels(); ++c) {
			for (unsigned int r = 0; r < pImported->GetPatternLength(Track); ++r) {
				// Get note
				stChanNote data = pImported->GetDataAtPattern(Track, p, c, r);		// // //
				// Translate instrument number
				if (data.Instrument < MAX_INSTRUMENTS)
					data.Instrument = pInstTable[data.Instrument];
				for (int i = 0; i < MAX_EFFECT_COLUMNS; i++)		// // //
					if (data.EffNumber[i] == EF_GROOVE && data.EffParam[i] < MAX_GROOVE)
						data.EffParam[i] = pGrooveMap[data.EffParam[i]];
				// Store
				SetDataAtPattern(NewTrack, p, c, r, data);		// // //
			}
		}
	}

	// Effect columns
	for (unsigned int c = 0; c < GetAvailableChannels(); ++c) {
		SetEffColumns(NewTrack, c, pImported->GetEffColumns(Track, c));
	}

	return true;
}

// End of file load/save

// DMC Stuff

const CDSample *CFamiTrackerDoc::GetSample(unsigned int Index) const
{
	ASSERT(Index < MAX_DSAMPLES);
	return m_pInstrumentManager->GetDSampleManager()->GetDSample(Index);		// // //
}

void CFamiTrackerDoc::SetSample(unsigned int Index, CDSample *pSamp)		// // //
{
	ASSERT(Index < MAX_DSAMPLES);
	if (m_pInstrumentManager->GetDSampleManager()->SetDSample(Index, pSamp)) {
		ModifyIrreversible();		// // //
	}
}

bool CFamiTrackerDoc::IsSampleUsed(unsigned int Index) const
{
	ASSERT(Index < MAX_DSAMPLES);
	return m_pInstrumentManager->GetDSampleManager()->IsSampleUsed(Index);		// // //
}

unsigned int CFamiTrackerDoc::GetSampleCount() const
{
	return m_pInstrumentManager->GetDSampleManager()->GetSampleCount();
}

int CFamiTrackerDoc::GetFreeSampleSlot() const
{
	return m_pInstrumentManager->GetDSampleManager()->GetFirstFree();
}

void CFamiTrackerDoc::RemoveSample(unsigned int Index)
{
	SetSample(Index, nullptr);		// // //
}

unsigned int CFamiTrackerDoc::GetTotalSampleSize() const
{
	return m_pInstrumentManager->GetDSampleManager()->GetTotalSize();
}

// ---------------------------------------------------------------------------------------------------------
// Document access functions
// ---------------------------------------------------------------------------------------------------------

//
// Sequences
//

CSequence *CFamiTrackerDoc::GetSequence(inst_type_t InstType, unsigned int Index, int Type) const		// // //
{
	return m_pInstrumentManager->GetSequence(InstType, Type, Index);
}

unsigned int CFamiTrackerDoc::GetSequenceItemCount(inst_type_t InstType, unsigned int Index, int Type) const		// // //
{
	ASSERT(Index < MAX_SEQUENCES);
	ASSERT(Type >= 0 && Type < SEQ_COUNT);

	const CSequence *pSeq = GetSequence(InstType, Index, Type);
	if (pSeq == NULL)
		return 0;
	return pSeq->GetItemCount();
}

int CFamiTrackerDoc::GetFreeSequence(inst_type_t InstType, int Type, CSeqInstrument *pInst) const		// // //
{
	ASSERT(Type >= 0 && Type < SEQ_COUNT);
	return m_pInstrumentManager->GetFreeSequenceIndex(InstType, Type, pInst);
}

int CFamiTrackerDoc::GetSequenceCount(inst_type_t InstType, int Type) const		// // //
{
	// Return number of allocated sequences of Type
	ASSERT(Type >= 0 && Type < SEQ_COUNT);

	int Count = 0;
	for (int i = 0; i < MAX_SEQUENCES; ++i) {
		if (GetSequenceItemCount(InstType, i, Type) > 0) // TODO: fix this and the instrument interface
			++Count;
	}
	return Count;
}

int CFamiTrackerDoc::GetTotalSequenceCount(inst_type_t InstType) const {		// // //
	int Count = 0;
	for (int i = 0; i < SEQ_COUNT; ++i)
		Count += GetSequenceCount(InstType, i);
	return Count;
}

//
// Song info
//

std::string_view CFamiTrackerDoc::GetModuleName() const		// // //
{
	return m_strName; 
}

std::string_view CFamiTrackerDoc::GetModuleArtist() const
{ 
	return m_strArtist; 
}

std::string_view CFamiTrackerDoc::GetModuleCopyright() const
{ 
	return m_strCopyright; 
}

void CFamiTrackerDoc::SetModuleName(std::string_view pName)
{
	pName = pName.substr(0, METADATA_FIELD_LENGTH - 1);		// // //
	if (m_strName != pName) {
		m_strName = pName;
//		ModifyIrreversible();
	}
}

void CFamiTrackerDoc::SetModuleArtist(std::string_view pArtist)
{
	pArtist = pArtist.substr(0, METADATA_FIELD_LENGTH - 1);		// // //
	if (m_strArtist != pArtist) {
		m_strArtist = pArtist;
//		ModifyIrreversible();
	}
}

void CFamiTrackerDoc::SetModuleCopyright(std::string_view pCopyright)
{
	pCopyright = pCopyright.substr(0, METADATA_FIELD_LENGTH - 1);		// // //
	if (m_strCopyright != pCopyright) {
		m_strCopyright = pCopyright;
//		ModifyIrreversible();
	}
}

//
// Instruments
//

std::shared_ptr<CInstrument> CFamiTrackerDoc::GetInstrument(unsigned int Index) const
{
	return m_pInstrumentManager->GetInstrument(Index);
}

unsigned int CFamiTrackerDoc::GetInstrumentCount() const
{
	return m_pInstrumentManager->GetInstrumentCount();
}

unsigned CFamiTrackerDoc::GetFreeInstrumentIndex() const {		// // //
	return m_pInstrumentManager->GetFirstUnused();
}

bool CFamiTrackerDoc::IsInstrumentUsed(unsigned int Index) const
{
	return m_pInstrumentManager->IsInstrumentUsed(Index);
}

bool CFamiTrackerDoc::AddInstrument(std::unique_ptr<CInstrument> pInstrument, unsigned int Slot)		// // //
{
	return m_pInstrumentManager->InsertInstrument(Slot, std::move(pInstrument));
}

bool CFamiTrackerDoc::RemoveInstrument(unsigned int Index)		// // //
{
	return m_pInstrumentManager->RemoveInstrument(Index);
}

int CFamiTrackerDoc::CloneInstrument(unsigned int Index)
{
	if (!IsInstrumentUsed(Index))
		return INVALID_INSTRUMENT;

	const int Slot = m_pInstrumentManager->GetFirstUnused();

	if (Slot != INVALID_INSTRUMENT) {
		auto pInst = std::unique_ptr<CInstrument>(m_pInstrumentManager->GetInstrument(Index)->Clone());
		if (!AddInstrument(std::move(pInst), Slot))		// // //
			return INVALID_INSTRUMENT;
	}

	return Slot;
}

inst_type_t CFamiTrackerDoc::GetInstrumentType(unsigned int Index) const
{
	return m_pInstrumentManager->GetInstrumentType(Index);
}

int CFamiTrackerDoc::DeepCloneInstrument(unsigned int Index) 
{
	int Slot = CloneInstrument(Index);

	if (Slot != INVALID_INSTRUMENT) {
		auto newInst = m_pInstrumentManager->GetInstrument(Slot);
		const inst_type_t it = newInst->GetType();
		if (auto pInstrument = std::dynamic_pointer_cast<CSeqInstrument>(newInst)) {
			for (int i = 0; i < SEQ_COUNT; i++) {
				int freeSeq = m_pInstrumentManager->GetFreeSequenceIndex(it, i, pInstrument.get());
				if (freeSeq != -1) {
					if (pInstrument->GetSeqEnable(i))
						GetSequence(it, unsigned(freeSeq), i)->Copy(pInstrument->GetSequence(i));
					pInstrument->SetSeqIndex(i, freeSeq);
				}
			}
		}
	}

	return Slot;
}

void CFamiTrackerDoc::SaveInstrument(unsigned int Index, CSimpleFile &file) const
{
	GetInstrument(Index)->SaveFTI(file);
}

int CFamiTrackerDoc::LoadInstrument(CString FileName)
{
	// FTI instruments files
	static const char INST_HEADER[] = "FTI";
	static const char INST_VERSION[] = "2.4";

	// Loads an instrument from file, return allocated slot or INVALID_INSTRUMENT if failed
	//
	int iInstMaj, iInstMin;
	// // // sscanf_s(INST_VERSION, "%i.%i", &iInstMaj, &iInstMin);
	static const int I_CURRENT_VER = 2 * 10 + 5;		// // // 050B
	
	int Slot = m_pInstrumentManager->GetFirstUnused();
	try {
		if (Slot == INVALID_INSTRUMENT)
			throw IDS_INST_LIMIT;

		// Open file
		// // // CFile implements RAII
		CSimpleFile file(FileName, std::ios::in | std::ios::binary);
		if (!file)
			throw IDS_FILE_OPEN_ERROR;

		// Signature
		const UINT HEADER_LEN = strlen(INST_HEADER);
		char Text[256] = {};
		file.ReadBytes(Text, HEADER_LEN);
		if (strcmp(Text, INST_HEADER) != 0)
			throw IDS_INSTRUMENT_FILE_FAIL;
		
		// Version
		file.ReadBytes(Text, static_cast<UINT>(strlen(INST_VERSION)));
		sscanf_s(Text, "%i.%i", &iInstMaj, &iInstMin);		// // //
		int iInstVer = iInstMaj * 10 + iInstMin;
		if (iInstVer > I_CURRENT_VER)
			throw IDS_INST_VERSION_UNSUPPORTED;
		
		m_csDocumentLock.Lock();

		inst_type_t InstType = static_cast<inst_type_t>(file.ReadChar());
		if (InstType == INST_NONE)
			InstType = INST_2A03;
		auto pInstrument = m_pInstrumentManager->CreateNew(InstType);
		AssertFileData(pInstrument.get() != nullptr, "Failed to create instrument");
		
		// Name
		std::string InstName = file.ReadString();
		AssertRange(InstName.size(), 0U, static_cast<unsigned>(CInstrument::INST_NAME_MAX), "Instrument name length");
		pInstrument->SetName(InstName.c_str());

		pInstrument->LoadFTI(file, iInstVer);		// // //
		m_pInstrumentManager->InsertInstrument(Slot, std::move(pInstrument));
		m_csDocumentLock.Unlock();
		return Slot;
	}
	catch (int ID) {		// // // TODO: put all error messages into string table then add exception ctor
		m_csDocumentLock.Unlock();
		AfxMessageBox(ID, MB_ICONERROR);
		return INVALID_INSTRUMENT;
	}
	catch (CModuleException *e) {
		m_csDocumentLock.Unlock();
		m_pInstrumentManager->RemoveInstrument(Slot);
		AfxMessageBox(e->GetErrorString().c_str(), MB_ICONERROR);
		delete e;
		return INVALID_INSTRUMENT;
	}
}

//
// // // General document
//

void CFamiTrackerDoc::SetFrameCount(unsigned int Track, unsigned int Count)
{
	ASSERT(Track < MAX_TRACKS);
	ASSERT(Count <= MAX_FRAMES);

	auto &Song = GetSongData(Track);
	unsigned int Old = Song.GetFrameCount();
	if (Old != Count) {
		Song.SetFrameCount(Count);
		if (Count < Old)
			m_pBookmarkManager->GetCollection(Track)->RemoveFrames(Count, Old - Count); // TODO: don't
		SetExceededFlag();			// // // TODO: is this needed?
	}
}

void CFamiTrackerDoc::SetPatternLength(unsigned int Track, unsigned int Length)
{ 
	ASSERT(Length <= MAX_PATTERN_LENGTH);

	auto &Song = GetSongData(Track);
	Song.SetPatternLength(Length);
}

void CFamiTrackerDoc::SetSongSpeed(unsigned int Track, unsigned int Speed)
{
	auto &Song = GetSongData(Track);
	if (Song.GetSongGroove())		// // //
		ASSERT(Speed < MAX_GROOVE);
	else
		ASSERT(Speed <= MAX_TEMPO);

	Song.SetSongSpeed(Speed);
}

void CFamiTrackerDoc::SetSongTempo(unsigned int Track, unsigned int Tempo)
{
	ASSERT(Tempo <= MAX_TEMPO);

	auto &Song = GetSongData(Track);
	Song.SetSongTempo(Tempo);
}

void CFamiTrackerDoc::SetSongGroove(unsigned int Track, bool Groove)		// // //
{
	auto &Song = GetSongData(Track);
	Song.SetSongGroove(Groove);
}

unsigned int CFamiTrackerDoc::GetPatternLength(unsigned int Track) const
{ 
	ASSERT(Track < MAX_TRACKS);
	return GetSongData(Track).GetPatternLength(); 
}

unsigned int CFamiTrackerDoc::GetCurrentPatternLength(unsigned int Track, int Frame) const		// // //
{ 
	if (theApp.GetSettings()->General.bShowSkippedRows)		// // //
		return GetPatternLength(Track);

	int Frames = GetFrameCount(Track);
	Frame %= Frames;
	if (Frame < 0) Frame += Frames;
	return GetFrameLength(Track, Frame);
}

unsigned int CFamiTrackerDoc::GetFrameCount(unsigned int Track) const 
{ 
	return GetSongData(Track).GetFrameCount(); 
}

unsigned int CFamiTrackerDoc::GetSongSpeed(unsigned int Track) const
{ 
	return GetSongData(Track).GetSongSpeed(); 
}

unsigned int CFamiTrackerDoc::GetSongTempo(unsigned int Track) const
{ 
	return GetSongData(Track).GetSongTempo(); 
}

bool CFamiTrackerDoc::GetSongGroove(unsigned int Track) const		// // //
{ 
	return GetSongData(Track).GetSongGroove();
}

unsigned int CFamiTrackerDoc::GetEffColumns(unsigned int Track, unsigned int Channel) const
{
	ASSERT(Channel < MAX_CHANNELS);
	return GetSongData(Track).GetEffectColumnCount(Channel);
}

void CFamiTrackerDoc::SetEffColumns(unsigned int Track, unsigned int Channel, unsigned int Columns)
{
	ASSERT(Track < MAX_TRACKS);
	ASSERT(Channel < MAX_CHANNELS);
	ASSERT(Columns < MAX_EFFECT_COLUMNS);

	GetSongData(Track).SetEffectColumnCount(Channel, Columns);
}

void CFamiTrackerDoc::SetEngineSpeed(unsigned int Speed)
{
	ASSERT(Speed >= 16 || Speed == 0);		// // //
	m_iEngineSpeed = Speed;
}

void CFamiTrackerDoc::SetMachine(machine_t Machine)
{
	ASSERT(Machine == PAL || Machine == NTSC);
	m_iMachine = Machine;
}

unsigned int CFamiTrackerDoc::GetPatternAtFrame(unsigned int Track, unsigned int Frame, unsigned int Channel) const
{
	ASSERT(Frame < MAX_FRAMES && Channel < MAX_CHANNELS);
	return GetSongData(Track).GetFramePattern(Frame, Channel);
}

void CFamiTrackerDoc::SetPatternAtFrame(unsigned int Track, unsigned int Frame, unsigned int Channel, unsigned int Pattern)
{
	ASSERT(Frame < MAX_FRAMES);
	ASSERT(Channel < MAX_CHANNELS);
	ASSERT(Pattern < MAX_PATTERN);

	GetSongData(Track).SetFramePattern(Frame, Channel, Pattern);
}

unsigned int CFamiTrackerDoc::GetFrameRate() const
{
	if (m_iEngineSpeed == 0)
		return (m_iMachine == NTSC) ? CAPU::FRAME_RATE_NTSC : CAPU::FRAME_RATE_PAL;
	
	return m_iEngineSpeed;
}

//// Pattern functions ////////////////////////////////////////////////////////////////////////////////

void CFamiTrackerDoc::SetNoteData(unsigned Track, unsigned Frame, unsigned Channel, unsigned Row, const stChanNote &Data)		// // //
{
	GetSongData(Track).GetPatternOnFrame(Channel, Frame).SetNoteOn(Row, Data);		// // //
}

const stChanNote &CFamiTrackerDoc::GetNoteData(unsigned Track, unsigned Frame, unsigned Channel, unsigned Row) const
{
	return GetSongData(Track).GetPatternOnFrame(Channel, Frame).GetNoteOn(Row);		// // //
}

stChanNote CFamiTrackerDoc::GetActiveNote(unsigned Track, unsigned Frame, unsigned Channel, unsigned Row) const {		// // //
	auto Note = GetNoteData(Track, Frame, Channel, Row);
	for (int i = GetEffColumns(Track, Channel) + 1; i < MAX_EFFECT_COLUMNS; ++i)
		Note.EffNumber[i] = EF_NONE;
	return Note;
}

void CFamiTrackerDoc::SetDataAtPattern(unsigned Track, unsigned Pattern, unsigned Channel, unsigned Row, const stChanNote &Data)		// // //
{
	// Set a note to a direct pattern
	GetSongData(Track).SetPatternData(Channel, Pattern, Row, Data);		// // //
}

const stChanNote &CFamiTrackerDoc::GetDataAtPattern(unsigned Track, unsigned Pattern, unsigned Channel, unsigned Row) const		// // //
{
	// Get note from a direct pattern
	return GetSongData(Track).GetPatternData(Channel, Pattern, Row);		// // //
}

bool CFamiTrackerDoc::InsertRow(unsigned int Track, unsigned int Frame, unsigned int Channel, unsigned int Row)
{
	auto &Song = GetSongData(Track);
	auto &Pattern = Song.GetPatternOnFrame(Channel, Frame);		// // //

	for (unsigned int i = Song.GetPatternLength() - 1; i > Row; --i)
		Pattern.SetNoteOn(i, Pattern.GetNoteOn(i - 1));
	Pattern.SetNoteOn(Row, { });

	return true;
}

void CFamiTrackerDoc::ClearPatterns(unsigned int Track)
{
	GetSongData(Track).ClearEverything();		// // //
}

void CFamiTrackerDoc::ClearPattern(unsigned int Track, unsigned int Frame, unsigned int Channel)
{
	// Clear entire pattern
	ASSERT(Track < MAX_TRACKS);
	ASSERT(Frame < MAX_FRAMES);
	ASSERT(Channel < MAX_CHANNELS);

	auto &Song = GetSongData(Track);
	int Pattern = Song.GetFramePattern(Frame, Channel);
	Song.ClearPattern(Channel, Pattern);
}

bool CFamiTrackerDoc::ClearRowField(unsigned int Track, unsigned int Frame, unsigned int Channel, unsigned int Row, cursor_column_t Column)
{
	ASSERT(Frame < MAX_FRAMES);
	ASSERT(Channel < MAX_CHANNELS);
	ASSERT(Row < MAX_PATTERN_LENGTH);

	auto &Song = GetSongData(Track);
	int Pattern = Song.GetFramePattern(Frame, Channel);
	stChanNote &Note = Song.GetPatternData(Channel, Pattern, Row);		// // //

	switch (Column) {
		case C_NOTE:			// Note
			Note.Note = NONE;
			Note.Octave = 0;
			Note.Instrument = MAX_INSTRUMENTS;	// Fix the old behaviour
			Note.Vol = MAX_VOLUME;
			break;
		case C_INSTRUMENT1:		// Instrument
		case C_INSTRUMENT2:
			Note.Instrument = MAX_INSTRUMENTS;
			break;
		case C_VOLUME:			// Volume
			Note.Vol = MAX_VOLUME;
			break;
		case C_EFF1_NUM:			// Effect 1
		case C_EFF1_PARAM1:
		case C_EFF1_PARAM2:
			Note.EffNumber[0] = EF_NONE;
			Note.EffParam[0] = 0;
			break;
		case C_EFF2_NUM:		// Effect 2
		case C_EFF2_PARAM1:
		case C_EFF2_PARAM2:
			Note.EffNumber[1] = EF_NONE;
			Note.EffParam[1] = 0;
			break;
		case C_EFF3_NUM:		// Effect 3
		case C_EFF3_PARAM1:
		case C_EFF3_PARAM2:
			Note.EffNumber[2] = EF_NONE;
			Note.EffParam[2] = 0;
			break;
		case C_EFF4_NUM:		// Effect 4
		case C_EFF4_PARAM1:
		case C_EFF4_PARAM2:
			Note.EffNumber[3] = EF_NONE;
			Note.EffParam[3] = 0;
			break;
	}

	return true;
}

bool CFamiTrackerDoc::RemoveNote(unsigned int Track, unsigned int Frame, unsigned int Channel, unsigned int Row)
{
	ASSERT(Row < MAX_PATTERN_LENGTH);

	auto &Song = GetSongData(Track);
	int Pattern = Song.GetFramePattern(Frame, Channel);

	unsigned int PatternLen = Song.GetPatternLength();

	for (unsigned int i = Row - 1; i < (PatternLen - 1); ++i)
		SetDataAtPattern(Track, Pattern, Channel, i,
			GetDataAtPattern(Track, Pattern, Channel, i + 1));		// // //
	SetDataAtPattern(Track, Pattern, Channel, PatternLen - 1, { });		// // //

	return true;
}

bool CFamiTrackerDoc::PullUp(unsigned int Track, unsigned int Frame, unsigned int Channel, unsigned int Row)
{
	auto &Song = GetSongData(Track);		// // //
	auto &Pattern = Song.GetPatternOnFrame(Channel, Frame);
	int PatternLen = Song.GetPatternLength();

	for (int i = Row; i < PatternLen - 1; ++i)
		Pattern.SetNoteOn(i, Pattern.GetNoteOn(i + 1));		// // //
	Pattern.SetNoteOn(PatternLen - 1, { });

	return true;
}

void CFamiTrackerDoc::CopyPattern(unsigned int Track, int Target, int Source, int Channel)
{
	// Copy one pattern to another

	auto &Song = GetSongData(Track);
	Song.GetPattern(Channel, Target) = Song.GetPattern(Channel, Source);		// // //
}

void CFamiTrackerDoc::SwapChannels(unsigned int Track, unsigned int First, unsigned int Second)		// // //
{
	ASSERT(First < MAX_CHANNELS);
	ASSERT(Second < MAX_CHANNELS);

	GetSongData(Track).SwapChannels(First, Second);
}

//// Frame functions //////////////////////////////////////////////////////////////////////////////////

bool CFamiTrackerDoc::InsertFrame(unsigned int Track, unsigned int Frame)
{
	ASSERT(Track < MAX_TRACKS);
	ASSERT(Frame < MAX_FRAMES);

	if (!AddFrames(Track, Frame, 1))
		return false;
	// Select free patterns 
	for (int i = 0, Channels = GetChannelCount(); i < Channels; ++i) {
		unsigned Pattern = GetFirstFreePattern(Track, i);		// // //
		SetPatternAtFrame(Track, Frame, i, Pattern == -1 ? 0 : Pattern);
	}

	return true;
}

bool CFamiTrackerDoc::RemoveFrame(unsigned int Track, unsigned int Frame)
{
	ASSERT(Track < MAX_TRACKS);
	ASSERT(Frame < MAX_FRAMES);

	const int FrameCount = GetFrameCount(Track);
	const int Channels = GetAvailableChannels();

	if (FrameCount == 1)
		return false;

	for (int i = Frame; i < FrameCount - 1; ++i)
		for (int j = 0; j < Channels; ++j)
			SetPatternAtFrame(Track, i, j, GetPatternAtFrame(Track, i + 1, j));

	for (int i = 0; i < Channels; ++i)
		SetPatternAtFrame(Track, FrameCount - 1, i, 0);		// // //
	
	m_pBookmarkManager->GetCollection(Track)->RemoveFrames(Frame, 1U);		// // //

	SetFrameCount(Track, FrameCount - 1);

	return true;
}

bool CFamiTrackerDoc::DuplicateFrame(unsigned int Track, unsigned int Frame)
{
	// Create a copy of selected frame
	ASSERT(Track < MAX_TRACKS);
	ASSERT(Frame < MAX_FRAMES);

	const int Frames = GetFrameCount(Track);
	const int Channels = GetAvailableChannels();

	if (Frames == MAX_FRAMES)
		return false;

	SetFrameCount(Track, Frames + 1);

	for (unsigned int i = Frames; i > (Frame + 1); --i)
		for (int j = 0; j < Channels; ++j)
			SetPatternAtFrame(Track, i, j, GetPatternAtFrame(Track, i - 1, j));

	for (int i = 0; i < Channels; ++i) 
		SetPatternAtFrame(Track, Frame + 1, i, GetPatternAtFrame(Track, Frame, i));

	m_pBookmarkManager->GetCollection(Track)->InsertFrames(Frame + 1, 1U);		// // //

	return true;
}

bool CFamiTrackerDoc::CloneFrame(unsigned int Track, unsigned int Frame)		// // // renamed
{
	ASSERT(Track < MAX_TRACKS);

	// Create a copy of selected frame including patterns
	int Frames = GetFrameCount(Track);
	int Channels = GetAvailableChannels();

	// insert new frame with next free pattern numbers
	if (!InsertFrame(Track, Frame))
		return false;

	// copy old patterns into new
	auto &Song = GetSongData(Track);		// / ///
	for (int i = 0; i < Channels; ++i)
		Song.GetPattern(i, Song.GetFramePattern(Frame, i)) = Song.GetPattern(i, Song.GetFramePattern(Frame - 1, i));

	return true;
}

bool CFamiTrackerDoc::MoveFrameDown(unsigned int Track, unsigned int Frame)
{
	int Channels = GetAvailableChannels();

	if (Frame == (GetFrameCount(Track) - 1))
		return false;

	for (int i = 0; i < Channels; ++i) {
		int Pattern = GetPatternAtFrame(Track, Frame, i);
		SetPatternAtFrame(Track, Frame, i, GetPatternAtFrame(Track, Frame + 1, i));
		SetPatternAtFrame(Track, Frame + 1, i, Pattern);
	}

	m_pBookmarkManager->GetCollection(Track)->SwapFrames(Frame, Frame + 1);		// // //

	return true;
}

bool CFamiTrackerDoc::MoveFrameUp(unsigned int Track, unsigned int Frame)
{
	int Channels = GetAvailableChannels();

	if (Frame == 0)
		return false;

	for (int i = 0; i < Channels; ++i) {
		int Pattern = GetPatternAtFrame(Track, Frame, i);
		SetPatternAtFrame(Track, Frame, i, GetPatternAtFrame(Track, Frame - 1, i));
		SetPatternAtFrame(Track, Frame - 1, i, Pattern);
	}
	
	m_pBookmarkManager->GetCollection(Track)->SwapFrames(Frame, Frame - 1);		// // //

	return true;
}

bool CFamiTrackerDoc::AddFrames(unsigned int Track, unsigned int Frame, int Count)		// // //
{
	ASSERT(Track < MAX_TRACKS);
	ASSERT(Frame < MAX_FRAMES);

	const int FrameCount = GetFrameCount(Track);
	const int Channels = GetAvailableChannels();

	if (FrameCount + Count > MAX_FRAMES)
		return false;

	SetFrameCount(Track, FrameCount + Count);

	for (unsigned int i = FrameCount + Count - 1; i >= Frame + Count; --i)
		for (int j = 0; j < Channels; ++j)
			SetPatternAtFrame(Track, i, j, GetPatternAtFrame(Track, i - Count, j));

	for (int i = 0; i < Channels; ++i)
		for (int f = 0; f < Count; ++f)		// // //
			SetPatternAtFrame(Track, Frame + f, i, 0);

	m_pBookmarkManager->GetCollection(Track)->InsertFrames(Frame, Count);		// // //

	return true;
}

bool CFamiTrackerDoc::DeleteFrames(unsigned int Track, unsigned int Frame, int Count)
{
	ASSERT(Track < MAX_TRACKS);
	ASSERT(Frame < MAX_FRAMES);

	for (int i = 0; i < Count; ++i)
		RemoveFrame(Track, Frame);

	return true;
}

//// Track functions //////////////////////////////////////////////////////////////////////////////////

const std::string &CFamiTrackerDoc::GetTrackTitle(unsigned int Track) const		// // //
{
	if (Track < GetTrackCount())
		return GetSongData(Track).GetTitle();
	return CSongData::DEFAULT_TITLE;
}

int CFamiTrackerDoc::AddTrack()
{
	// Add new track. Returns -1 on failure, or added track number otherwise

	int NewTrack = GetTrackCount();

	if (NewTrack >= MAX_TRACKS)
		return -1;

	AllocateSong(NewTrack);
	m_pBookmarkManager->InsertTrack(NewTrack);		// // //

	return NewTrack;
}

void CFamiTrackerDoc::RemoveTrack(unsigned int Track)
{
	ASSERT(GetTrackCount() > 1);

	// Move down all other tracks
	m_pTracks.erase(m_pTracks.cbegin() + Track);		// // //
	m_pBookmarkManager->RemoveTrack(Track);		// // //
}

void CFamiTrackerDoc::SetTrackTitle(unsigned int Track, const std::string &title)		// // //
{
	GetSongData(Track).SetTitle(title);
}

void CFamiTrackerDoc::MoveTrackUp(unsigned int Track)
{
	ASSERT(Track > 0);

	SwapSongs(Track, Track - 1);
}

void CFamiTrackerDoc::MoveTrackDown(unsigned int Track)
{
	ASSERT(Track < MAX_TRACKS);

	SwapSongs(Track, Track + 1);
}

void CFamiTrackerDoc::SwapSongs(unsigned int First, unsigned int Second)
{
	m_pTracks[First].swap(m_pTracks[Second]);		// // //
	m_pBookmarkManager->SwapTracks(First, Second);		// // //
}

void CFamiTrackerDoc::AllocateSong(unsigned int Index)
{
	// Allocate a new song if not already done
	ASSERT(Index < MAX_TRACKS);
	while (Index >= m_pTracks.size()) {		// // //
		auto &pSong = m_pTracks.emplace_back(std::make_unique<CSongData>());		// // //
		pSong->SetSongTempo(m_iMachine == NTSC ? DEFAULT_TEMPO_NTSC : DEFAULT_TEMPO_PAL);
		m_pBookmarkManager->GetCollection(Index)->ClearBookmarks();
	}
}

CSongData &CFamiTrackerDoc::GetSongData(unsigned int Index)		// // //
{
	// Ensure track is allocated
	AllocateSong(Index);
	return *m_pTracks[Index];
}

const CSongData &CFamiTrackerDoc::GetSongData(unsigned int Index) const		// // //
{
	return *m_pTracks[Index];
}

std::unique_ptr<CSongData> CFamiTrackerDoc::ReplaceSong(unsigned Index, std::unique_ptr<CSongData> pSong) {		// // //
	m_pTracks[Index].swap(pSong);
	return std::move(pSong);
}

unsigned int CFamiTrackerDoc::GetTrackCount() const
{
	return m_pTracks.size();
}

void CFamiTrackerDoc::SelectExpansionChip(unsigned char Chip, bool Move)
{
	// // // Move pattern data upon removing expansion chips
	if (Move) {
		int oldIndex[CHANNELS] = {};
		int newIndex[CHANNELS] = {};
		for (int j = 0; j < CHANNELS; j++) {
			oldIndex[j] = GetChannelPosition(j, m_iExpansionChip);
			newIndex[j] = GetChannelPosition(j, Chip);
		}
		auto it = m_pTracks.begin();
		for (const auto &pSong : m_pTracks) {
			auto pNew = std::make_unique<CSongData>(pSong->GetPatternLength());
			pNew->SetHighlight(pSong->GetRowHighlight());
			pNew->SetSongTempo(pSong->GetSongTempo());
			pNew->SetSongSpeed(pSong->GetSongSpeed());
			pNew->SetSongGroove(pSong->GetSongGroove());
			pNew->SetFrameCount(pSong->GetFrameCount());
			pNew->SetTitle(pSong->GetTitle());
			for (int j = 0; j < CHANNELS; ++j)
				if (oldIndex[j] != -1 && newIndex[j] != -1)
					pNew->CopyTrack(newIndex[j], *pSong, oldIndex[j]);
			*it++ = std::move(pNew);
		}
	}
	// Complete sound chip setup
	SetupChannels(Chip);
	ApplyExpansionChip();
	ModifyIrreversible();

	if (!(Chip & SNDCHIP_N163))		// // //
		m_iNamcoChannels = 0;
}

void CFamiTrackerDoc::SetupChannels(unsigned char Chip)
{
	// This will select a chip in the sound emulator

	if (Chip != SNDCHIP_NONE) {
		// Do not allow expansion chips in PAL mode
		SetMachine(NTSC);
	}

	// Store the chip
	m_iExpansionChip = Chip;

	// Register the channels
	theApp.GetSoundGenerator()->RegisterChannels();		// // //

	m_iChannelsAvailable = GetChannelCount();

	/*if (Chip & SNDCHIP_N163) {			// // //
		m_iChannelsAvailable -= (8 - m_iNamcoChannels);
	}*/

	// Must call ApplyExpansionChip after this
}

void CFamiTrackerDoc::ApplyExpansionChip()
{
	// Tell the sound emulator to switch expansion chip
	theApp.GetSoundGenerator()->SelectChip(m_iExpansionChip);

	// Change period tables
	theApp.GetSoundGenerator()->LoadMachineSettings();		// // //
}

//
// from the compoment interface
//

CChannelMap *const CFamiTrackerDoc::GetChannelMap() const {
	return m_pChannelMap.get();
}

CSequenceManager *const CFamiTrackerDoc::GetSequenceManager(int InstType) const
{
	return m_pInstrumentManager->GetSequenceManager(InstType);
}

CInstrumentManager *const CFamiTrackerDoc::GetInstrumentManager() const
{
	return m_pInstrumentManager.get();
}

CDSampleManager *const CFamiTrackerDoc::GetDSampleManager() const
{
	return m_pInstrumentManager->GetDSampleManager();
}

CBookmarkManager *const CFamiTrackerDoc::GetBookmarkManager() const
{
	return m_pBookmarkManager.get();
}

void CFamiTrackerDoc::Modify(bool Change)
{
	SetModifiedFlag(Change ? TRUE : FALSE);
}

void CFamiTrackerDoc::ModifyIrreversible()
{
	SetModifiedFlag(TRUE);
	SetExceededFlag(TRUE);
}

bool CFamiTrackerDoc::ExpansionEnabled(int Chip) const
{
	// Returns true if a specified chip is enabled
	return (GetExpansionChip() & Chip) == Chip; 
}

void CFamiTrackerDoc::SetNamcoChannels(int Channels, bool Move)
{
	if (Channels == 0) {		// // //
		SelectExpansionChip(m_iExpansionChip & ~SNDCHIP_N163, true);
		return;
	}
	if (!ExpansionEnabled(SNDCHIP_N163))
		SelectExpansionChip(m_iExpansionChip | SNDCHIP_N163, true);

	ASSERT(Channels <= 8);
	m_iNamcoChannels = Channels;
	
	// // // Move pattern data upon removing N163 channels
	if (Move) {
		int oldIndex[CHANNELS] = {};		// // //
		int newIndex[CHANNELS] = {};
		for (int j = 0; j < CHANNELS; j++) {
			oldIndex[j] = GetChannelIndex(j);
			newIndex[j] = GetChannelPosition(j, m_iExpansionChip);
		}
		auto it = m_pTracks.begin();
		for (const auto &pSong : m_pTracks) {
			auto pNew = std::make_unique<CSongData>(pSong->GetPatternLength());
			pNew->SetHighlight(pSong->GetRowHighlight());
			pNew->SetSongTempo(pSong->GetSongTempo());
			pNew->SetSongSpeed(pSong->GetSongSpeed());
			pNew->SetSongGroove(pSong->GetSongGroove());
			pNew->SetFrameCount(pSong->GetFrameCount());
			pNew->SetTitle(pSong->GetTitle());
			for (int j = 0; j < CHANNELS; ++j)
				if (oldIndex[j] != -1 && newIndex[j] != -1)
					pNew->CopyTrack(newIndex[j], *pSong, oldIndex[j]);
			*it++ = std::move(pNew);
		}
	}

	SelectExpansionChip(m_iExpansionChip, false);		// // //
}

int CFamiTrackerDoc::GetNamcoChannels() const
{
	if (!ExpansionEnabled(SNDCHIP_N163)) return 0;		// // //
	return m_iNamcoChannels;
}

unsigned int CFamiTrackerDoc::GetFirstFreePattern(unsigned int Track, unsigned int Channel) const
{
	auto &Song = GetSongData(Track);

	for (int i = 0; i < MAX_PATTERN; ++i) {
		if (!Song.IsPatternInUse(Channel, i) && Song.IsPatternEmpty(Channel, i))
			return i;
	}

	return -1;		// // //
}

bool CFamiTrackerDoc::IsPatternEmpty(unsigned int Track, unsigned int Channel, unsigned int Pattern) const
{
	return GetSongData(Track).IsPatternEmpty(Channel, Pattern);
}

// Channel interface, these functions must be synchronized!!!

int CFamiTrackerDoc::GetChannelType(int Channel) const
{
	return m_pChannelMap->GetChannelType(Channel);		// // //
}

int CFamiTrackerDoc::GetChipType(int Channel) const
{
	return m_pChannelMap->GetChipType(Channel);		// // //
}

int CFamiTrackerDoc::GetChannelCount() const
{
	return m_pChannelMap->GetChannelCount();		// // //
}

int CFamiTrackerDoc::GetChannelPosition(int Channel, unsigned char Chip)		// // //
{
	// TODO: use information from the current channel map instead
	unsigned int pos = Channel;
	if (pos == CHANID_MMC5_VOICE) return -1;

	if (!(Chip & SNDCHIP_S5B)) {
		if (pos > CHANID_S5B_CH3) pos -= 3;
		else if (pos >= CHANID_S5B_CH1) return -1;
	}
	if (!(Chip & SNDCHIP_VRC7)) {
		if (pos > CHANID_VRC7_CH6) pos -= 6;
		else if (pos >= CHANID_VRC7_CH1) return -1;
	}
	if (!(Chip & SNDCHIP_FDS)) {
		if (pos > CHANID_FDS) pos -= 1;
		else if (pos >= CHANID_FDS) return -1;
	}
		if (pos > CHANID_N163_CH8) pos -= 8 - (!(Chip & SNDCHIP_N163) ? 0 : m_iNamcoChannels);
		else if (pos > CHANID_MMC5_VOICE + (!(Chip & SNDCHIP_N163) ? 0 : m_iNamcoChannels)) return -1;
	if (pos > CHANID_MMC5_VOICE) pos -= 1;
	if (!(Chip & SNDCHIP_MMC5)) {
		if (pos > CHANID_MMC5_SQUARE2) pos -= 2;
		else if (pos >= CHANID_MMC5_SQUARE1) return -1;
	}
	if (!(Chip & SNDCHIP_VRC6)) {
		if (pos > CHANID_VRC6_SAWTOOTH) pos -= 3;
		else if (pos >= CHANID_VRC6_PULSE1) return -1;
	}

	return pos;
}

CTrackerChannel &CFamiTrackerDoc::GetChannel(int Index) const		// // //
{
	return m_pChannelMap->GetChannel(Index);		// // //
}

int CFamiTrackerDoc::GetChannelIndex(int Channel) const
{
	return m_pChannelMap->GetChannelIndex(Channel);		// // //
}

// Vibrato functions

vibrato_t CFamiTrackerDoc::GetVibratoStyle() const
{
	return m_iVibratoStyle;
}

void CFamiTrackerDoc::SetVibratoStyle(vibrato_t Style)
{
	m_iVibratoStyle = Style;
}

// Linear pitch slides

bool CFamiTrackerDoc::GetLinearPitch() const
{
	return m_bLinearPitch;
}

void CFamiTrackerDoc::SetLinearPitch(bool Enable)
{
	m_bLinearPitch = Enable;
}

// Attributes

CString CFamiTrackerDoc::GetFileTitle() const 
{
	// Return file name without extension
	CString FileName = GetTitle();

	static const LPCSTR EXT[] = {_T(".ftm"), _T(".0cc"), _T(".ftm.bak"), _T(".0cc.bak")};		// // //
	// Remove extension

	for (size_t i = 0; i < sizeof(EXT) / sizeof(LPCSTR); ++i) {
		int Len = lstrlen(EXT[i]);
		if (FileName.Right(Len).CompareNoCase(EXT[i]) == 0)
			return FileName.Left(FileName.GetLength() - Len);
	}

	return FileName;
}

bool CFamiTrackerDoc::IsFileLoaded() const
{
	return m_bFileLoaded;
}

bool CFamiTrackerDoc::HasLastLoadFailed() const
{
	return m_bFileLoadFailed;
}

#ifdef AUTOSAVE

// Auto-save (experimental)

void CFamiTrackerDoc::SetupAutoSave()
{
	TCHAR TempPath[MAX_PATH], TempFile[MAX_PATH];

	GetTempPath(MAX_PATH, TempPath);
	GetTempFileName(TempPath, _T("Aut"), 21587, TempFile);

	// Check if file exists
	CFile file;
	if (file.Open(TempFile, CFile::modeRead)) {
		file.Close();
		if (AfxMessageBox(_T("It might be possible to recover last document, do you want to try?"), MB_YESNO) == IDYES) {
			OpenDocument(TempFile);
			SelectExpansionChip(m_iExpansionChip);
		}
		else {
			DeleteFile(TempFile);
		}
	}

	TRACE("Doc: Allocated file for auto save: ");
	TRACE(TempFile);
	TRACE("\n");

	m_sAutoSaveFile = TempFile;
}

void CFamiTrackerDoc::ClearAutoSave()
{
	if (m_sAutoSaveFile.GetLength() == 0)
		return;

	DeleteFile(m_sAutoSaveFile);

	m_sAutoSaveFile = _T("");
	m_iAutoSaveCounter = 0;

	TRACE("Doc: Removed auto save file\n");
}

void CFamiTrackerDoc::AutoSave()
{
	// Autosave
	if (!m_iAutoSaveCounter || !m_bFileLoaded || m_sAutoSaveFile.GetLength() == 0)
		return;

	m_iAutoSaveCounter--;

	if (m_iAutoSaveCounter == 0) {
		TRACE("Doc: Performing auto save\n");
		SaveDocument(m_sAutoSaveFile);
	}
}

#endif

//
// Comment functions
//

void CFamiTrackerDoc::SetComment(const std::string &comment, bool bShowOnLoad)		// // //
{
	m_strComment = comment;
	m_bDisplayComment = bShowOnLoad;
}

const std::string &CFamiTrackerDoc::GetComment() const		// // //
{
	return m_strComment;
}

bool CFamiTrackerDoc::ShowCommentOnOpen() const
{
	return m_bDisplayComment;
}

void CFamiTrackerDoc::SetSpeedSplitPoint(int SplitPoint)
{
	m_iSpeedSplitPoint = SplitPoint;
}

int CFamiTrackerDoc::GetSpeedSplitPoint() const
{
	return m_iSpeedSplitPoint;
}

void CFamiTrackerDoc::SetHighlight(unsigned int Track, const stHighlight &Hl)		// // //
{
	GetSongData(Track).SetHighlight(Hl);
}

const stHighlight &CFamiTrackerDoc::GetHighlight(unsigned int Track) const		// // //
{
	return GetSongData(Track).GetRowHighlight();
}

void CFamiTrackerDoc::SetHighlight(const stHighlight &Hl)		// // //
{
	m_vHighlight = Hl;
}

const stHighlight &CFamiTrackerDoc::GetHighlight() const		// // //
{
	return m_vHighlight;
}

stHighlight CFamiTrackerDoc::GetHighlightAt(unsigned int Track, unsigned int Frame, unsigned int Row) const		// // //
{
	while (Frame < 0) Frame += GetFrameCount(Track);
	Frame %= GetFrameCount(Track);

	stHighlight Hl = m_vHighlight;
	
	const CBookmark Zero { };
	CBookmarkCollection *pCol = m_pBookmarkManager->GetCollection(Track);
	if (const unsigned Count = pCol->GetCount()) {
		CBookmark tmp(Frame, Row);
		unsigned int Min = tmp.Distance(Zero);
		for (unsigned i = 0; i < Count; ++i) {
			CBookmark *pMark = pCol->GetBookmark(i);
			unsigned Dist = tmp.Distance(*pMark);
			if (Dist <= Min) {
				Min = Dist;
				if (pMark->m_Highlight.First != -1 && (pMark->m_bPersist || pMark->m_iFrame == Frame))
					Hl.First = pMark->m_Highlight.First;
				if (pMark->m_Highlight.Second != -1 && (pMark->m_bPersist || pMark->m_iFrame == Frame))
					Hl.Second = pMark->m_Highlight.Second;
				Hl.Offset = pMark->m_Highlight.Offset + pMark->m_iRow;
			}
		}
	}

	return Hl;
}

unsigned int CFamiTrackerDoc::GetHighlightState(unsigned int Track, unsigned int Frame, unsigned int Row) const		// // //
{
	stHighlight Hl = GetHighlightAt(Track, Frame, Row);
	if (Hl.Second > 0 && !((Row - Hl.Offset) % Hl.Second))
		return 2;
	if (Hl.First > 0 && !((Row - Hl.Offset) % Hl.First))
		return 1;
	return 0;
}

CBookmark *CFamiTrackerDoc::GetBookmarkAt(unsigned int Track, unsigned int Frame, unsigned int Row) const		// // //
{
	if (CBookmarkCollection *pCol = m_pBookmarkManager->GetCollection(Track)) {
		for (unsigned i = 0, Count = pCol->GetCount(); i < Count; ++i) {
			CBookmark *pMark = pCol->GetBookmark(i);
			if (pMark->m_iFrame == Frame && pMark->m_iRow == Row)
				return pMark;
		}
	}
	return nullptr;
}

unsigned int CFamiTrackerDoc::ScanActualLength(unsigned int Track, unsigned int Count) const		// // //
{
	// Return number for frames played for a certain number of loops

	char RowVisited[MAX_FRAMES][MAX_PATTERN_LENGTH] = { };		// // //
	int JumpTo = -1;
	int SkipTo = -1;
	int FirstLoop = 0;
	int SecondLoop = 0;
	unsigned int f = 0;		// // //
	unsigned int r = 0;		// // //
	bool bScanning = true;
	unsigned int FrameCount = GetFrameCount(Track);
	int RowCount = 0;
	// // //

	while (bScanning) {
		bool hasJump = false;
		for (int j = 0; j < GetChannelCount(); ++j) {
			const auto &Note = GetNoteData(Track, f, j, r);		// // //
			for (unsigned l = 0; l < GetEffColumns(Track, j) + 1; ++l) {
				switch (Note.EffNumber[l]) {
					case EF_JUMP:
						JumpTo = Note.EffParam[l];
						SkipTo = 0;
						hasJump = true;
						break;
					case EF_SKIP:
						if (hasJump) break;
						JumpTo = (f + 1) % FrameCount;
						SkipTo = Note.EffParam[l];
						break;
					case EF_HALT:
						Count = 1;
						bScanning = false;
						break;
				}
			}
		}

		switch (RowVisited[f][r]) {
		case 0: ++FirstLoop; break;
		case 1: ++SecondLoop; break;
		case 2: bScanning = false; break;
		}
		
		++RowVisited[f][r++];

		if (JumpTo > -1) {
			f = std::min(static_cast<unsigned int>(JumpTo), FrameCount - 1);
			JumpTo = -1;
		}
		if (SkipTo > -1) {
			r = std::min(static_cast<unsigned int>(SkipTo), GetPatternLength(Track) - 1);
			SkipTo = -1;
		}
		if (r >= GetPatternLength(Track)) {		// // //
			++f;
			r = 0;
		}
		if (f >= FrameCount)
			f = 0;
	}

	return FirstLoop + SecondLoop * (Count - 1);		// // //
}

double CFamiTrackerDoc::GetStandardLength(int Track, unsigned int ExtraLoops) const		// // //
{
	char RowVisited[MAX_FRAMES][MAX_PATTERN_LENGTH] = { };
	int JumpTo = -1;
	int SkipTo = -1;
	double FirstLoop = 0.0;
	double SecondLoop = 0.0;
	bool IsGroove = GetSongGroove(Track);
	double Tempo = GetSongTempo(Track);
	double Speed = GetSongSpeed(Track);
	if (!GetSongTempo(Track))
		Tempo = 2.5 * GetFrameRate();
	int GrooveIndex = GetSongSpeed(Track) * (m_pGrooveTable[GetSongSpeed(Track)] != NULL), GroovePointer = 0;
	bool bScanning = true;
	unsigned int FrameCount = GetFrameCount(Track);

	if (IsGroove && GetGroove(GetSongSpeed(Track)) == NULL) {
		IsGroove = false;
		Speed = DEFAULT_SPEED;
	}

	unsigned int f = 0;
	unsigned int r = 0;
	while (bScanning) {
		bool hasJump = false;
		for (int j = 0; j < GetChannelCount(); ++j) {
			const auto &Note = GetNoteData(Track, f, j, r);
			for (unsigned l = 0; l < GetEffColumns(Track, j) + 1; ++l) {
				switch (Note.EffNumber[l]) {
				case EF_JUMP:
					JumpTo = Note.EffParam[l];
					SkipTo = 0;
					hasJump = true;
					break;
				case EF_SKIP:
					if (hasJump) break;
					JumpTo = (f + 1) % FrameCount;
					SkipTo = Note.EffParam[l];
					break;
				case EF_HALT:
					ExtraLoops = 0;
					bScanning = false;
					break;
				case EF_SPEED:
					if (GetSongTempo(Track) && Note.EffParam[l] >= m_iSpeedSplitPoint)
						Tempo = Note.EffParam[l];
					else {
						IsGroove = false;
						Speed = Note.EffParam[l];
					}
					break;
				case EF_GROOVE:
					if (m_pGrooveTable[Note.EffParam[l]] == NULL) break;
					IsGroove = true;
					GrooveIndex = Note.EffParam[l];
					GroovePointer = 0;
					break;
				}
			}
		}
		if (IsGroove)
			Speed = m_pGrooveTable[GrooveIndex]->GetEntry(GroovePointer++);
		
		switch (RowVisited[f][r]) {
		case 0: FirstLoop += Speed / Tempo; break;
		case 1: SecondLoop += Speed / Tempo; break;
		case 2: bScanning = false; break;
		}
		
		++RowVisited[f][r++];

		if (JumpTo > -1) {
			f = std::min(static_cast<unsigned int>(JumpTo), FrameCount - 1);
			JumpTo = -1;
		}
		if (SkipTo > -1) {
			r = std::min(static_cast<unsigned int>(SkipTo), GetPatternLength(Track) - 1);
			SkipTo = -1;
		}
		if (r >= GetPatternLength(Track)) {		// // //
			++f;
			r = 0;
		}
		if (f >= FrameCount)
			f = 0;
	}

	return (2.5 * (FirstLoop + SecondLoop * ExtraLoops));
}

// Operations

void CFamiTrackerDoc::RemoveUnusedInstruments()
{
	for (int i = 0; i < MAX_INSTRUMENTS; ++i) {
		if (IsInstrumentUsed(i)) {
			bool Used = false;
			for (const auto &pSong : m_pTracks) {		// // //
				for (unsigned int Channel = 0; Channel < m_iChannelsAvailable; ++Channel) {
					for (unsigned int Frame = 0; Frame < pSong->GetFrameCount(); ++Frame) {
						unsigned int Pattern = pSong->GetFramePattern(Frame, Channel);
						for (unsigned int Row = 0; Row < pSong->GetPatternLength(); ++Row) {
							if (pSong->GetPatternData(Channel, Pattern, Row).Instrument == i)		// // //
								Used = true;
						}
					}
				}
			}
			if (!Used)
				RemoveInstrument(i);
		}
	}

	static const inst_type_t inst[] = {INST_2A03, INST_VRC6, INST_N163, INST_S5B};
	static const uint8_t chip[] = {SNDCHIP_NONE, SNDCHIP_VRC6, SNDCHIP_N163, SNDCHIP_S5B};

	// Also remove unused sequences
	for (unsigned int i = 0; i < MAX_SEQUENCES; ++i) for (int j = 0; j < SEQ_COUNT; ++j) {
		for (size_t c = 0; c < sizeof(chip); c++) if (GetSequenceItemCount(inst[c], i, j) > 0) {		// // //
			bool Used = false;
			for (int k = 0; k < MAX_INSTRUMENTS; ++k) {
				if (IsInstrumentUsed(k) && GetInstrumentType(k) == inst[c]) {
					auto pInstrument = std::static_pointer_cast<CSeqInstrument>(GetInstrument(k));
					if (pInstrument->GetSeqIndex(j) == i && pInstrument->GetSeqEnable(j)) {		// // //
						Used = true; break;
					}
				}
			}
			if (!Used)
				GetSequence(inst[c], i, j)->Clear();
		}
	}
}

void CFamiTrackerDoc::RemoveUnusedPatterns()
{
	for (auto &pSong : m_pTracks)
		for (unsigned int c = 0; c < m_iChannelsAvailable; ++c)
			for (unsigned int p = 0; p < MAX_PATTERN; ++p)
				if (!pSong->IsPatternInUse(c, p))
					pSong->ClearPattern(c, p);
}

void CFamiTrackerDoc::RemoveUnusedSamples()		// // //
{
	bool AssignUsed[MAX_INSTRUMENTS][OCTAVE_RANGE][NOTE_RANGE] = { };

	for (int i = 0; i < MAX_DSAMPLES; ++i) {
		if (IsSampleUsed(i)) {
			bool Used = false;
			for (const auto &pSong : m_pTracks) {
				for (unsigned int Frame = 0; Frame < pSong->GetFrameCount(); ++Frame) {
					unsigned int Pattern = pSong->GetFramePattern(Frame, CHANID_DPCM);
					for (unsigned int Row = 0; Row < pSong->GetPatternLength(); ++Row) {
						const auto &Note = pSong->GetPatternData(CHANID_DPCM, Pattern, Row);		// // //
						int Index = Note.Instrument;
						if (Note.Note < NOTE_C || Note.Note > NOTE_B || Index == MAX_INSTRUMENTS)
							continue;		// // //
						if (GetInstrumentType(Index) != INST_2A03)
							continue;
						AssignUsed[Index][Note.Octave][Note.Note - 1] = true;
						auto pInst = std::static_pointer_cast<CInstrument2A03>(GetInstrument(Index));
						if (pInst->GetSampleIndex(Note.Octave, Note.Note - 1) == i + 1)
							Used = true;
					}
				}
			}
			if (!Used)
				RemoveSample(i);
		}
	}
	// also remove unused assignments
	for (int i = 0; i < MAX_INSTRUMENTS; i++) if (IsInstrumentUsed(i))
		if (auto pInst = std::dynamic_pointer_cast<CInstrument2A03>(GetInstrument(i)))
			for (int o = 0; o < OCTAVE_RANGE; o++) for (int n = 0; n < NOTE_RANGE; n++)
				if (!AssignUsed[i][o][n])
					pInst->SetSampleIndex(o, n, 0);
}

bool CFamiTrackerDoc::ArePatternsSame(unsigned int Track, unsigned int Channel, unsigned int Pattern1, unsigned int Pattern2) const		// // //
{
	const auto &song = GetSongData(Track);
	return song.GetPattern(Channel, Pattern1) == song.GetPattern(Channel, Pattern2);
}

void CFamiTrackerDoc::SwapInstruments(int First, int Second)
{
	// Swap instruments
	m_pInstrumentManager->SwapInstruments(First, Second);		// // //
	
	// Scan patterns
	VisitSongs([&] (CSongData &song) {
		song.VisitPatterns([&] (CPatternData &pat) {
			pat.VisitRows([&] (stChanNote &note, unsigned row) {
				if (note.Instrument == First)
					note.Instrument = Second;
				else if (note.Instrument == Second)
					note.Instrument = First;
			});
		});
	});
}

void CFamiTrackerDoc::SetDetuneOffset(int Chip, int Note, int Detune)		// // //
{
	m_iDetuneTable[Chip][Note] = Detune;
}

int CFamiTrackerDoc::GetDetuneOffset(int Chip, int Note) const		// // //
{
	return m_iDetuneTable[Chip][Note];
}

void CFamiTrackerDoc::ResetDetuneTables()		// // //
{
	for (int i = 0; i < 6; i++) for (int j = 0; j < NOTE_COUNT; j++)
		m_iDetuneTable[i][j] = 0;
}

void CFamiTrackerDoc::SetTuning(int Semitone, int Cent)		// // // 050B
{
	m_iDetuneSemitone = Semitone;
	m_iDetuneCent = Cent;
}

int CFamiTrackerDoc::GetTuningSemitone() const		// // // 050B
{
	return m_iDetuneSemitone;
}

int CFamiTrackerDoc::GetTuningCent() const		// // // 050B
{
	return m_iDetuneCent;
}

CGroove *CFamiTrackerDoc::GetGroove(unsigned Index) const		// // //
{
	return Index < MAX_GROOVE ? m_pGrooveTable[Index].get() : nullptr;
}

void CFamiTrackerDoc::SetGroove(unsigned Index, std::unique_ptr<CGroove> Groove)
{
	m_pGrooveTable[Index] = std::move(Groove);
}

void CFamiTrackerDoc::SetExceededFlag(bool Exceed)
{
	m_bExceeded = Exceed;
}

int CFamiTrackerDoc::GetFrameLength(unsigned int Track, unsigned int Frame) const
{
	// // // moved from PatternEditor.cpp
	return GetSongData(Track).GetFrameSize(Frame, GetChannelCount());		// // //
}

struct Kraid {		// // // Easter egg
	void buildDoc(CFamiTrackerDoc &doc) {
		// Instruments and sequences
		makeInst(doc, 0, 6, "Lead ");
		makeInst(doc, 1, 2, "Echo");
		makeInst(doc, 2, 15, "Triangle");
	}

	void buildSong(CSongData &song) {
		const unsigned FRAMES = 14;
		const unsigned ROWS = 24;
		const unsigned PATTERNS[][FRAMES] = {
			{0, 0, 0, 0, 1, 1, 2, 3, 3, 3, 4, 5, 6, 6},
			{0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4},
			{0, 0, 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
		};

		song.SetFrameCount(FRAMES);
		song.SetPatternLength(ROWS);
		song.SetSongSpeed(8);
		song.SetEffectColumnCount(0, 1);

		for (int ch = 0; ch < FRAMES; ++ch)
			for (int f = 0; f < ROWS; ++f)
				song.SetFramePattern(f, ch, PATTERNS[ch][f]);
		
		makePattern(song, 2, 0, "<e.>e...<e.>e...<e.>e...<e.>e...");
		makePattern(song, 2, 1, "<c.>c...<c.>c...<d.>d...<d.>d...");
		makePattern(song, 2, 2, "<e.>e.>e.<<F.>F.>F.<<f.>f.>f.<<<b.>b.>b.");
		makePattern(song, 2, 3, "<e...b.>c...<b.c...g.a...b.");
		makePattern(song, 2, 4, "<<e");

		makePattern(song, 1, 0, "@e...<b.>a... c. F...d.<b...A.");
		makePattern(song, 1, 1, "@g... d. e...<b.>F...d. a...e.");
		makePattern(song, 1, 2, "@g<b>g<b>g<b>AeAeAeacacacaDFDbD");
		makePattern(song, 1, 3, "Fgab>d<b>Fd<agFb>aFd<agFega>de-");
		makePattern(song, 1, 4, ">a-g-F-e-F-g-a-g-F-e-F-g-");

		int f = 0;
		int r = 0;
		do { // TODO: use CSongIterator
			auto note = song.GetPatternOnFrame(1, f).GetNoteOn(r);
			if (++r >= ROWS) {
				r = 0;
				if (++f >= FRAMES)
					f = 0;
			}
			if (note != stChanNote { }) {
				note.Instrument = 1;
				note.EffNumber[1] = EF_DELAY;
				note.EffParam[1] = 3;
				song.GetPatternOnFrame(0, f).SetNoteOn(r, note);
			}
		} while (f || r);
	}

private:
	void makeInst(CFamiTrackerDoc &doc, unsigned index, char vol, const char *name) {
		doc.AddInstrument(doc.GetInstrumentManager()->CreateNew(INST_2A03), index);
		auto leadInst = std::dynamic_pointer_cast<CInstrument2A03>(doc.GetInstrument(index));
		leadInst->SetSeqEnable(SEQ_VOLUME, true);
		leadInst->SetSeqIndex(SEQ_VOLUME, index);
		leadInst->SetName(name);

		CSequence &leadEnv = *leadInst->GetSequence(SEQ_VOLUME);
		leadEnv.SetItemCount(1);
		leadEnv.SetItem(0, vol);
		leadEnv.SetLoopPoint(-1);
		leadEnv.SetReleasePoint(-1);
	}

	void makePattern(CSongData &song, unsigned ch, unsigned pat, std::string_view mml) {
		const uint8_t INST = ch == 1 ? 0 : 2;
		uint8_t octave = 3;
		int row = 0;
		auto &pattern = song.GetPattern(ch, pat);

		for (auto c : mml) {
			auto &note = pattern.GetNoteOn(row);
			switch (c) {
			case '<': --octave; break;
			case '>': ++octave; break;
			case '.': ++row; break;
			case '-': ++row; note.Note = HALT   ; break;
			case '=': ++row; note.Note = RELEASE; break;
			case 'c': ++row; note.Note = NOTE_C ; note.Octave = octave, note.Instrument = INST; break;
			case 'C': ++row; note.Note = NOTE_Cs; note.Octave = octave, note.Instrument = INST; break;
			case 'd': ++row; note.Note = NOTE_D ; note.Octave = octave, note.Instrument = INST; break;
			case 'D': ++row; note.Note = NOTE_Ds; note.Octave = octave, note.Instrument = INST; break;
			case 'e': ++row; note.Note = NOTE_E ; note.Octave = octave, note.Instrument = INST; break;
			case 'f': ++row; note.Note = NOTE_F ; note.Octave = octave, note.Instrument = INST; break;
			case 'F': ++row; note.Note = NOTE_Fs; note.Octave = octave, note.Instrument = INST; break;
			case 'g': ++row; note.Note = NOTE_G ; note.Octave = octave, note.Instrument = INST; break;
			case 'G': ++row; note.Note = NOTE_Gs; note.Octave = octave, note.Instrument = INST; break;
			case 'a': ++row; note.Note = NOTE_A ; note.Octave = octave, note.Instrument = INST; break;
			case 'A': ++row; note.Note = NOTE_As; note.Octave = octave, note.Instrument = INST; break;
			case 'b': ++row; note.Note = NOTE_B ; note.Octave = octave, note.Instrument = INST; break;
			case '@': note.EffNumber[0] = EF_DUTY_CYCLE; note.EffParam[0] = 2; break;
			}
		}
	}
};

void CFamiTrackerDoc::MakeKraid()			// // // Easter Egg
{
	// Basic info
	CreateEmpty();

	Kraid builder;
	builder.buildDoc(*this);
	builder.buildSong(GetSongData(0));
}
