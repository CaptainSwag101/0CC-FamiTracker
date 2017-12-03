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


#pragma once

#include <array>		// // //
#include <string>		// // //
#include "FamiTrackerTypes.h"		// // //
#include "PatternData.h"		// // //
#include "Highlight.h"		// // //
#include "BookmarkCollection.h"		// // //

class stChanNote;		// // //

// CSongData holds all notes in the patterns
class CSongData
{
public:
	CSongData(unsigned int PatternLength = DEFAULT_ROW_COUNT);		// // //

	bool IsCellFree(unsigned int Channel, unsigned int Pattern, unsigned int Row) const;
	bool IsPatternEmpty(unsigned int Channel, unsigned int Pattern) const;
	bool IsPatternInUse(unsigned int Channel, unsigned int Pattern) const;
	bool ArePatternsSame(unsigned ch1, unsigned pat1, unsigned ch2, unsigned pat2) const;		// // //

	void ClearPattern(unsigned int Channel, unsigned int Pattern);

	stChanNote &GetPatternData(unsigned Channel, unsigned Pattern, unsigned Row);		// // //
	const stChanNote &GetPatternData(unsigned Channel, unsigned Pattern, unsigned Row) const;		// // //
	void SetPatternData(unsigned Channel, unsigned Pattern, unsigned Row, const stChanNote &Note);		// // //

	CPatternData &GetPattern(unsigned Channel, unsigned Pattern);		// // //
	const CPatternData &GetPattern(unsigned Channel, unsigned Pattern) const;		// // //
	CPatternData &GetPatternOnFrame(unsigned Channel, unsigned Frame);		// // //
	const CPatternData &GetPatternOnFrame(unsigned Channel, unsigned Frame) const;		// // //

	const std::string &GetTitle() const;		// // //
	unsigned int GetPatternLength() const;
	unsigned int GetFrameCount() const;
	unsigned int GetSongSpeed() const;
	unsigned int GetSongTempo() const;
	int GetEffectColumnCount(int Channel) const;;
	bool GetSongGroove() const;		// // //

	void SetTitle(const std::string &str);		// // //
	void SetPatternLength(unsigned int Length);
	void SetFrameCount(unsigned int Count);
	void SetSongSpeed(unsigned int Speed);
	void SetSongTempo(unsigned int Tempo);
	void SetEffectColumnCount(int Channel, int Count);;
	void SetSongGroove(bool Groove);		// // //

	unsigned int GetFramePattern(unsigned int Frame, unsigned int Channel) const;
	void SetFramePattern(unsigned int Frame, unsigned int Channel, unsigned int Pattern);

	unsigned GetFrameSize(unsigned Frame, unsigned MaxChans) const;		// // //

	void SetHighlight(const stHighlight &Hl);		// // //
	const stHighlight &GetRowHighlight() const;

	void CopyTrack(unsigned Chan, const CSongData &From, unsigned ChanFrom);		// // //
	void SwapChannels(unsigned int First, unsigned int Second);		// // //

	CBookmarkCollection &GetBookmarks();		// // //
	const CBookmarkCollection &GetBookmarks() const;
	void SetBookmarks(const CBookmarkCollection &bookmarks);
	void SetBookmarks(CBookmarkCollection &&bookmarks);

	// void (*F)(CPatternData &pat [, unsigned ch, unsigned pat_index])
	template <typename F>
	void VisitPatterns(F f) {		// // //
		if constexpr (std::is_invocable_v<F, CPatternData &>)
			visit_patterns_impl(f);
		else
			visit_patterns_impl2(f);
	}

	// void (*F)(const CPatternData &pat [, unsigned ch, unsigned pat_index])
	template <typename F>
	void VisitPatterns(F f) const {
		if constexpr (std::is_invocable_v<F, const CPatternData &>)
			visit_patterns_impl(f);
		else
			visit_patterns_impl2(f);
	}

private:
	template <typename F>
	void visit_patterns_impl(F f) {
		for (auto &ch : m_pPatternData)
			for (auto &p : ch)
				f(p);
	}
	template <typename F>
	void visit_patterns_impl(F f) const {
		for (auto &ch : m_pPatternData)
			for (auto &p : ch)
				f(p);
	}
	template <typename F>
	void visit_patterns_impl2(F f) {
		unsigned ch_index = 0;
		for (auto &ch : m_pPatternData) {
			unsigned p_index = 0;
			for (auto &p : ch)
				f(p, ch_index, p_index++);
			++ch_index;
		}
	}
	template <typename F>
	void visit_patterns_impl2(F f) const {
		unsigned ch_index = 0;
		for (auto &ch : m_pPatternData) {
			unsigned p_index = 0;
			for (auto &p : ch)
				f(p, ch_index, p_index++);
			++ch_index;
		}
	}

public:
	// // // moved from CFamiTrackerDoc
	static const std::string DEFAULT_TITLE;
	static const stHighlight DEFAULT_HIGHLIGHT;

private:
	static const unsigned DEFAULT_ROW_COUNT;

	// Track parameters
	std::string	 m_sTrackName;				// // // moved
	unsigned int m_iPatternLength;			// Amount of rows in one pattern
	unsigned int m_iFrameCount;				// Number of frames
	unsigned int m_iSongSpeed;				// Song speed
	unsigned int m_iSongTempo;				// Song tempo
	bool		 m_bUseGroove;				// // // Groove

	// Row highlight settings
	stHighlight  m_vRowHighlight;			// // //

	// Bookmarks
	CBookmarkCollection bookmarks_;		// // //

	// Number of visible effect columns for each channel
	std::array<unsigned char, MAX_CHANNELS> m_iEffectColumns = { };		// // //

	// List of the patterns assigned to frames
	std::array<std::array<unsigned char, MAX_CHANNELS>, MAX_FRAMES> m_iFrameList = { };		// // //

	// All accesses to m_pPatternData must go through GetPatternData()
	std::array<std::array<CPatternData, MAX_PATTERN>, MAX_CHANNELS> m_pPatternData;		// // //
};
