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

#include "InstrumentManager.h"
#include "FTMComponentInterface.h"
#include "Instrument.h"
#include "SeqInstrument.h"
#include "InstrumentFactory.h"
#include "Sequence.h"
#include "SequenceCollection.h"
#include "SequenceManager.h"
#include "DSampleManager.h"

const int CInstrumentManager::MAX_INSTRUMENTS = 64;
const int CInstrumentManager::SEQ_MANAGER_COUNT = 5;

CInstrumentManager::CInstrumentManager(CFTMComponentInterface *pInterface) :
	m_pDSampleManager(std::make_unique<CDSampleManager>()),
	m_pInstruments(MAX_INSTRUMENTS),
	m_pDocInterface(pInterface)
{
	for (int i = 0; i < SEQ_MANAGER_COUNT; i++)
		m_pSequenceManager.push_back(std::make_unique<CSequenceManager>(i == 2 ? 3 : SEQ_COUNT));
}

CInstrumentManager::~CInstrumentManager()
{
	for (auto &ptr : m_pInstruments)
		if (ptr)
			ptr->RegisterManager(nullptr);
}

//
// Instrument methods
//

std::shared_ptr<CInstrument> CInstrumentManager::GetInstrument(unsigned int Index) const
{
	std::lock_guard<std::mutex> lock(m_InstrumentLock);
	return m_pInstruments[Index];
}

std::unique_ptr<CInstrument> CInstrumentManager::CreateNew(inst_type_t InstType)
{
	return FTExt::InstrumentFactory::Make(InstType);
}

bool CInstrumentManager::InsertInstrument(unsigned int Index, std::shared_ptr<CInstrument> pInst)
{
	std::lock_guard<std::mutex> lock(m_InstrumentLock);
	if (Index >= m_pInstruments.size())
		return false;
	if (m_pInstruments[Index] != pInst) {
		if (m_pInstruments[Index])
			m_pInstruments[Index]->RegisterManager(nullptr);
		m_pInstruments[Index] = pInst;
		pInst->RegisterManager(this);
		return true;
	}
	return false;
}

bool CInstrumentManager::RemoveInstrument(unsigned int Index)
{
	std::lock_guard<std::mutex> lock(m_InstrumentLock);
	if (Index >= m_pInstruments.size())
		return false;
	if (m_pInstruments[Index]) {
		m_pInstruments[Index]->RegisterManager(nullptr);
		m_pInstruments[Index].reset();
		return true;
	}
	return false;
}

void CInstrumentManager::SwapInstruments(unsigned int IndexA, unsigned int IndexB) {
	m_pInstruments[IndexA].swap(m_pInstruments[IndexB]);
}

void CInstrumentManager::ClearAll()
{
	std::lock_guard<std::mutex> lock(m_InstrumentLock);
	for (auto &ptr : m_pInstruments) {
		if (ptr)
			ptr->RegisterManager(nullptr);
		ptr.reset();
	}
	for (int i = 0; i < SEQ_MANAGER_COUNT; i++)
		m_pSequenceManager[i].swap(std::make_unique<CSequenceManager>(i == 2 ? 3 : SEQ_COUNT));
	m_pDSampleManager.swap(std::make_unique<CDSampleManager>());
}

bool CInstrumentManager::IsInstrumentUsed(unsigned int Index) const
{
	return Index < MAX_INSTRUMENTS && m_pInstruments[Index];
}

unsigned int CInstrumentManager::GetInstrumentCount() const
{
	unsigned x = 0;
	for (int i = 0; i < MAX_INSTRUMENTS; i++)
		if (m_pInstruments[i])
			++x;
	return x;
}

unsigned int CInstrumentManager::GetFirstUnused() const
{
	for (int i = 0; i < MAX_INSTRUMENTS; i++)
		if (!m_pInstruments[i])
			return i;
	return -1;
}

int CInstrumentManager::GetFreeSequenceIndex(inst_type_t InstType, int Type, CSeqInstrument *pInst) const
{
	// moved from CFamiTrackerDoc
	std::vector<bool> Used(CSequenceCollection::MAX_SEQUENCES, false);
	for (int i = 0; i < MAX_INSTRUMENTS; i++) if (GetInstrumentType(i) == InstType) {		// // //
		auto pInstrument = std::static_pointer_cast<CSeqInstrument>(GetInstrument(i));
		if (pInstrument->GetSeqEnable(Type) && (pInst && pInst->GetSequence(Type)->GetItemCount() || pInst != pInstrument.get()))
			Used[pInstrument->GetSeqIndex(Type)] = true;
	}
	for (int i = 0; i < CSequenceCollection::MAX_SEQUENCES; ++i) if (!Used[i]) {
		const CSequence *pSeq = GetSequence(InstType, Type, i);
		if (!pSeq || !pSeq->GetItemCount())
			return i;
	}
	return -1;
}

inst_type_t CInstrumentManager::GetInstrumentType(unsigned int Index) const
{
	return !IsInstrumentUsed(Index) ? INST_NONE : m_pInstruments[Index]->GetType();
}

void CInstrumentManager::CloneInstrumentShallow(unsigned int Old, unsigned int New)
{
}

void CInstrumentManager::CloneInstrumentDeep(unsigned int Old, unsigned int New)
{
}

//
// Sequence methods
//

CSequenceManager *const CInstrumentManager::GetSequenceManager(int InstType) const
{
	int Index = -1;
	switch (InstType) {
	case INST_2A03: Index = 0; break;
	case INST_VRC6: Index = 1; break;
	case INST_FDS:  Index = 2; break;
	case INST_N163: Index = 3; break;
	case INST_S5B:  Index = 4; break;
	default: return nullptr;
	}
	return m_pSequenceManager[Index].get();
}

CDSampleManager *const CInstrumentManager::GetDSampleManager() const
{
	return m_pDSampleManager.get();
}

//
// from interface
//

CSequence *CInstrumentManager::GetSequence(int InstType, int SeqType, int Index) const
{
	if (auto pManager = GetSequenceManager(InstType))
		if (auto pCol = pManager->GetCollection(SeqType))
			return pCol->GetSequence(Index);
	return nullptr;
}

void CInstrumentManager::SetSequence(int InstType, int SeqType, int Index, CSequence *pSeq)
{
	if (auto pManager = GetSequenceManager(InstType))
		if (auto pCol = pManager->GetCollection(SeqType))
			pCol->SetSequence(Index, pSeq);
}

int CInstrumentManager::AddSequence(int InstType, int SeqType, CSequence *pSeq, CSeqInstrument *pInst)
{
	int Index = GetFreeSequenceIndex(static_cast<inst_type_t>(InstType), SeqType, pInst);
	if (Index != -1)
		SetSequence(InstType, SeqType, Index, pSeq);
	return Index;
}

const CDSample *CInstrumentManager::GetDSample(int Index) const
{
	return m_pDSampleManager->GetDSample(Index);
}

void CInstrumentManager::SetDSample(int Index, CDSample *pSamp)
{
	m_pDSampleManager->SetDSample(Index, pSamp);
}

int CInstrumentManager::AddDSample(CDSample *pSamp)
{
	int Index = m_pDSampleManager->GetFirstFree();
	if (Index != -1)
		SetDSample(Index, pSamp);
	return Index;
}

void CInstrumentManager::InstrumentChanged() const
{
	if (m_pDocInterface)
		m_pDocInterface->ModifyIrreversible();
}
