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
/// @file  TSubTaskScanDirectory.cpp
/// @date  2010/09/18
/// @brief Contains implementation of classes related to scan directory subtask.
// ============================================================================
#include "stdafx.h"
#include "TSubTaskScanDirectory.h"
#include "TSubTaskContext.h"
#include "TTaskConfiguration.h"
#include "TLocalFilesystem.h"
#include "IFeedbackHandler.h"
#include "TBasePathData.h"
#include "TWorkerThreadController.h"
#include "TTaskLocalStats.h"
#include <boost\smart_ptr\make_shared.hpp>
#include "..\libicpf\log.h"
#include "TFileInfoArray.h"
#include "TFileInfo.h"
#include "SerializationHelpers.h"
#include "TBinarySerializer.h"
#include "DataBuffer.h"
#include "TCoreException.h"
#include "ErrorCodes.h"
#include "TPathContainer.h"

BEGIN_CHCORE_NAMESPACE

namespace details
{
	///////////////////////////////////////////////////////////////////////////////////////////////////
	// class TScanDirectoriesProgressInfo

	TScanDirectoriesProgressInfo::TScanDirectoriesProgressInfo() :
		m_stCurrentIndex(0)
	{
	}

	TScanDirectoriesProgressInfo::~TScanDirectoriesProgressInfo()
	{
	}

	void TScanDirectoriesProgressInfo::Serialize(TReadBinarySerializer& rSerializer)
	{
		boost::unique_lock<boost::shared_mutex> lock(m_lock);
		Serializers::Serialize(rSerializer, m_stCurrentIndex);
	}

	void TScanDirectoriesProgressInfo::Serialize(TWriteBinarySerializer& rSerializer) const
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		Serializers::Serialize(rSerializer, m_stCurrentIndex);
	}

	void TScanDirectoriesProgressInfo::ResetProgress()
	{
		boost::unique_lock<boost::shared_mutex> lock(m_lock);
		m_stCurrentIndex = 0;
	}

	void TScanDirectoriesProgressInfo::SetCurrentIndex(size_t stIndex)
	{
		boost::unique_lock<boost::shared_mutex> lock(m_lock);
		m_stCurrentIndex = stIndex;
	}

	void TScanDirectoriesProgressInfo::IncreaseCurrentIndex()
	{
		boost::unique_lock<boost::shared_mutex> lock(m_lock);
		++m_stCurrentIndex;
	}

	size_t TScanDirectoriesProgressInfo::GetCurrentIndex() const
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		return m_stCurrentIndex;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// class TSubTaskScanDirectories
TSubTaskScanDirectories::TSubTaskScanDirectories(TSubTaskContext& rContext) :
	TSubTaskBase(rContext)
{
	m_tSubTaskStats.SetSubOperationType(eSubOperation_Scanning);
}

TSubTaskScanDirectories::~TSubTaskScanDirectories()
{
}

void TSubTaskScanDirectories::Reset()
{
	m_tProgressInfo.ResetProgress();
	m_tSubTaskStats.Clear();
}

