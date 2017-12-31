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

#include "PatternEditorTypes.h"
#include "FamiTrackerDoc.h"
#include "PatternNote.h"		// // //

// CCursorPos /////////////////////////////////////////////////////////////////////

CCursorPos::CCursorPos() : m_iRow(0), m_iChannel(0), m_iColumn(C_NOTE), m_iFrame(0)		// // //
{
}

CCursorPos::CCursorPos(int Row, int Channel, cursor_column_t Column, int Frame) :		// // //
	m_iRow(Row), m_iChannel(Channel), m_iColumn(Column), m_iFrame(Frame)
{
}

bool CCursorPos::operator!=(const CCursorPos &other) const
{
	// Unequality check
	return (m_iRow != other.m_iRow) || (m_iChannel != other.m_iChannel)		// // //
		|| (m_iColumn != other.m_iColumn) || (m_iFrame != other.m_iFrame);
}

bool CCursorPos::operator<(const CCursorPos &other) const		// // //
{
	return m_iFrame < other.m_iFrame || (m_iFrame == other.m_iFrame && m_iRow < other.m_iRow);
}

bool CCursorPos::operator<=(const CCursorPos &other) const		// // //
{
	return !(other < *this);
}

bool CCursorPos::IsValid(int RowCount, int ChannelCount) const		// // //
{
	// Check if a valid pattern position
	//if (m_iFrame < -FrameCount || m_iFrame >= 2 * FrameCount)		// // //
	//	return false;
	if (m_iChannel < 0 || m_iChannel >= ChannelCount)
		return false;
	if (m_iRow < 0 || m_iRow >= RowCount)
		return false;
	if (m_iColumn < C_NOTE || m_iColumn > C_EFF4_PARAM2)		// // //
		return false;

	return true;
}

// CSelection /////////////////////////////////////////////////////////////////////

int CSelection::GetRowStart() const
{
	if (m_cpEnd.m_iFrame > m_cpStart.m_iFrame)		// // //
		return m_cpStart.m_iRow;
	if (m_cpEnd.m_iFrame < m_cpStart.m_iFrame)
		return m_cpEnd.m_iRow;

	return (m_cpEnd.m_iRow > m_cpStart.m_iRow ?  m_cpStart.m_iRow : m_cpEnd.m_iRow);
}

int CSelection::GetRowEnd() const
{
	if (m_cpEnd.m_iFrame > m_cpStart.m_iFrame)		// // //
		return m_cpEnd.m_iRow;
	if (m_cpEnd.m_iFrame < m_cpStart.m_iFrame)
		return m_cpStart.m_iRow;

	return (m_cpEnd.m_iRow > m_cpStart.m_iRow ? m_cpEnd.m_iRow : m_cpStart.m_iRow);
}

cursor_column_t CSelection::GetColStart() const
{
	cursor_column_t Col = C_NOTE;
	if (m_cpStart.m_iChannel == m_cpEnd.m_iChannel)
		Col = (m_cpEnd.m_iColumn > m_cpStart.m_iColumn ? m_cpStart.m_iColumn : m_cpEnd.m_iColumn);
	else if (m_cpEnd.m_iChannel > m_cpStart.m_iChannel)
		Col = m_cpStart.m_iColumn;
	else
		Col = m_cpEnd.m_iColumn;
	switch (Col) {
		case C_INSTRUMENT2: Col = C_INSTRUMENT1; break;
		case C_EFF1_PARAM1: case C_EFF1_PARAM2: Col = C_EFF1_NUM; break;
		case C_EFF2_PARAM1: case C_EFF2_PARAM2: Col = C_EFF2_NUM; break;
		case C_EFF3_PARAM1: case C_EFF3_PARAM2: Col = C_EFF3_NUM; break;
		case C_EFF4_PARAM1: case C_EFF4_PARAM2: Col = C_EFF4_NUM; break;
	}
	return Col;
}

