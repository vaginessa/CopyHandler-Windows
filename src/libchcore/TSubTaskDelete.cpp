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
/// @file  TSubTaskDelete.cpp
/// @date  2010/09/19
/// @brief Contains implementation of classes responsible for delete sub-operation.
// ============================================================================
#include "stdafx.h"
#include "TSubTaskDelete.h"
#include "TSubTaskContext.h"
#include "TWorkerThreadController.h"
#include "TTaskConfiguration.h"
#include "TLocalFilesystem.h"
#include "..\libicpf\log.h"
#include "IFeedbackHandler.h"
#include <boost\lexical_cast.hpp>
#include "TFileInfoArray.h"
#include "TFileInfo.h"
#include "TTaskLocalStats.h"
#include "TCoreException.h"
#include "ErrorCodes.h"
#include "TScopedRunningTimeTracker.h"
#include "TFeedbackHandlerWrapper.h"
#include <boost/make_shared.hpp>
#include "TBufferSizes.h"

BEGIN_CHCORE_NAMESPACE

///////////////////////////////////////////////////////////////////////////////////////////////////
// class TSubTaskDelete

TSubTaskDelete::TSubTaskDelete(TSubTaskContext& rContext) : 
	TSubTaskBase(rContext),
	m_tSubTaskStats(eSubOperation_Deleting)
{
}

void TSubTaskDelete::Reset()
{
	m_tSubTaskStats.Clear();
}

TSubTaskBase::ESubOperationResult TSubTaskDelete::Exec(const IFeedbackHandlerPtr& spFeedback)
{
	TScopedRunningTimeTracker guard(m_tSubTaskStats);
	TFeedbackHandlerWrapperPtr spFeedbackHandler(boost::make_shared<TFeedbackHandlerWrapper>(spFeedback, guard));

	// log
	icpf::log_file& rLog = GetContext().GetLog();
	TFileInfoArray& rFilesCache = GetContext().GetFilesCache();
	TWorkerThreadController& rThreadController = GetContext().GetThreadController();
	const TConfig& rConfig = GetContext().GetConfig();

	// log
	rLog.logi(_T("Deleting files (DeleteFiles)..."));

	// new stats
	m_tSubTaskStats.SetCurrentBufferIndex(TBufferSizes::eBuffer_Default);
	m_tSubTaskStats.SetTotalCount(rFilesCache.GetSize());
	m_tSubTaskStats.SetProcessedCount(0);
	m_tSubTaskStats.SetTotalSize(0);
	m_tSubTaskStats.SetProcessedSize(0);
	m_tSubTaskStats.SetCurrentPath(TString());

	// current processed path
	BOOL bSuccess;
	TFileInfoPtr spFileInfo;
	TString strFormat;

	// index points to 0 or next item to process
	file_count_t fcIndex = m_tSubTaskStats.GetCurrentIndex();
	while(fcIndex < rFilesCache.GetSize())
	{
		spFileInfo = rFilesCache.GetAt(rFilesCache.GetSize() - fcIndex - 1);

		m_tSubTaskStats.SetCurrentIndex(fcIndex);

		// new stats
		m_tSubTaskStats.SetProcessedCount(fcIndex);
		m_tSubTaskStats.SetCurrentPath(spFileInfo->GetFullFilePath().ToString());

		// check for kill flag
		if(rThreadController.KillRequested())
		{
			// log
			rLog.logi(_T("Kill request while deleting files (Delete Files)"));
			return TSubTaskBase::eSubResult_KillRequest;
		}

		// current processed element
		if(!spFileInfo->IsProcessed())
		{
			++fcIndex;
			continue;
		}

		// delete data
		if(spFileInfo->IsDirectory())
		{
			if(!GetTaskPropValue<eTO_ProtectReadOnlyFiles>(rConfig))
				TLocalFilesystem::SetAttributes(spFileInfo->GetFullFilePath(), FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY);
			bSuccess = TLocalFilesystem::RemoveDirectory(spFileInfo->GetFullFilePath());
		}
		else
		{
			// set files attributes to normal - it'd slow processing a bit, but it's better.
			if(!GetTaskPropValue<eTO_ProtectReadOnlyFiles>(rConfig))
				TLocalFilesystem::SetAttributes(spFileInfo->GetFullFilePath(), FILE_ATTRIBUTE_NORMAL);
			bSuccess = TLocalFilesystem::DeleteFile(spFileInfo->GetFullFilePath());
		}

		// operation failed
		DWORD dwLastError = GetLastError();
		if(!bSuccess && dwLastError != ERROR_PATH_NOT_FOUND && dwLastError != ERROR_FILE_NOT_FOUND)
		{
			// log
			strFormat = _T("Error #%errno while deleting file/folder %path");
			strFormat.Replace(_T("%errno"), boost::lexical_cast<std::wstring>(dwLastError).c_str());
			strFormat.Replace(_T("%path"), spFileInfo->GetFullFilePath().ToString());
			rLog.loge(strFormat.c_str());

			EFeedbackResult frResult = spFeedbackHandler->FileError(spFileInfo->GetFullFilePath().ToWString(), TString(), EFileError::eDeleteError, dwLastError);
			switch(frResult)
			{
			case EFeedbackResult::eResult_Cancel:
				rLog.logi(_T("Cancel request while deleting file."));
				return TSubTaskBase::eSubResult_CancelRequest;

			case EFeedbackResult::eResult_Retry:
				continue;	// no fcIndex bump, since we are trying again

			case EFeedbackResult::eResult_Pause:
				return TSubTaskBase::eSubResult_PauseRequest;

			case EFeedbackResult::eResult_Skip:
				break;		// just do nothing

			default:
				BOOST_ASSERT(FALSE);		// unknown result
				THROW_CORE_EXCEPTION(eErr_UnhandledCase);
			}
		}

		++fcIndex;
	}//while

	m_tSubTaskStats.SetCurrentIndex(fcIndex);
	m_tSubTaskStats.SetProcessedCount(fcIndex);
	m_tSubTaskStats.SetCurrentPath(TString());

	// log
	rLog.logi(_T("Deleting files finished"));

	return TSubTaskBase::eSubResult_Continue;
}