TSubTaskScanDirectories::ESubOperationResult TSubTaskScanDirectories::Exec()
{
	TSubTaskProcessingGuard guard(m_tSubTaskStats);

	// log
	icpf::log_file& rLog = GetContext().GetLog();
	TFileInfoArray& rFilesCache = GetContext().GetFilesCache();
	IFeedbackHandlerPtr spFeedbackHandler = GetContext().GetFeedbackHandler();
	TWorkerThreadController& rThreadController = GetContext().GetThreadController();
	TBasePathDataContainerPtr spBasePaths = GetContext().GetBasePaths();
	const TConfig& rConfig = GetContext().GetConfig();
	const TFileFiltersArray& rafFilters = GetContext().GetFilters();

	rLog.logi(_T("Searching for files..."));

	// reset progress
	rFilesCache.SetComplete(false);

	// new stats
	m_tSubTaskStats.SetCurrentBufferIndex(TBufferSizes::eBuffer_Default);
	m_tSubTaskStats.SetTotalCount(spBasePaths->GetCount());
	m_tSubTaskStats.SetProcessedCount(0);
	m_tSubTaskStats.SetTotalSize(0);
	m_tSubTaskStats.SetProcessedSize(0);
	m_tSubTaskStats.SetCurrentPath(TString());

	// delete the content of rFilesCache
	rFilesCache.Clear();

	bool bIgnoreDirs = GetTaskPropValue<eTO_IgnoreDirectories>(rConfig);
	bool bForceDirectories = GetTaskPropValue<eTO_CreateDirectoriesRelativeToRoot>(rConfig);

	// add everything
	TString strFormat;
	bool bRetry = true;
	bool bSkipInputPath = false;

	size_t stSize = spBasePaths->GetCount();
	// NOTE: in theory, we should resume the scanning, but in practice we are always restarting scanning if interrupted.
	size_t stIndex = 0;		// m_tProgressInfo.GetCurrentIndex()
	for(; stIndex < stSize; stIndex++)
	{
		TBasePathDataPtr spBasePath = spBasePaths->GetAt(stIndex);
		TSmartPath pathCurrent = spBasePath->GetSrcPath();

		m_tProgressInfo.SetCurrentIndex(stIndex);

		// new stats
		m_tSubTaskStats.SetProcessedCount(stIndex);
		m_tSubTaskStats.SetCurrentPath(pathCurrent.ToString());

		bSkipInputPath = false;
		TFileInfoPtr spFileInfo(boost::make_shared<TFileInfo>());

		// check if we want to process this path at all (might be already fast moved)
		if(spBasePath->GetSkipFurtherProcessing())
			continue;

		// try to get some info about the input path; let user know if the path does not exist.
		do
		{
			bRetry = false;

			// read attributes of src file/folder
			bool bExists = TLocalFilesystem::GetFileInfo(pathCurrent, spFileInfo, spBasePath);
			if(!bExists)
			{
				FEEDBACK_FILEERROR ferr = { pathCurrent.ToString(), NULL, eFastMoveError, ERROR_FILE_NOT_FOUND };
				IFeedbackHandler::EFeedbackResult frResult = (IFeedbackHandler::EFeedbackResult)spFeedbackHandler->RequestFeedback(IFeedbackHandler::eFT_FileError, &ferr);
				switch(frResult)
				{
				case IFeedbackHandler::eResult_Cancel:
					rFilesCache.Clear();
					return eSubResult_CancelRequest;

				case IFeedbackHandler::eResult_Retry:
					bRetry = true;
					break;

				case IFeedbackHandler::eResult_Pause:
					rFilesCache.Clear();
					return eSubResult_PauseRequest;

				case IFeedbackHandler::eResult_Skip:
					bSkipInputPath = true;
					break;		// just do nothing

				default:
					BOOST_ASSERT(FALSE);		// unknown result
					THROW_CORE_EXCEPTION(eErr_UnhandledCase);
				}
			}
		}
		while(bRetry);

		// if we have chosen to skip the input path then there's nothing to do
		if(bSkipInputPath)
			continue;

		// log
		strFormat = _T("Adding file/folder (clipboard) : %path ...");
		strFormat.Replace(_T("%path"), pathCurrent.ToString());
		rLog.logi(strFormat);

		// add if needed
		if(spFileInfo->IsDirectory())
		{
			// add if folder's aren't ignored
			if(!bIgnoreDirs && !bForceDirectories)
			{
				// add directory info; it is not to be filtered with afFilters
				rFilesCache.AddFileInfo(spFileInfo);

				// log
				strFormat = _T("Added folder %path");
				strFormat.Replace(_T("%path"), spFileInfo->GetFullFilePath().ToString());
				rLog.logi(strFormat);
			}

			// don't add folder contents when moving inside one disk boundary
			// log
			strFormat = _T("Recursing folder %path");
			strFormat.Replace(_t("%path"), spFileInfo->GetFullFilePath().ToString());
			rLog.logi(strFormat);

			ScanDirectory(spFileInfo->GetFullFilePath(), spBasePath, true, !bIgnoreDirs || bForceDirectories, rafFilters);

			// check for kill need
			if(rThreadController.KillRequested())
			{
				// log
				rLog.logi(_T("Kill request while adding data to files array (RecurseDirectories)"));
				rFilesCache.Clear();
				return eSubResult_KillRequest;
			}
		}
		else
		{
			// add file info if passes filters
			if(rafFilters.Match(spFileInfo))
				rFilesCache.AddFileInfo(spFileInfo);

			// log
			strFormat = _T("Added file %path");
			strFormat.Replace(_T("%path"), spFileInfo->GetFullFilePath().ToString());
			rLog.logi(strFormat);
		}
	}

	// calc size of all files
	m_tProgressInfo.SetCurrentIndex(stIndex);

	// new stats
	m_tSubTaskStats.SetProcessedCount(stIndex);
	m_tSubTaskStats.SetCurrentPath(TString());

	rFilesCache.SetComplete(true);

	// log
	rLog.logi(_T("Searching for files finished"));

	return eSubResult_Continue;
}

void TSubTaskScanDirectories::GetStatsSnapshot(TSubTaskStatsSnapshotPtr& spStats) const
{
	m_tSubTaskStats.GetSnapshot(spStats);
}

int TSubTaskScanDirectories::ScanDirectory(TSmartPath pathDirName, const TBasePathDataPtr& spBasePathData,
										   bool bRecurse, bool bIncludeDirs, const TFileFiltersArray& afFilters)
{
	TFileInfoArray& rFilesCache = GetContext().GetFilesCache();
	TWorkerThreadController& rThreadController = GetContext().GetThreadController();
	TBasePathDataContainerPtr spBasePaths = GetContext().GetBasePaths();

	TLocalFilesystemFind finder = TLocalFilesystem::CreateFinderObject(pathDirName, PathFromString(_T("*")));
	TFileInfoPtr spFileInfo(boost::make_shared<TFileInfo>());

	while(finder.FindNext(spFileInfo))
	{
		if(rThreadController.KillRequested())
			break;

		if(!spFileInfo->IsDirectory())
		{
			if(afFilters.Match(spFileInfo))
			{
				spFileInfo->SetParentObject(spBasePathData);
				rFilesCache.AddFileInfo(spFileInfo);
				spFileInfo = boost::make_shared<TFileInfo>();
			}
		}
		else
		{
			TSmartPath pathCurrent = spFileInfo->GetFullFilePath();
			if(bIncludeDirs)
			{
				spFileInfo->SetParentObject(spBasePathData);
				rFilesCache.AddFileInfo(spFileInfo);
				spFileInfo = boost::make_shared<TFileInfo>();
			}

			if(bRecurse)
				ScanDirectory(pathCurrent, spBasePathData, bRecurse, bIncludeDirs, afFilters);
		}
	}

	return 0;
}

END_CHCORE_NAMESPACE