cursor_column_t CSelection::GetColEnd() const
{
	cursor_column_t Col = C_NOTE;
	if (m_cpStart.m_iChannel == m_cpEnd.m_iChannel)
		Col = (m_cpEnd.m_iColumn > m_cpStart.m_iColumn ? m_cpEnd.m_iColumn : m_cpStart.m_iColumn);
	else if (m_cpEnd.m_iChannel > m_cpStart.m_iChannel)
		Col = m_cpEnd.m_iColumn;
	else
		Col = m_cpStart.m_iColumn;
	switch (Col) {
		case C_INSTRUMENT1: Col = C_INSTRUMENT2; break;						// Instrument
		case C_EFF1_NUM: case C_EFF1_PARAM1: Col = C_EFF1_PARAM2; break;	// Eff 1
		case C_EFF2_NUM: case C_EFF2_PARAM1: Col = C_EFF2_PARAM2; break;	// Eff 2
		case C_EFF3_NUM: case C_EFF3_PARAM1: Col = C_EFF3_PARAM2; break;	// Eff 3
		case C_EFF4_NUM: case C_EFF4_PARAM1: Col = C_EFF4_PARAM2; break;	// Eff 4
	}
	return Col;
}

int CSelection::GetChanStart() const
{
	return (m_cpEnd.m_iChannel > m_cpStart.m_iChannel) ? m_cpStart.m_iChannel : m_cpEnd.m_iChannel;
}

int CSelection::GetChanEnd() const
{
	return (m_cpEnd.m_iChannel > m_cpStart.m_iChannel) ? m_cpEnd.m_iChannel : m_cpStart.m_iChannel;
}

int CSelection::GetFrameStart() const		// // //
{
	return (m_cpEnd.m_iFrame > m_cpStart.m_iFrame) ? m_cpStart.m_iFrame : m_cpEnd.m_iFrame;
}

int CSelection::GetFrameEnd() const		// // //
{
	return (m_cpEnd.m_iFrame > m_cpStart.m_iFrame) ? m_cpEnd.m_iFrame : m_cpStart.m_iFrame;
}

bool CSelection::IsSameStartPoint(const CSelection &selection) const
{
	return GetChanStart() == selection.GetChanStart() &&
		GetRowStart() == selection.GetRowStart() &&
		GetColStart() == selection.GetColStart() &&
		GetFrameStart() == selection.GetFrameStart();		// // //
}

bool CSelection::IsColumnSelected(column_t Column, int Channel) const
{
	column_t SelStart = GetSelectColumn(GetColStart());		// // //
	column_t SelEnd = GetSelectColumn(GetColEnd());

	return (Channel > GetChanStart() || (Channel == GetChanStart() && Column >= SelStart))		// // //
		&& (Channel < GetChanEnd() || (Channel == GetChanEnd() && Column <= SelEnd));
}

void CSelection::Normalize(CCursorPos &Begin, CCursorPos &End) const		// // //
{
	CCursorPos Temp {GetRowStart(), GetChanStart(), GetColStart(), GetFrameStart()};
	End = CCursorPos {GetRowEnd(), GetChanEnd(), GetColEnd(), GetFrameEnd()};
	Begin = Temp;
}

CSelection CSelection::GetNormalized() const
{
	CSelection Sel;
	Normalize(Sel.m_cpStart, Sel.m_cpEnd);
	return Sel;
}

// // // CPatternIterator //////////////////////////////////////////////////////

CPatternIterator::CPatternIterator(const CPatternIterator &it) :
	m_iTrack(it.m_iTrack), m_pDocument(it.m_pDocument), CCursorPos(static_cast<const CCursorPos &>(it))
{
}

CPatternIterator::CPatternIterator(CFamiTrackerDoc *pDoc, int Track, const CCursorPos &Pos) :
	m_iTrack(Track), m_pDocument(pDoc), CCursorPos(Pos)
{
	Warp();
}

std::pair<CPatternIterator, CPatternIterator> CPatternIterator::FromCursor(const CCursorPos &Pos, CFamiTrackerDoc *const pDoc, int Track)
{
	return std::make_pair(
		CPatternIterator {pDoc, Track, Pos},
		CPatternIterator {pDoc, Track, Pos}
	);
}

