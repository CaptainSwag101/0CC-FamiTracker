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


#pragma once

//
// This thread will take care of the NES sound generation
//

#include "stdafx.h"		// // //
#include "Common.h"
#include <string>
#include <vector>		// // //
#include <array>		// // //
#include <memory>		// // //
#include "FamiTrackerTypes.h"		// // //
#include "SoundGenBase.h"		// // //

// Custom messages
enum {
	WM_USER_SILENT_ALL = WM_USER + 1,
	WM_USER_LOAD_SETTINGS,
	WM_USER_PLAY,
	WM_USER_STOP,
	WM_USER_RESET,
	WM_USER_START_RENDER,
	WM_USER_STOP_RENDER,
	WM_USER_PREVIEW_SAMPLE,
	WM_USER_WRITE_APU,
	WM_USER_CLOSE_SOUND,
	WM_USER_SET_CHIP,
	WM_USER_VERIFY_EXPORT,
	WM_USER_REMOVE_DOCUMENT
};

class stChanNote;		// // //
struct stRecordSetting;

enum note_prio_t : int;		// // //

class CFamiTrackerView;
class CFamiTrackerDoc;
class CInstrument;		// // //
class CSequence;		// // //
class CAPU;
class CDSound;
class CDSoundChannel;
class CVisualizerWnd;
class CFTMComponentInterface;		// // //
class CInstrumentRecorder;		// // //
class CRegisterState;		// // //
class CArpeggiator;		// // //
class CAudioDriver;		// // //
class CWaveRenderer;		// // //
class CTempoDisplay;		// // //
class CTempoCounter;		// // //
class CPlayerCursor;		// // //
class CSoundDriver;		// // //
class CChannelMap;		// // //

namespace ft0cc::doc {
class dpcm_sample;
} // namespace ft0cc::doc

// CSoundGen

class CSoundGen : public CWinThread, public IAudioCallback, public CSoundGenBase		// // //
{
protected:
	DECLARE_DYNCREATE(CSoundGen)
public:
	CSoundGen();
	virtual ~CSoundGen();

	//
	// Public functions
	//
public:

	// One time initialization
	void		AssignDocument(CFamiTrackerDoc *pDoc);
	void		AssignView(CFamiTrackerView *pView);
	void		RemoveDocument();
	void		SetVisualizerWindow(CVisualizerWnd *pWnd);

	// Multiple times initialization
	std::unique_ptr<CChannelMap>	MakeChannelMap(unsigned chips, unsigned n163chs) const;		// // //
	void		SelectChip(int Chip);
	void		LoadMachineSettings();		// // // 050B

	// Sound
	bool		InitializeSound(HWND hWnd);
	void		FlushBuffer(int16_t *Buffer, uint32_t Size) override;
	CDSound		*GetSoundInterface() const;		// // //
	CAudioDriver *GetAudioDriver() const;		// // //

	void		Interrupt() const;

	bool		WaitForStop() const;
	bool		IsRunning() const;

	void		DocumentPropertiesChanged(CFamiTrackerDoc *pDocument);

public:
	int			 ReadVibratoTable(int index) const;
	int			 ReadPeriodTable(int Index, int Table) const;		// // //

	// Player interface
	void		 StartPlayer(std::unique_ptr<CPlayerCursor> Pos);		// // //
	void		 StopPlayer();
	void		 ResetPlayer(int Track);
	void		 LoadSettings();
	void		 SilentAll();

	void		 SetChannelMute(int chan, bool mute);		// // // TODO: move into CChannel
	bool		 IsChannelMuted(int chan) const override;

	void		 ResetState();
	void		 ResetTempo();
	void		 SetHighlightRows(int Rows);		// // //
	float		 GetCurrentBPM() const;		// // //
	bool		 IsPlaying() const;

	CArpeggiator &GetArpeggiator();		// // //

	// Stats
	unsigned int GetFrameRate();

	// Tracker playing
	stDPCMState	 GetDPCMState() const;
	int			 GetChannelNote(int chan) const;		// // //
	int			 GetChannelVolume(int chan) const;		// // //

	// Rendering
	bool		 RenderToFile(LPCTSTR pFile, const std::shared_ptr<CWaveRenderer> &pRender);		// // //
	bool		 IsRendering() const;
	bool		 IsBackgroundTask() const;

	// Sample previewing
	void		 PreviewSample(std::shared_ptr<const ft0cc::doc::dpcm_sample> pSample, int Offset, int Pitch);		// // //
	void		 CancelPreviewSample();
	bool		 PreviewDone() const;

	void		 WriteAPU(int Address, char Value);

	// Other
	bool		IsExpansionEnabled(int Chip) const;		// // //
	int			GetNamcoChannelCount() const;		// // //

	uint8_t		GetReg(int Chip, int Reg) const;
	CRegisterState *GetRegState(unsigned Chip, unsigned Reg) const;		// // //
	double		GetChannelFrequency(unsigned Chip, int Channel) const;		// // //
	std::string	RecallChannelState(int Channel) const;		// // //

	// FDS & N163 wave preview
	void		WaveChanged();
	bool		HasWaveChanged() const;
	void		ResetWaveChanged();

	void		SetNamcoMixing(bool bLinear);			// // //

	// Player
	std::pair<unsigned, unsigned> GetPlayerPos() const;		// // // frame / row
	int			GetPlayerTrack() const;
	int			GetPlayerTicks() const;
	void		QueueNote(int Channel, const stChanNote &NoteData, note_prio_t Priority) const;		// // //
	void		ForceReloadInstrument(int Channel);		// // //
	void		MoveToFrame(int Frame);
	void		SetQueueFrame(unsigned Frame);		// // //
	unsigned	GetQueueFrame() const;		// // //

