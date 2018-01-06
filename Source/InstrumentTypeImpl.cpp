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

#include "InstrumentTypeImpl.h"

#include "Instrument2A03.h"
#include "InstrumentVRC6.h"
#include "InstrumentFDS.h"
#include "InstrumentVRC7.h"
#include "InstrumentN163.h"
#include "InstrumentS5B.h"

template class CInstrumentTypeImpl<CInstrument2A03, INST_2A03>;
template class CInstrumentTypeImpl<CInstrumentVRC6, INST_VRC6>;
template class CInstrumentTypeImpl<CInstrumentVRC7, INST_VRC7>;
template class CInstrumentTypeImpl<CInstrumentFDS , INST_FDS >;
template class CInstrumentTypeImpl<CInstrumentN163, INST_N163>;
template class CInstrumentTypeImpl<CInstrumentS5B , INST_S5B >;