std::pair<CPatternIterator, CPatternIterator> CPatternIterator::FromSelection(const CSelection &Sel, CFamiTrackerDoc *const pDoc, int Track)
{
	CCursorPos it, end;
	Sel.Normalize(it, end);
	return std::make_pair(
		CPatternIterator {pDoc, Track, it},
		CPatternIterator {pDoc, Track, end}
	);
}

const stChanNote &CPatternIterator::Get(int Channel) const
{
	int Frame = m_iFrame % m_pDocument->GetFrameCount(m_iTrack);
	if (Frame < 0) Frame += m_pDocument->GetFrameCount(m_iTrack);
	return m_pDocument->GetNoteData(m_iTrack, Frame, Channel, m_iRow);
}

void CPatternIterator::Set(int Channel, const stChanNote &Note)
{
	int Frame = m_iFrame % m_pDocument->GetFrameCount(m_iTrack);
	if (Frame < 0) Frame += m_pDocument->GetFrameCount(m_iTrack);
	m_pDocument->SetNoteData(m_iTrack, Frame, Channel, m_iRow, Note);
}

void CPatternIterator::Step() // resolves skip effects
{
	for (int i = m_pDocument->GetChannelCount() - 1; i >= 0; --i) {
		const auto &Note = Get(i);
		for (int c = m_pDocument->GetEffColumns(m_iTrack, i); c >= 0; --c) {
			if (Note.EffNumber[c] == EF_JUMP) {
				m_iFrame = Note.EffParam[c];
				if (m_iFrame >= static_cast<int>(m_pDocument->GetFrameCount(m_iTrack)))
					m_iFrame = m_pDocument->GetFrameCount(m_iTrack) - 1;
				m_iRow = 0;
				return;
			}
		}
	}
	for (int i = m_pDocument->GetChannelCount() - 1; i >= 0; i--) {
		const auto &Note = Get(i);
		for (int c = m_pDocument->GetEffColumns(m_iTrack, i); c >= 0; --c) {
			if (Note.EffNumber[c] == EF_SKIP) {
				++m_iFrame;
				m_iRow = 0;
				return;
			}
		}
	}
	++m_iRow;
	Warp();
}

CPatternIterator& CPatternIterator::operator+=(const int Rows)
{
	m_iRow += Rows;
	Warp();
	return *this;
}

CPatternIterator& CPatternIterator::operator-=(const int Rows)
{
	return operator+=(-Rows);
}

CPatternIterator& CPatternIterator::operator++()
{
	return operator+=(1);
}

CPatternIterator CPatternIterator::operator++(int)
{
	CPatternIterator tmp(*this);
	operator+=(1);
	return tmp;
}

CPatternIterator& CPatternIterator::operator--()
{
	return operator+=(-1);
}

CPatternIterator CPatternIterator::operator--(int)
{
	CPatternIterator tmp(*this);
	operator+=(-1);
	return tmp;
}

bool CPatternIterator::operator==(const CPatternIterator &other) const
{
	return m_iFrame == other.m_iFrame && m_iRow == other.m_iRow;
}

void CPatternIterator::Warp()
{
	while (true) {
		const int Length = m_pDocument->GetCurrentPatternLength(m_iTrack, m_iFrame);
		if (m_iRow >= Length) {
			m_iRow -= Length;
			m_iFrame++;
		}
		else break;
	}
	while (m_iRow < 0)
		m_iRow += m_pDocument->GetCurrentPatternLength(m_iTrack, --m_iFrame);
	//m_iFrame %= m_pDocument->GetFrameCount(m_iTrack);
	//if (m_iFrame < 0) m_iFrame += m_pDocument->GetFrameCount(m_iTrack);
}

// CPatternEditorLayout ////////////////////////////////////////////////////////
/*
CPatternEditorLayout::CPatternEditorLayout()
{
}

void CPatternEditorLayout::SetSize(int Width, int Height)
{
//	m_iWinWidth = width;
//	m_iWinHeight = height - ::GetSystemMetrics(SM_CYHSCROLL);
}

void CPatternEditorLayout::CalculateLayout()
{
	// Calculate pattern layout. Must be called when layout or window size has changed

}
*/
