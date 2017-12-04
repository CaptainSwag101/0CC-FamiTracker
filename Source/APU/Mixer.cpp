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

 This will mix and synthesize the APU audio using blargg's blip-buffer

 Mixing of internal audio relies on Blargg's findings

 Mixing of external channles are based on my own research:

 VRC6 (Madara): 
	Pulse channels has the same amplitude as internal-
	pulse channels on equal volume levels.

 FDS: 
	Square wave @ v = $1F: 2.4V
	  			  v = $0F: 1.25V
	(internal square wave: 1.0V)

 MMC5 (just breed): 
	2A03 square @ v = $0F: 760mV (the cart attenuates internal channels a little)
	MMC5 square @ v = $0F: 900mV

 VRC7:
	2A03 Square  @ v = $0F: 300mV (the cart attenuates internal channels a lot)
	VRC7 Patch 5 @ v = $0F: 900mV
	Did some more tests and found patch 14 @ v=15 to be 13.77dB stronger than a 50% square @ v=15

 ---

 N163 & 5B are still unknown

*/

#include "../stdafx.h"
#include <memory>
#include <cmath>
#include "Mixer.h"
#include "APU.h"
#include "ext/emu2413.h"		// // //

//#define LINEAR_MIXING

static const double AMP_2A03 = 400.0;

static const float LEVEL_FALL_OFF_RATE	= 0.6f;
static const int   LEVEL_FALL_OFF_DELAY = 3;

CMixer::CMixer()
{
	memset(m_fChannelLevels, 0, sizeof(float) * CHANNELS);
	memset(m_iChanLevelFallOff, 0, sizeof(uint32_t) * CHANNELS);

	m_fLevelAPU1 = 1.0f;
	m_fLevelAPU2 = 1.0f;
	m_fLevelVRC6 = 1.0f;
	m_fLevelMMC5 = 1.0f;
	m_fLevelFDS = 1.0f;
	m_fLevelN163 = 1.0f;
	m_fLevelS5B = 1.0f;		// // // 050B

	m_iExternalChip = 0;
	m_iSampleRate = 0;
	m_iLowCut = 0;
	m_iHighCut = 0;
	m_iHighDamp = 0;
	m_fOverallVol = 1.0f;

	levels2A03SS_.ResetDelta();		// // //
	levels2A03TND_.ResetDelta();		// // //

	m_iMeterDecayRate = DECAY_SLOW;		// // // 050B
	m_bNamcoMixing = false;		// // //
}

CMixer::~CMixer()
{
}

inline double CMixer::stLevels2A03SS::CalcPin()
{
#ifdef LINEAR_MIXING
	double SumL = (sq1_.Left  + sq2_.Left ) * 0.00752 * InternalVol;
	double SumR = (sq1_.Right + sq2_.Right) * 0.00752 * InternalVol;
#endif
	if ((sq1_ + sq2_) > 0)
		return AMP_2A03 * 95.88 / (100.0 + 8128.0 / (sq1_ + sq2_));
	return 0;
}

inline int CMixer::stLevels2A03SS::GetDelta(int ChanID, int Value) {
	switch (ChanID) {
	case CHANID_SQUARE1: sq1_ = Value; break;
	case CHANID_SQUARE2: sq2_ = Value; break;
	}
	double Sum = CalcPin();
	return static_cast<int>(Sum - std::exchange(lastSum_, Sum));
}

inline void CMixer::stLevels2A03SS::ResetDelta() {
	sq1_ = sq2_ = 0;
	lastSum_ = 0.;
}

inline double CMixer::stLevels2A03TND::CalcPin()
{
#ifdef LINEAR_MIXING
	double SumL = (0.00851 * tri_.Left  + 0.00494 * noi_.Left  + 0.00335 * dmc_.Left ) * InternalVol;
	double SumR = (0.00851 * tri_.Right + 0.00494 * noi_.Right + 0.00335 * dmc_.Right) * InternalVol;
#endif
	if ((tri_ + noi_ + dmc_) > 0)
		return AMP_2A03 * 159.79 / (100.0 + 1.0 / (tri_ / 8227.0 + noi_ / 12241.0 + dmc_ / 22638.0));
	return 0;
}

inline int CMixer::stLevels2A03TND::GetDelta(int ChanID, int Value) {
	switch (ChanID) {
	case CHANID_TRIANGLE: tri_ = Value; break;
	case CHANID_NOISE:    noi_ = Value; break;
	case CHANID_DPCM:     dmc_ = Value; break;
	}
	double Sum = CalcPin();
	return static_cast<int>(Sum - std::exchange(lastSum_, Sum));
}

inline void CMixer::stLevels2A03TND::ResetDelta() {
	tri_ = noi_ = dmc_ = 0;
	lastSum_ = 0.;
}

void CMixer::ExternalSound(int Chip)
{
	m_iExternalChip = Chip;
	UpdateSettings(m_iLowCut, m_iHighCut, m_iHighDamp, m_fOverallVol);
}

void CMixer::SetNamcoMixing(bool bLinear)		// // //
{
	m_bNamcoMixing = bLinear;
}