	// // // Instrument recorder
	CInstrument		*GetRecordInstrument() const;
	void			ResetDumpInstrument();
	int				GetRecordChannel() const;
	void			SetRecordChannel(int Channel);
	const stRecordSetting &GetRecordSetting() const;
	void			SetRecordSetting(const stRecordSetting &Setting);

	bool HasDocument() const { return m_pDocument != NULL; };
	CFamiTrackerDoc *GetDocument() const { return m_pDocument; };

	// Sequence play position
	void SetSequencePlayPos(std::shared_ptr<const CSequence> pSequence, int Pos);		// // //
	int GetSequencePlayPos(std::shared_ptr<const CSequence> pSequence);		// // //

	void SetMeterDecayRate(decay_rate_t Type) const;		// // // 050B
	decay_rate_t GetMeterDecayRate() const;		// // // 050B

	int GetDefaultInstrument() const;

	//
	// Private functions
	//
private:
	// Internal initialization
	void		ResetAPU();

	// Audio
	bool		ResetAudioDevice();
	void		CloseAudio();

	bool		PlayBuffer() override;

	void		StartRendering();		// // //
	void		StopRendering();		// // //

	// Player
	void		UpdateAPU();
	void		ResetBuffer();
	void		BeginPlayer(std::unique_ptr<CPlayerCursor> Pos);		// // //
	void		HaltPlayer();
	void		MakeSilent();

	bool		is_rendering_impl() const;		// // //

	// Misc
	void		PlayPreviewSample(int Offset, int Pitch);		// // //

	// Player
	double		GetAverageBPM() const;		// // //

	void		ApplyGlobalState();		// // //

	// // // CSoundGenBase impl
	CFTMComponentInterface *GetDocumentInterface() const override;
	void		OnTick() override;
	void		OnStepRow() override;
	void		OnPlayNote(int chan, const stChanNote &note) override;
	void		OnUpdateRow(int frame, int row) override;
	bool		ShouldStopPlayer() const override;
	int			GetArpNote(int chan) const override; // TODO: remove

	//
	// Private variables
	//
private:
	// Objects
	CFamiTrackerDoc		*m_pDocument = nullptr;
	CFamiTrackerView	*m_pTrackerView = nullptr;

	// Sound
	std::unique_ptr<CDSound>		m_pDSound;		// // //
	std::unique_ptr<CAudioDriver>	m_pAudioDriver;			// // //
	std::unique_ptr<CAPU>			m_pAPU;

	std::shared_ptr<const ft0cc::doc::dpcm_sample> m_pPreviewSample;
	CVisualizerWnd					*m_pVisualizerWnd = nullptr;

	bool				m_bRunning;

	// Thread synchronization
private:
	mutable CCriticalSection m_csAPULock;		// // //
	mutable CCriticalSection m_csVisualizerWndLock;
	mutable CCriticalSection m_csRenderer;		// // //

	// Handles
	HANDLE				m_hInterruptEvent;					// Used to interrupt sound buffer syncing

// Tracker playing variables
private:
	std::shared_ptr<CTempoCounter> m_pTempoCounter;			// // // tempo calculation
	std::unique_ptr<CSoundDriver> m_pSoundDriver;			// // // main sound engine

	std::unique_ptr<CTempoDisplay> m_pTempoDisplay;			// // // 050B
	bool				m_bHaltRequest;						// True when a halt is requested
	int					m_iFrameCounter;

	int					m_iUpdateCycles;					// Number of cycles/APU update

	int					m_iLastTrack = 0;					// // //
	int					m_iLastHighlight;					// // //

	machine_t			m_iMachineType;						// // // NTSC/PAL

	std::unique_ptr<CArpeggiator> m_pArpeggiator;			// // //

	std::shared_ptr<CWaveRenderer> m_pWaveRenderer;			// // //
	std::unique_ptr<CInstrumentRecorder> m_pInstRecorder;

	std::array<bool, MAX_CHANNELS> muted_ = { };			// // //

	// FDS & N163 waves
	volatile bool		m_bWaveChanged;
	volatile bool		m_bInternalWaveChanged;

	// Sequence play visualization
	std::shared_ptr<const CSequence> m_pSequencePlayPos;		// // //
	int					m_iSequencePlayPos;
	int					m_iSequenceTimeout;

	// Overloaded functions
public:
	virtual BOOL InitInstance();
	virtual int	 ExitInstance();
	virtual BOOL OnIdle(LONG lCount);
	BOOL IdleLoop();		// // //

	// Implementation
public:
	DECLARE_MESSAGE_MAP()
	afx_msg void OnSilentAll(WPARAM wParam, LPARAM lParam);
	afx_msg void OnLoadSettings(WPARAM wParam, LPARAM lParam);
	afx_msg void OnStartPlayer(WPARAM wParam, LPARAM lParam);
	afx_msg void OnStopPlayer(WPARAM wParam, LPARAM lParam);
	afx_msg void OnResetPlayer(WPARAM wParam, LPARAM lParam);
	afx_msg void OnStartRender(WPARAM wParam, LPARAM lParam);
	afx_msg void OnStopRender(WPARAM wParam, LPARAM lParam);
	afx_msg void OnPreviewSample(WPARAM wParam, LPARAM lParam);
	afx_msg void OnHaltPreview(WPARAM wParam, LPARAM lParam);
	afx_msg void OnWriteAPU(WPARAM wParam, LPARAM lParam);
	afx_msg void OnCloseSound(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSetChip(WPARAM wParam, LPARAM lParam);
	afx_msg void OnRemoveDocument(WPARAM wParam, LPARAM lParam);
};