void TSubTaskDelete::GetStatsSnapshot(TSubTaskStatsSnapshotPtr& spStats) const
{
	m_tSubTaskStats.GetSnapshot(spStats);
	// if this subtask is not started yet, try to get the most fresh information for processing
	if(!spStats->IsRunning() && spStats->GetTotalCount() == 0 && spStats->GetTotalSize() == 0)
	{
		spStats->SetTotalCount(GetContext().GetFilesCache().GetSize());
		spStats->SetTotalSize(0);
	}
}

void TSubTaskDelete::Store(const ISerializerPtr& spSerializer) const
{
	ISerializerContainerPtr spContainer = spSerializer->GetContainer(_T("subtask_delete"));
	InitColumns(spContainer);

	ISerializerRowData& rRow = spContainer->GetRow(0, m_tSubTaskStats.WasAdded());

	m_tSubTaskStats.Store(rRow);
}

void TSubTaskDelete::Load(const ISerializerPtr& spSerializer)
{
	ISerializerContainerPtr spContainer = spSerializer->GetContainer(_T("subtask_delete"));

	InitColumns(spContainer);

	ISerializerRowReaderPtr spRowReader = spContainer->GetRowReader();
	if(spRowReader->Next())
		m_tSubTaskStats.Load(spRowReader);
}

void TSubTaskDelete::InitColumns(const ISerializerContainerPtr& spContainer) const
{
	IColumnsDefinition& rColumns = spContainer->GetColumnsDefinition();
	if(rColumns.IsEmpty())
		TSubTaskStatsInfo::InitColumns(rColumns);
}

END_CHCORE_NAMESPACE
