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

#include "Sequence.h"
#include <cstring>		// // //
#include <stdexcept>		// // //
#include <algorithm>		// // //

bool CSequence::operator==(const CSequence &other)		// // //
{
	return m_iItemCount == other.m_iItemCount &&
		m_iLoopPoint == other.m_iLoopPoint &&
		m_iReleasePoint == other.m_iReleasePoint &&
		m_iSetting == other.m_iSetting &&
		std::equal(m_cValues.cbegin(), m_cValues.cbegin() + m_iItemCount,
			other.m_cValues.cbegin(), other.m_cValues.cbegin() + m_iItemCount);
}

void CSequence::SetItem(int Index, int8_t Value)		// // //
{
	m_cValues[Index] = Value;
}

void CSequence::SetItemCount(unsigned int Count)
{
#ifdef _DEBUG
	if (Count > MAX_SEQUENCE_ITEMS)		// // //
		throw std::runtime_error {"CSequence size exceeded"};
#endif

	m_iItemCount = Count;

	if (m_iLoopPoint > m_iItemCount)
		m_iLoopPoint = -1;
	if (m_iReleasePoint > m_iItemCount)
		m_iReleasePoint = -1;
}

void CSequence::SetLoopPoint(unsigned int Point)
{
	m_iLoopPoint = Point;
	if (m_iLoopPoint > m_iItemCount)		// // //
		m_iLoopPoint = -1;
}

void CSequence::SetReleasePoint(unsigned int Point)
{
	m_iReleasePoint = Point;
	if (m_iReleasePoint > m_iItemCount)		// // //
		m_iReleasePoint = -1;
}

void CSequence::SetSetting(seq_setting_t Setting)		// // //
{
	m_iSetting = Setting;
}

int8_t CSequence::GetItem(int Index) const		// // //
{
	return m_cValues[Index];
}

unsigned int CSequence::GetItemCount() const
{
	return m_iItemCount;
}

unsigned int CSequence::GetLoopPoint() const
{
	return m_iLoopPoint;
}

unsigned int CSequence::GetReleasePoint() const
{
	return m_iReleasePoint;
}

seq_setting_t CSequence::GetSetting() const		// // //
{
	return m_iSetting;
}
