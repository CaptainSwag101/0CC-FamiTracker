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

#include <utility>		// // //

// // // common classes for frame editor

class CFamiTrackerDoc;		// // //
class CFrameClipData;		// // //

// // // Frame editor cursor
struct CFrameCursorPos
{
	CFrameCursorPos() = default;
	CFrameCursorPos(int Frame, int Channel) : m_iFrame(Frame), m_iChannel(Channel) { }

	int m_iFrame = 0;
	int m_iChannel = 0;
};

// // // Frame editor selection
struct CFrameSelection
{
	CFrameSelection() = default;
	CFrameSelection(const CFrameCursorPos &cursor);
	CFrameSelection(const CFrameCursorPos &b, const CFrameCursorPos &e);
	CFrameSelection(const CFrameClipData &clipdata, int frame);

	int GetFrameStart() const;
	int GetFrameEnd() const;
	int GetChanStart() const;
	int GetChanEnd() const;

	void Normalize(CFrameCursorPos &Begin, CFrameCursorPos &End) const;		// // //
	CFrameSelection GetNormalized() const;		// // //

	CFrameCursorPos m_cpStart;
	CFrameCursorPos m_cpEnd;
};

struct CFrameIterator : public CFrameCursorPos		// // //
{
public:
	CFrameIterator(const CFrameIterator &it);
	CFrameIterator(CFamiTrackerDoc *const pDoc, int Track, const CFrameCursorPos &Pos);
	CFrameIterator(const CFamiTrackerDoc *const pDoc, int Track, const CFrameCursorPos &Pos);

	static std::pair<CFrameIterator, CFrameIterator> FromSelection(const CFrameSelection &Sel, CFamiTrackerDoc *const pDoc, int Track);
	
	int Get(int Channel) const; // use int& output parameter?
	void Set(int Channel, int Pattern);

	CFrameIterator &operator+=(const int Frames);
	CFrameIterator &operator-=(const int Frames);
	CFrameIterator &operator++();
	CFrameIterator operator++(int);
	CFrameIterator &operator--();
	CFrameIterator operator--(int);
	bool operator==(const CFrameIterator &other) const;
	bool operator!=(const CFrameIterator &other) const;

private:
	int NormalizeFrame(int Frame) const;

public:
	int m_iTrack;

private:
	CFamiTrackerDoc *m_pDocument;
};
