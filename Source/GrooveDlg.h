/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2015 Jonathan Liss
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

#include "stdafx.h"
#include "../resource.h"
#include "FamiTrackerTypes.h"
#include <memory>
#include <array>

class CFamiTrackerModule;

// CGrooveDlg dialog

namespace ft0cc::doc {
class groove;
} // namespace ft0cc::doc

class CGrooveDlg : public CDialog
{
	DECLARE_DYNAMIC(CGrooveDlg)

public:
	CGrooveDlg(CWnd* pParent = NULL);   // standard constructor

	void AssignModule(CFamiTrackerModule &modfile);
	void SetGrooveIndex(int Index);

// Dialog Data
	enum { IDD = IDD_GROOVE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	CFamiTrackerModule *modfile_ = nullptr;

	std::array<std::unique_ptr<ft0cc::doc::groove>, MAX_GROOVE> GrooveTable;
	ft0cc::doc::groove *Groove = nullptr;
	int m_iGrooveIndex, m_iGroovePos;

	CListBox m_cGrooveTable, m_cCurrentGroove;

	void ReloadGrooves();
	void UpdateCurrentGroove();
	void UpdateIndicators();
	void ParseGrooveField();

	DECLARE_MESSAGE_MAP()
public:
	virtual BOOL OnInitDialog();
	virtual BOOL PreTranslateMessage(MSG* pMsg);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnLbnSelchangeListGrooveTable();
	afx_msg void OnLbnSelchangeListCurrentGroove();
	afx_msg void OnBnClickedButtonGroovelUp();
	afx_msg void OnBnClickedButtonGroovelDown();
	afx_msg void OnBnClickedButtonGroovelClear();
	afx_msg void OnBnClickedButtonGroovelClearall();
	afx_msg void OnBnClickedButtonGrooveUp();
	afx_msg void OnBnClickedButtonGrooveDown();
	afx_msg void OnBnClickedButtonGrooveCopyFxx();
	afx_msg void OnBnClickedButtonGrooveExpand();
	afx_msg void OnBnClickedButtonGrooveShrink();
	afx_msg void OnBnClickedButtonGrooveGenerate();
	afx_msg void OnBnClickedButtonGroovePad();
	afx_msg void OnBnClickedApply();
};
