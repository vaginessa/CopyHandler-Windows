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
/// @file  TSubTaskFastMove.h
/// @date  2011/11/13
/// @brief Contains declarations of classes responsible for fast move subtask.
// ============================================================================
#ifndef __TSUBTASKFASTMOVE_H__
#define __TSUBTASKFASTMOVE_H__

#include "libchcore.h"
#include "TSubTaskBase.h"
#include "TPath.h"

BEGIN_CHCORE_NAMESPACE

class TFileFiltersArray;
class TReadBinarySerializer;
class TWriteBinarySerializer;

namespace details
{
	///////////////////////////////////////////////////////////////////////////
	// TFastMoveProgressInfo

	class TFastMoveProgressInfo : public TSubTaskProgressInfo
	{
	public:
		TFastMoveProgressInfo();
		virtual ~TFastMoveProgressInfo();

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
// TSubTaskFastMove

class LIBCHCORE_API TSubTaskFastMove : public TSubTaskBase
{
public:
	TSubTaskFastMove(TSubTaskContext& rContext);
	virtual ~TSubTaskFastMove();

	virtual ESubOperationResult Exec();
	virtual ESubOperationType GetSubOperationType() const { return eSubOperation_Scanning; }

	virtual TSubTaskProgressInfo& GetProgressInfo() { return m_tProgressInfo; }

private:
	int ScanDirectory(TSmartPath pathDirName, size_t stSrcIndex, bool bRecurse, bool bIncludeDirs, TFileFiltersArray& afFilters);

private:
#pragma warning(push)
#pragma warning(disable: 4251)
	details::TFastMoveProgressInfo m_tProgressInfo;
#pragma warning(pop)
};

END_CHCORE_NAMESPACE

#endif