/************************************************************************
	Copy Handler 1.x - program for copying data in Microsoft Windows
						 systems.
	Copyright (C) 2001-2004 Ixen Gerthannes (copyhandler@o2.pl)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*************************************************************************/
#include "stdafx.h"
#include "shortcuts.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

bool CShortcut::FromString(const CString& strText)
{
	int iPos=strText.ReverseFind(_T('|'));
	if (iPos != -1 && iPos < strText.GetLength()-1)
	{
		m_strName=strText.Left(iPos);
		m_strPath=strText.Mid(iPos+1);

		return true;
	}
	else
		return false;
}

CShortcut::CShortcut(const CString& strText)
{
	FromString(strText);
}

CShortcut::operator CString()
{
	if (m_strPath.IsEmpty())
		return _T("");
	else
		return m_strName+_T("|")+m_strPath;
}
