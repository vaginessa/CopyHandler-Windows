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

#ifndef __FILEINFO_H__
#define __FILEINFO_H__

#include "libchcore.h"
#include "TPath.h"

BEGIN_CHCORE_NAMESPACE

// CFileInfo flags
// flag stating that file has been processed (used to determine if file can be deleted at the end of copying)
#define FIF_PROCESSED		0x00000001

class CFiltersArray;

class LIBCHCORE_API CFileInfo
{  
public:
	CFileInfo();
	CFileInfo(const CFileInfo& finf);
	~CFileInfo();

	// with base path
	void Init(const chcore::TSmartPath& rpathFile, size_t stSrcIndex, const chcore::TPathContainer* pBasePaths,
		DWORD dwAttributes, ULONGLONG uhFileSize, FILETIME ftCreation, FILETIME ftLastAccess, FILETIME ftLastWrite,
		uint_t uiFlags);

	// without base path
	void Init(const chcore::TSmartPath& rpathFile, DWORD dwAttributes, ULONGLONG uhFileSize, FILETIME ftCreation,
		FILETIME ftLastAccess, FILETIME ftLastWrite, uint_t uiFlags);

	// setting parent object
	void SetParentObject(size_t stIndex, const chcore::TPathContainer* pBasePaths);

	ULONGLONG GetLength64() const { return m_uhFileSize; }
	void SetLength64(ULONGLONG uhSize) { m_uhFileSize=uhSize; }

	const chcore::TSmartPath& GetFilePath() const { return m_pathFile; }	// returns path with m_pathFile (probably not full)
	chcore::TSmartPath GetFullFilePath() const;		// returns full path
	void SetFilePath(const chcore::TSmartPath& tPath) { m_pathFile = tPath; };

	/* Get File times info (equivalent to CFindFile members) */
	const FILETIME& GetCreationTime() const { return m_ftCreation; };
	const FILETIME& GetLastAccessTime() const { return m_ftLastAccess; };
	const FILETIME& GetLastWriteTime() const { return m_ftLastWrite; };

	/* Get File attributes info (equivalent to CFindFile members) */
	DWORD GetAttributes() const { return m_dwAttributes; }
	bool IsDirectory() const { return (m_dwAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0; }
	bool IsArchived() const { return (m_dwAttributes & FILE_ATTRIBUTE_ARCHIVE) != 0; }
	bool IsReadOnly() const { return (m_dwAttributes & FILE_ATTRIBUTE_READONLY) != 0; }
	bool IsCompressed() const { return (m_dwAttributes & FILE_ATTRIBUTE_COMPRESSED) != 0; }
	bool IsSystem() const { return (m_dwAttributes & FILE_ATTRIBUTE_SYSTEM) != 0; }
	bool IsHidden() const { return (m_dwAttributes & FILE_ATTRIBUTE_HIDDEN) != 0; }
	bool IsTemporary() const { return (m_dwAttributes & FILE_ATTRIBUTE_TEMPORARY) != 0; }
	bool IsNormal() const { return m_dwAttributes == 0; }

	uint_t GetFlags() const { return m_uiFlags; }
	void SetFlags(uint_t uiFlags, uint_t uiMask = 0xffffffff) { m_uiFlags = (m_uiFlags & ~(uiFlags & uiMask)) | (uiFlags & uiMask); }

	// operations
	void SetClipboard(const chcore::TPathContainer* pBasePaths) { m_pBasePaths = pBasePaths; }

	void SetSrcIndex(size_t stIndex) { m_stSrcIndex = stIndex; };
	size_t GetSrcIndex() const { return m_stSrcIndex; };

	// operators
	bool operator==(const CFileInfo& rInfo);

	void Serialize(chcore::TReadBinarySerializer& rSerializer);
	void Serialize(chcore::TWriteBinarySerializer& rSerializer) const;

private:
	chcore::TSmartPath m_pathFile;	// contains relative path (first path is in CClipboardArray)

	size_t m_stSrcIndex;		// index in CClipboardArray table (which contains the first part of the path)
	const chcore::TPathContainer* m_pBasePaths;

	DWORD m_dwAttributes;	// attributes
	ULONGLONG m_uhFileSize;
	FILETIME  m_ftCreation;
	FILETIME  m_ftLastAccess;
	FILETIME  m_ftLastWrite;

	uint_t m_uiFlags;
};

typedef boost::shared_ptr<CFileInfo> CFileInfoPtr;

class LIBCHCORE_API CFileInfoArray
{
public:
	CFileInfoArray(const chcore::TPathContainer& rBasePaths);
	~CFileInfoArray();

	// Adds a new object info to this container
	void AddFileInfo(const CFileInfoPtr& spFileInfo);

	/// Retrieves count of elements in this object
	size_t GetSize() const;

	/// Retrieves an element at the specified index
	CFileInfoPtr GetAt(size_t stIndex) const;

	/// Retrieves a copy of the element at a specified index
	CFileInfo GetCopyAt(size_t stIndex) const;

	/// Removes all elements from this object
	void Clear();

	// specialized operations on contents of m_vFiles
	/// Calculates the size of the first stCount file info objects
	unsigned long long CalculatePartialSize(size_t stCount);

	/// Calculates the size of all file info objects inside this object
	unsigned long long CalculateTotalSize();

	/// Stores infos about elements in the archive
	void Serialize(chcore::TReadBinarySerializer& rSerializer, bool bOnlyFlags);
	void Serialize(chcore::TWriteBinarySerializer& rSerializer, bool bOnlyFlags) const;

protected:
   const chcore::TPathContainer& m_rBasePaths;

#pragma warning(push)
#pragma warning(disable: 4251)
	std::vector<CFileInfoPtr> m_vFiles;
	mutable boost::shared_mutex m_lock;
#pragma warning(pop)
};

END_CHCORE_NAMESPACE

#endif