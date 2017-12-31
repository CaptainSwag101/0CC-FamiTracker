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

#include <memory>		// // //
#include <string>		// // //

// DPCM sample class

class CDSample {
public:
	// Constructor
	explicit CDSample(unsigned int Size = 0U);		// // //
	CDSample(unsigned int Size, std::unique_ptr<char[]> pData);		// // //

	// Copy constructor
	CDSample(const CDSample &sample);
	CDSample(CDSample &&sample) = default;
	CDSample &operator=(const CDSample &sample);
	CDSample &operator=(CDSample &&sample);		// // //

	bool operator==(const CDSample &other) const;		// // //
	bool operator!=(const CDSample &other) const { return !(*this == other); }

	// Set sample data and size, the object will own the memory area assigned
	void SetData(unsigned int Size, std::unique_ptr<char[]> pData);		// // //

	// Get sample size
	unsigned int GetSize() const;

	// Get sample data
	const char *GetData() const;

	// Set sample name
	void SetName(const char *pName);

	// Get sample name
	const char *GetName() const;

	void RemoveData(int startsample, int endsample);		// // //
	void Tilt(int startsample, int endsample);		// // //

public:
	// Max size of a sample as supported by the NES, in bytes
	static const int MAX_SIZE = 0x0FF1;
	// Size of sample name
	static const int MAX_NAME_SIZE = 256;

private:
	// Sample data
	unsigned int m_iSampleSize;
	std::unique_ptr<char[]> m_pSampleData;		// // //
	std::string m_sName;		// // //
};