void CMixer::SetChipLevel(chip_level_t Chip, float Level)
{
	switch (Chip) {
		case CHIP_LEVEL_APU1:
			m_fLevelAPU1 = Level;
			break;
		case CHIP_LEVEL_APU2:
			m_fLevelAPU2 = Level;
			break;
		case CHIP_LEVEL_VRC6:
			m_fLevelVRC6 = Level;
			break;
		case CHIP_LEVEL_MMC5:
			m_fLevelMMC5 = Level;
			break;
		case CHIP_LEVEL_FDS:
			m_fLevelFDS = Level;
			break;
		case CHIP_LEVEL_N163:
			m_fLevelN163 = Level;
			break;
		case CHIP_LEVEL_S5B:		// // // 050B
			m_fLevelS5B = Level;
			break;
	}
}

float CMixer::GetAttenuation() const
{
	const float ATTENUATION_VRC6 = 0.80f;
	const float ATTENUATION_VRC7 = 0.64f;
	const float ATTENUATION_MMC5 = 0.83f;
	const float ATTENUATION_FDS  = 0.90f;
	const float ATTENUATION_N163 = 0.70f;
	const float ATTENUATION_S5B  = 0.50f;		// // // 050B

	float Attenuation = 1.0f;

	// Increase headroom if some expansion chips are enabled

	if (m_iExternalChip & SNDCHIP_VRC6)
		Attenuation *= ATTENUATION_VRC6;

	if (m_iExternalChip & SNDCHIP_VRC7)
		Attenuation *= ATTENUATION_VRC7;

	if (m_iExternalChip & SNDCHIP_MMC5)
		Attenuation *= ATTENUATION_MMC5;

	if (m_iExternalChip & SNDCHIP_FDS)
		Attenuation *= ATTENUATION_FDS;

	if (m_iExternalChip & SNDCHIP_N163)
		Attenuation *= ATTENUATION_N163;

	if (m_iExternalChip & SNDCHIP_S5B)		// // // 050B
		Attenuation *= ATTENUATION_S5B;

	return Attenuation;
}

void CMixer::UpdateSettings(int LowCut,	int HighCut, int HighDamp, float OverallVol)
{
	float Volume = OverallVol * GetAttenuation();

	// Blip-buffer filtering
	BlipBuffer.bass_freq(LowCut);

	blip_eq_t eq(-HighDamp, HighCut, m_iSampleRate);

	Synth2A03SS.treble_eq(eq);
	Synth2A03TND.treble_eq(eq);
	SynthVRC6.treble_eq(eq);
	SynthMMC5.treble_eq(eq);
	SynthS5B.treble_eq(eq);

	// N163 special filtering
	double n163_treble = 24;
	long n163_rolloff = 12000;

	if (HighDamp > n163_treble)
		n163_treble = HighDamp;

	if (n163_rolloff > HighCut)
		n163_rolloff = HighCut;

	blip_eq_t eq_n163(-n163_treble, n163_rolloff, m_iSampleRate);
	SynthN163.treble_eq(eq_n163);

	// FDS special filtering (TODO fix this for high sample rates)
	blip_eq_t fds_eq(-48, 1000, m_iSampleRate);

	SynthFDS.treble_eq(fds_eq);

	// Volume levels
	Synth2A03SS.volume(Volume * m_fLevelAPU1);
	Synth2A03TND.volume(Volume * m_fLevelAPU2);
	SynthVRC6.volume(Volume * 3.98333f * m_fLevelVRC6);
	SynthFDS.volume(Volume * 1.00f * m_fLevelFDS);
	SynthMMC5.volume(Volume * 1.18421f * m_fLevelMMC5);
	
	// Not checked
	SynthS5B.volume(Volume * m_fLevelS5B);		// // // 050B
	SynthN163.volume(Volume * 1.1f * m_fLevelN163);

	m_iLowCut = LowCut;
	m_iHighCut = HighCut;
	m_iHighDamp = HighDamp;
	m_fOverallVol = OverallVol;
}

void CMixer::SetNamcoVolume(float fVol)
{
	float fVolume = fVol * m_fOverallVol * GetAttenuation();

	SynthN163.volume(fVolume * 1.1f * m_fLevelN163);
}

int CMixer::GetMeterDecayRate() const		// // // 050B
{
	return m_iMeterDecayRate;
}

void CMixer::SetMeterDecayRate(int Rate)		// // // 050B
{
	m_iMeterDecayRate = Rate;
}

void CMixer::MixSamples(blip_sample_t *pBuffer, uint32_t Count)
{
	// For VRC7
	BlipBuffer.mix_samples(pBuffer, Count);
}

uint32_t CMixer::GetMixSampleCount(int t) const
{
	return BlipBuffer.count_samples(t);
}

bool CMixer::AllocateBuffer(unsigned int BufferLength, uint32_t SampleRate, uint8_t NrChannels)
{
	m_iSampleRate = SampleRate;
	BlipBuffer.set_sample_rate(SampleRate, (BufferLength * 1000 * 4) / SampleRate);		// // //
	return true;
}

