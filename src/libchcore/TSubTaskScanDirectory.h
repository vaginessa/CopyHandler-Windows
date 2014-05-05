// ============================================================================
//  Copyright (C) 2001-2009 by Jozef Starosczyk
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
/// @file  TSubTaskScanDirectory.h
/// @date  2010/09/18
/// @brief Contains declarations of classes responsible for directory scan sub-operation.
// ============================================================================
#ifndef __TSUBTASKSCANDIRECTORY_H__
#define __TSUBTASKSCANDIRECTORY_H__

#include "libchcore.h"
#include "TSubTaskBase.h"
#include "TPath.h"
#include "TBasePathData.h"

BEGIN_CHCORE_NAMESPACE

class TFileFiltersArray;
class TReadBinarySerializer;
class TWriteBinarySerializer;

namespace details
{
	///////////////////////////////////////////////////////////////////////////
	// TScanDirectoriesProgressInfo

	class TScanDirectoriesProgressInfo : public TSubTaskProgressInfo
	{
	public:
		TScanDirectoriesProgressInfo();
		virtual ~TScanDirectoriesProgressInfo();

		virtual void Serialize(TReadBinarySerializer& rSerializer);
		virtual void Serialize(TWriteBinarySerializer& rSerializer) const;

		virtual void ResetProgress();

		void SetCurrentIndex(size_t stIndex);
		void IncreaseCurrentIndex();
		size_t GetCurrentIndex() const;

	private:
		size_t m_stCurrentIndex;
		mutable boost::shared_mutex m_lock;
	};
}

///////////////////////////////////////////////////////////////////////////
// TSubTaskScanDirectories

class LIBCHCORE_API TSubTaskScanDirectories : public TSubTaskBase
{
public:
	TSubTaskScanDirectories(TSubTaskContext& rContext);
	virtual ~TSubTaskScanDirectories();

	virtual void Reset();

	virtual ESubOperationResult Exec();
	virtual ESubOperationType GetSubOperationType() const { return eSubOperation_Scanning; }

	virtual TSubTaskProgressInfo& GetProgressInfo() { return m_tProgressInfo; }
	virtual void GetStatsSnapshot(TSubTaskStatsSnapshotPtr& spStats) const;

private:
	int ScanDirectory(TSmartPath pathDirName, const TBasePathDataPtr& spBasePathData,
					  bool bRecurse, bool bIncludeDirs, const TFileFiltersArray& afFilters);

private:
#pragma warning(push)
#pragma warning(disable: 4251)
	details::TScanDirectoriesProgressInfo m_tProgressInfo;
	TSubTaskStatsInfo m_tSubTaskStats;
#pragma warning(pop)
};

END_CHCORE_NAMESPACE

#endif
