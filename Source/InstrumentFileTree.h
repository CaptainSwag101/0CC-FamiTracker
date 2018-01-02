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

#include "stdafx.h"		// // //
#include <vector>		// // //
#include <memory>		// // //

// CInstrumentFileTree

class CInstrumentFileTree
{
public:
	bool BuildMenuTree(const CString &instrumentPath);		// // //
	CMenu &GetMenu();		// // //
	CString GetFile(int Index) const;
	bool ShouldRebuild() const;
	void Changed();

public:
	// Limits, to avoid very deep recursions
	static const int RECURSION_LIMIT = 6;
	static const int MAX_MENUS = 200;

	static const int MENU_BASE = 0x9000;	// Choose a range where no strings are located

	static const int CACHE_TIMEOUT = 60000;	// 1 minute

protected:
	bool ScanDirectory(const CString &path, CMenu &Menu, int level);		// // //

private:
	CMenu m_RootMenu;		// // //
	int m_iFileIndex = 0;
	std::vector<CString> m_fileList;		// // //
	std::vector<std::unique_ptr<CMenu>> m_menuArray;		// // //
	DWORD m_iTimeout;
	bool m_bShouldRebuild = true;
	int m_iTotalMenusAdded;
};