void CMixer::SetClockRate(uint32_t Rate)
{
	// Change the clockrate
	BlipBuffer.clock_rate(Rate);
}

void CMixer::ClearBuffer()
{
	BlipBuffer.clear();

	levels2A03SS_.ResetDelta();		// // //
	levels2A03TND_.ResetDelta();		// // //
}

int CMixer::SamplesAvail() const
{	
	return (int)BlipBuffer.samples_avail();
}

int CMixer::FinishBuffer(int t)
{
	BlipBuffer.end_frame(t);

	for (int i = 0; i < CHANNELS; ++i) {
		// TODO: this is more complicated than 0.5.0 beta's implementation
		if (m_iChanLevelFallOff[i] > 0) {
			if (m_iMeterDecayRate == DECAY_FAST)		// // // 050B
				m_iChanLevelFallOff[i] = 0;
			else
				--m_iChanLevelFallOff[i];
		}
		else if (m_fChannelLevels[i] > 0) {
			if (m_iMeterDecayRate == DECAY_FAST)		// // // 050B
				m_fChannelLevels[i] = 0;
			else {
				m_fChannelLevels[i] -= LEVEL_FALL_OFF_RATE;
				if (m_fChannelLevels[i] < 0)
					m_fChannelLevels[i] = 0;
			}
		}
	}

	// Get channel levels for VRC7
	for (int i = 0; i < 6; ++i)
		StoreChannelLevel(CHANID_VRC7_CH1 + i, OPLL_getchanvol(i));

	// Return number of samples available
	return BlipBuffer.samples_avail();
}

//
// Mixing
//

void CMixer::AddValue(int ChanID, int Chip, int Delta, int Value, int FrameCycles) {
	switch (Chip) {
	case SNDCHIP_VRC6:
		SynthVRC6.offset(FrameCycles, Delta, &BlipBuffer); break;
	case SNDCHIP_FDS:
		SynthFDS.offset(FrameCycles, Delta, &BlipBuffer); break;
	case SNDCHIP_MMC5:
		SynthMMC5.offset(FrameCycles, Delta, &BlipBuffer); break;
	case SNDCHIP_N163:
		SynthN163.offset(FrameCycles, Delta, &BlipBuffer); break;
	case SNDCHIP_S5B:		// // // 050B
		SynthS5B.offset(FrameCycles, Delta, &BlipBuffer); break;
	}

	StoreChannelLevel(ChanID, Value);
}

void CMixer::AddValueTND(int ChanID, int Value, int FrameCycles) {		// // //
	int Delta = levels2A03TND_.GetDelta(ChanID, Value);
	Synth2A03TND.offset(FrameCycles, Delta, &BlipBuffer);
	StoreChannelLevel(ChanID, Value);
}

void CMixer::AddValueSS(int ChanID, int Value, int FrameCycles) {		// // //
	int Delta = levels2A03SS_.GetDelta(ChanID, Value);
	Synth2A03SS.offset(FrameCycles, Delta, &BlipBuffer);
	StoreChannelLevel(ChanID, Value);
}

int CMixer::ReadBuffer(int Size, void *Buffer, bool Stereo)
{
	return BlipBuffer.read_samples((blip_sample_t*)Buffer, Size);
}

int32_t CMixer::GetChanOutput(uint8_t Chan) const
{
	return (int32_t)m_fChannelLevels[Chan];
}

void CMixer::StoreChannelLevel(int Channel, int Value)
{
	int AbsVol = std::abs(Value);

	// Adjust channel levels for some channels
	if (Channel == CHANID_VRC6_SAWTOOTH)
		AbsVol = (AbsVol * 3) / 4;

	if (Channel == CHANID_DPCM)
		AbsVol /= 8;

	if (Channel == CHANID_FDS)
		AbsVol = AbsVol / 38;

	if (Channel >= CHANID_N163_CH1 && Channel <= CHANID_N163_CH8) {
		AbsVol /= 15;
		Channel = (7 - (Channel - CHANID_N163_CH1)) + CHANID_N163_CH1;
	}

	if (Channel >= CHANID_VRC7_CH1 && Channel <= CHANID_VRC7_CH6) {
		AbsVol = (int)(std::logf((float)AbsVol) * 3.0f);
	}

	if (Channel >= CHANID_S5B_CH1 && Channel <= CHANID_S5B_CH3) {
		AbsVol = (int)(std::logf((float)AbsVol) * 2.8f);
	}

	if (float(AbsVol) >= m_fChannelLevels[Channel]) {
		m_fChannelLevels[Channel] = float(AbsVol);
		m_iChanLevelFallOff[Channel] = LEVEL_FALL_OFF_DELAY;
	}
}

void CMixer::ClearChannelLevels()
{
	memset(m_fChannelLevels, 0, sizeof(float) * CHANNELS);
	memset(m_iChanLevelFallOff, 0, sizeof(uint32_t) * CHANNELS);
}

uint32_t CMixer::ResampleDuration(uint32_t Time) const
{
	return (uint32_t)BlipBuffer.resampled_duration((blip_time_t)Time);
}
