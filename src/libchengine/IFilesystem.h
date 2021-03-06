// ============================================================================
//  Copyright (C) 2001-2015 by Jozef Starosczyk
//  ixen@copyhandler.com
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU Library General Public License
//  (version 2) as published by the Free Software Foundation;
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU Library General Public
//  License along with this program; if not, write to the
//  Free Software Foundation, Inc.,
//  59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// ============================================================================
#ifndef __IFILESYSTEM_H__
#define __IFILESYSTEM_H__

#include "TFileInfoFwd.h"
#include "TBasePathDataFwd.h"
#include "IFilesystemFind.h"
#include "IFilesystemFile.h"

namespace chcore {
	class TSmartPath;
}

namespace chengine
{
	class LIBCHENGINE_API IFilesystem
	{
	public:
		enum EPathsRelation
		{
			eRelation_Network,				// at least one of the paths is network one
			eRelation_CDRom,				// at least one of the paths relates to cd/dvd drive
			eRelation_TwoPhysicalDisks,		// paths lies on two separate physical disks
			eRelation_SinglePhysicalDisk,	// paths lies on the same physical disk
			eRelation_Other					// other type of relation
		};

	public:
		virtual ~IFilesystem();

		virtual bool PathExist(const chcore::TSmartPath& strPath) = 0;

		virtual void SetFileDirBasicInfo(const chcore::TSmartPath& pathFileDir, DWORD dwAttributes, const chcore::TFileTime& ftCreationTime, const chcore::TFileTime& ftLastAccessTime, const chcore::TFileTime& ftLastWriteTime) = 0;
		virtual void SetAttributes(const chcore::TSmartPath& pathFileDir, DWORD dwAttributes) = 0;

		virtual void CreateDirectory(const chcore::TSmartPath& pathDirectory, bool bCreateFullPath) = 0;
		virtual void RemoveDirectory(const chcore::TSmartPath& pathFile) = 0;
		virtual void DeleteFile(const chcore::TSmartPath& pathFile) = 0;

		virtual void GetFileInfo(const chcore::TSmartPath& pathFile, TFileInfoPtr& rFileInfo, const TBasePathDataPtr& spBasePathData = TBasePathDataPtr()) = 0;
		virtual void FastMove(const chcore::TSmartPath& pathSource, const chcore::TSmartPath& pathDestination) = 0;

		virtual IFilesystemFindPtr CreateFinderObject(const chcore::TSmartPath& pathDir, const chcore::TSmartPath& pathMask) = 0;
		virtual IFilesystemFilePtr CreateFileObject(IFilesystemFile::EOpenMode eMode, const chcore::TSmartPath& pathFile, bool bNoBuffering, bool bProtectReadOnlyFiles) = 0;

		virtual EPathsRelation GetPathsRelation(const chcore::TSmartPath& pathFirst, const chcore::TSmartPath& pathSecond) = 0;

		virtual void GetDynamicFreeSpace(const chcore::TSmartPath& path, unsigned long long& rullFree, unsigned long long& rullTotal) = 0;
	};

	typedef std::shared_ptr<IFilesystem> IFilesystemPtr;
}

#endif
