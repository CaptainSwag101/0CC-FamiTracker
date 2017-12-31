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

#include <vector>		// // //
#include <unordered_map>		// // //
#include "APU/Types.h"		// // //

class CTrackerChannel;		// // //

// CChannelMap

class CChannelMap
{
public:
	CChannelMap();		// // //
	CChannelMap(unsigned chips, unsigned n163chs);

	void			ResetChannels();
	void			RegisterChannel(CTrackerChannel &Channel);		// // //
	bool			SupportsChannel(const CTrackerChannel &ch) const;		// // //

	CTrackerChannel	&GetChannel(int Index) const;		// // //
	int				GetChannelIndex(int Channel) const;
	int				GetChannelCount() const;		// // //
	int				GetChannelType(int Channel) const;
	int				GetChipType(int Channel) const;

	unsigned		GetExpansionFlag() const noexcept;		// // //
	unsigned		GetChipChannelCount(unsigned chip) const;
	bool			HasExpansionChip(unsigned chips) const noexcept; // all

private:		// // //
	std::vector<CTrackerChannel *> m_pChannels;		// // //
	std::unordered_map<int, int> m_iChannelIndices;		// // //

	unsigned chips_;		// // //
	unsigned n163chs_;		// // //
};
