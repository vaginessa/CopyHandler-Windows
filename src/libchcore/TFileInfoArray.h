/***************************************************************************
*   Copyright (C) 2001-2008 by Jozef Starosczyk                           *
*   ixen@copyhandler.com                                                  *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU Library General Public License          *
*   (version 2) as published by the Free Software Foundation;             *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU Library General Public     *
*   License along with this program; if not, write to the                 *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
// File was originally based on FileInfo.h by Antonio Tejada Lacaci.
// Almost everything has changed since then.

#ifndef __TFILEINFOARRAY_H__
#define __TFILEINFOARRAY_H__

#include "libchcore.h"
#include "TPath.h"

BEGIN_CHCORE_NAMESPACE

// CFileInfo flags
// flag stating that file has been processed (used to determine if file can be deleted at the end of copying)
#define FIF_PROCESSED		0x00000001

class TFileInfo;
typedef boost::shared_ptr<TFileInfo> TFileInfoPtr;

class LIBCHCORE_API TFileInfoArray
{
public:
	TFileInfoArray(const TPathContainer& rBasePaths);
	~TFileInfoArray();

	// Adds a new object info to this container
	void AddFileInfo(const TFileInfoPtr& spFileInfo);

	/// Retrieves count of elements in this object
	size_t GetSize() const;

	/// Retrieves an element at the specified index
	TFileInfoPtr GetAt(size_t stIndex) const;

	/// Retrieves a copy of the element at a specified index
	TFileInfo GetCopyAt(size_t stIndex) const;

	/// Removes all elements from this object
	void Clear();

	// specialized operations on contents of m_vFiles
	/// Calculates the size of the first stCount file info objects
	unsigned long long CalculatePartialSize(size_t stCount);

	/// Calculates the size of all file info objects inside this object
	unsigned long long CalculateTotalSize();

	void SetComplete(bool bComplete);
	bool IsComplete() const;

	/// Stores infos about elements in the archive
	void Serialize(TReadBinarySerializer& rSerializer, bool bOnlyFlags);
	void Serialize(TWriteBinarySerializer& rSerializer, bool bOnlyFlags) const;

protected:
	const TPathContainer& m_rBasePaths;
	bool m_bComplete;

#pragma warning(push)
#pragma warning(disable: 4251)
	std::vector<TFileInfoPtr> m_vFiles;
	mutable boost::shared_mutex m_lock;
#pragma warning(pop)
};

END_CHCORE_NAMESPACE

#endif