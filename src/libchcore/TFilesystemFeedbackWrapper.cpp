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
#include "stdafx.h"
#include "TFilesystemFeedbackWrapper.h"
#include <boost/lexical_cast.hpp>
#include "TFileException.h"
#include "TFileInfo.h"
#include "TWorkerThreadController.h"

namespace chcore
{
	TFilesystemFeedbackWrapper::TFilesystemFeedbackWrapper(const IFeedbackHandlerPtr& spFeedbackHandler, const IFilesystemPtr& spFilesystem, icpf::log_file& rLog, TWorkerThreadController& rThreadController) :
		m_spFeedbackHandler(spFeedbackHandler),
		m_spFilesystem(spFilesystem),
		m_rLog(rLog),
		m_rThreadController(rThreadController)
	{
		if (!spFilesystem)
			THROW_CORE_EXCEPTION_MSG(eErr_InvalidArgument, L"Filesystem not provided");
	}

	TSubTaskBase::ESubOperationResult TFilesystemFeedbackWrapper::CreateDirectoryFB(const TSmartPath& pathDirectory)
	{
		bool bRetry = false;
		do
		{
			bRetry = false;

			DWORD dwLastError = ERROR_SUCCESS;
			try
			{
				m_spFilesystem->CreateDirectory(pathDirectory, false);
				return TSubTaskBase::eSubResult_Continue;
			}
			catch (const TFileException& e)
			{
				dwLastError = e.GetNativeError();
			}

			if (dwLastError == ERROR_ALREADY_EXISTS)
				return TSubTaskBase::eSubResult_Continue;

			// log
			TString strFormat;
			strFormat = _T("Error %errno while calling CreateDirectory %path (ProcessFiles)");
			strFormat.Replace(_T("%errno"), boost::lexical_cast<std::wstring>(dwLastError).c_str());
			strFormat.Replace(_T("%path"), pathDirectory.ToString());
			m_rLog.loge(strFormat.c_str());

			TFeedbackResult frResult = m_spFeedbackHandler->FileError(pathDirectory.ToWString(), TString(), EFileError::eCreateError, dwLastError);
			switch (frResult.GetResult())
			{
			case EFeedbackResult::eResult_Cancel:
				return TSubTaskBase::eSubResult_CancelRequest;

			case EFeedbackResult::eResult_Retry:
				bRetry = true;
				break;

			case EFeedbackResult::eResult_Pause:
				return TSubTaskBase::eSubResult_PauseRequest;

			case EFeedbackResult::eResult_Skip:
				return TSubTaskBase::eSubResult_Continue;

			default:
				BOOST_ASSERT(FALSE);		// unknown result
				THROW_CORE_EXCEPTION(eErr_UnhandledCase);
			}

			if(WasKillRequested(frResult))
				return TSubTaskBase::eSubResult_KillRequest;
		}
		while (bRetry);

		return TSubTaskBase::eSubResult_Continue;
	}

	bool TFilesystemFeedbackWrapper::WasKillRequested(const TFeedbackResult& rFeedbackResult) const
	{
		if(m_rThreadController.KillRequested(rFeedbackResult.IsAutomatedReply() ? m_spFeedbackHandler->GetRetryInterval() : 0))
			return true;
		return false;
	}

	TSubTaskBase::ESubOperationResult TFilesystemFeedbackWrapper::CheckForFreeSpaceFB(const TSmartPath& pathFirstSrc, const TSmartPath& pathDestination, unsigned long long ullNeededSize)
	{
		unsigned long long ullAvailableSize = 0;
		TFeedbackResult frResult(eResult_Unknown, false);
		bool bRetry = false;

		do
		{
			bRetry = false;

			m_rLog.logi(_T("Checking for free space on destination disk..."));

			// get free space
			DWORD dwLastError = ERROR_SUCCESS;
			bool bCheckFailed = false;
			try
			{
				m_spFilesystem->GetDynamicFreeSpace(pathDestination, ullAvailableSize);
			}
			catch (const TFileException& e)
			{
				dwLastError = e.GetNativeError();
				bCheckFailed = true;
			}

			if (bCheckFailed)
			{
				TString strFormat;
				strFormat = _T("Error %errno while checking free space at %path");
				strFormat.Replace(_T("%errno"), boost::lexical_cast<std::wstring>(dwLastError).c_str());
				strFormat.Replace(_T("%path"), pathDestination.ToString());
				m_rLog.loge(strFormat.c_str());

				frResult = m_spFeedbackHandler->FileError(pathDestination.ToWString(), TString(), EFileError::eCheckForFreeSpace, dwLastError);
				switch (frResult.GetResult())
				{
				case EFeedbackResult::eResult_Cancel:
					return TSubTaskBase::eSubResult_CancelRequest;

				case EFeedbackResult::eResult_Retry:
					bRetry = true;
					break;

				case EFeedbackResult::eResult_Pause:
					return TSubTaskBase::eSubResult_PauseRequest;

				case EFeedbackResult::eResult_Skip:
					return TSubTaskBase::eSubResult_Continue;

				default:
					BOOST_ASSERT(FALSE);		// unknown result
					THROW_CORE_EXCEPTION(eErr_UnhandledCase);
				}
			}

			if (!bRetry && ullNeededSize > ullAvailableSize)
			{
				TString strFormat = _T("Not enough free space on disk - needed %needsize bytes for data, available: %availablesize bytes.");
				strFormat.Replace(_t("%needsize"), boost::lexical_cast<std::wstring>(ullNeededSize).c_str());
				strFormat.Replace(_t("%availablesize"), boost::lexical_cast<std::wstring>(ullAvailableSize).c_str());
				m_rLog.logw(strFormat.c_str());

				frResult = m_spFeedbackHandler->NotEnoughSpace(pathFirstSrc.ToWString(), pathDestination.ToWString(), ullNeededSize);
				switch (frResult.GetResult())
				{
				case EFeedbackResult::eResult_Cancel:
					m_rLog.logi(_T("Cancel request while checking for free space on disk."));
					return TSubTaskBase::eSubResult_CancelRequest;

				case EFeedbackResult::eResult_Retry:
					m_rLog.logi(_T("Retrying to read drive's free space..."));
					bRetry = true;
					break;

				case EFeedbackResult::eResult_Ignore:
					m_rLog.logi(_T("Ignored warning about not enough place on disk to copy data."));
					return TSubTaskBase::eSubResult_Continue;

				default:
					BOOST_ASSERT(FALSE);		// unknown result
					THROW_CORE_EXCEPTION(eErr_UnhandledCase);
				}
			}

			if(bRetry && WasKillRequested(frResult))
				return TSubTaskBase::eSubResult_KillRequest;
		}
		while (bRetry);

		return TSubTaskBase::eSubResult_Continue;
	}

	TSubTaskBase::ESubOperationResult TFilesystemFeedbackWrapper::RemoveDirectoryFB(const TFileInfoPtr& spFileInfo, bool bProtectReadOnlyFiles)
	{
		bool bRetry = false;
		do
		{
			bRetry = false;

			// #bug - changing read-only attribute should only be done when user explicitly requests it (via feedback response)
			// for now it is ignored
			try
			{
				if (!bProtectReadOnlyFiles)
					m_spFilesystem->SetAttributes(spFileInfo->GetFullFilePath(), FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY);
			}
			catch (const TFileException&)
			{
			}

			DWORD dwLastError = ERROR_SUCCESS;
			try
			{
				m_spFilesystem->RemoveDirectory(spFileInfo->GetFullFilePath());
				return TSubTaskBase::eSubResult_Continue;
			}
			catch (const TFileException& e)
			{
				dwLastError = e.GetNativeError();
			}

			if (dwLastError == ERROR_PATH_NOT_FOUND || dwLastError == ERROR_FILE_NOT_FOUND)
				return TSubTaskBase::eSubResult_Continue;

			// log
			TString strFormat = _T("Error #%errno while deleting file/folder %path");
			strFormat.Replace(_T("%errno"), boost::lexical_cast<std::wstring>(dwLastError).c_str());
			strFormat.Replace(_T("%path"), spFileInfo->GetFullFilePath().ToString());
			m_rLog.loge(strFormat.c_str());

			TFeedbackResult frResult = m_spFeedbackHandler->FileError(spFileInfo->GetFullFilePath().ToWString(), TString(), EFileError::eDeleteError, dwLastError);
			switch (frResult.GetResult())
			{
			case EFeedbackResult::eResult_Cancel:
				m_rLog.logi(_T("Cancel request while deleting file."));
				return TSubTaskBase::eSubResult_CancelRequest;

			case EFeedbackResult::eResult_Retry:
				bRetry = true;
				break;	// no fcIndex bump, since we are trying again

			case EFeedbackResult::eResult_Pause:
				return TSubTaskBase::eSubResult_PauseRequest;

			case EFeedbackResult::eResult_Skip:
				TSubTaskBase::eSubResult_Continue;		// just do nothing

			default:
				BOOST_ASSERT(FALSE);		// unknown result
				THROW_CORE_EXCEPTION(eErr_UnhandledCase);
			}

			if(WasKillRequested(frResult))
				return TSubTaskBase::eSubResult_KillRequest;
		}
		while (bRetry);

		return TSubTaskBase::eSubResult_Continue;
	}

	TSubTaskBase::ESubOperationResult TFilesystemFeedbackWrapper::DeleteFileFB(const TFileInfoPtr& spFileInfo, bool bProtectReadOnlyFiles)
	{
		bool bRetry = false;
		do
		{
			bRetry = false;

			// #bug - changing read-only attribute should only be done when user explicitly requests it (via feedback response)
			// for now it is ignored
			try
			{
				if (!bProtectReadOnlyFiles)
					m_spFilesystem->SetAttributes(spFileInfo->GetFullFilePath(), FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY);
			}
			catch (const TFileException&)
			{
			}

			DWORD dwLastError = ERROR_SUCCESS;
			try
			{
				m_spFilesystem->DeleteFile(spFileInfo->GetFullFilePath());
				return TSubTaskBase::eSubResult_Continue;
			}
			catch (const TFileException& e)
			{
				dwLastError = e.GetNativeError();
			}

			if (dwLastError == ERROR_PATH_NOT_FOUND || dwLastError == ERROR_FILE_NOT_FOUND)
				return TSubTaskBase::eSubResult_Continue;

			// log
			TString strFormat = _T("Error #%errno while deleting file/folder %path");
			strFormat.Replace(_T("%errno"), boost::lexical_cast<std::wstring>(dwLastError).c_str());
			strFormat.Replace(_T("%path"), spFileInfo->GetFullFilePath().ToString());
			m_rLog.loge(strFormat.c_str());

			TFeedbackResult frResult = m_spFeedbackHandler->FileError(spFileInfo->GetFullFilePath().ToWString(), TString(), EFileError::eDeleteError, dwLastError);
			switch (frResult.GetResult())
			{
			case EFeedbackResult::eResult_Cancel:
				m_rLog.logi(_T("Cancel request while deleting file."));
				return TSubTaskBase::eSubResult_CancelRequest;

			case EFeedbackResult::eResult_Retry:
				bRetry = true;
				break;

			case EFeedbackResult::eResult_Pause:
				return TSubTaskBase::eSubResult_PauseRequest;

			case EFeedbackResult::eResult_Skip:
				return TSubTaskBase::eSubResult_Continue;		// just do nothing

			default:
				BOOST_ASSERT(FALSE);		// unknown result
				THROW_CORE_EXCEPTION(eErr_UnhandledCase);
			}

			if(WasKillRequested(frResult))
				return TSubTaskBase::eSubResult_KillRequest;
		}
		while(bRetry);

		return TSubTaskBase::eSubResult_Continue;
	}

	TSubTaskBase::ESubOperationResult TFilesystemFeedbackWrapper::FastMoveFB(const TFileInfoPtr& spFileInfo, const TSmartPath& pathDestination, const TBasePathDataPtr& spBasePath, bool& bSkip)
	{
		bool bRetry = false;
		do
		{
			bRetry = false;

			TSmartPath pathSrc = spBasePath->GetSrcPath();

			DWORD dwLastError = ERROR_SUCCESS;
			try
			{
				m_spFilesystem->FastMove(pathSrc, pathDestination);
				spBasePath->SetSkipFurtherProcessing(true);		// mark that this path should not be processed any further
				return TSubTaskBase::eSubResult_Continue;
			}
			catch (const TFileException& e)
			{
				dwLastError = e.GetNativeError();
			}

			// check if this is one of the errors, that will just cause fast move to skip
			if (dwLastError == ERROR_ACCESS_DENIED || dwLastError == ERROR_ALREADY_EXISTS || dwLastError == ERROR_NOT_SAME_DEVICE)
			{
				bSkip = true;
				return TSubTaskBase::eSubResult_Continue;
			}

			//log
			TString strFormat = _T("Error %errno while calling fast move %srcpath -> %dstpath (TSubTaskFastMove)");
			strFormat.Replace(_T("%errno"), boost::lexical_cast<std::wstring>(dwLastError).c_str());
			strFormat.Replace(_T("%srcpath"), spFileInfo->GetFullFilePath().ToString());
			strFormat.Replace(_T("%dstpath"), pathDestination.ToString());
			m_rLog.loge(strFormat.c_str());

			TFeedbackResult frResult = m_spFeedbackHandler->FileError(pathSrc.ToWString(), pathDestination.ToWString(), EFileError::eFastMoveError, dwLastError);
			switch (frResult.GetResult())
			{
			case EFeedbackResult::eResult_Cancel:
				return TSubTaskBase::eSubResult_CancelRequest;

			case EFeedbackResult::eResult_Retry:
				bRetry = true;
				break;

			case EFeedbackResult::eResult_Pause:
				return TSubTaskBase::eSubResult_PauseRequest;

			case EFeedbackResult::eResult_Skip:
				bSkip = true;
				return TSubTaskBase::eSubResult_Continue;

			default:
				BOOST_ASSERT(FALSE);		// unknown result
				THROW_CORE_EXCEPTION(eErr_UnhandledCase);
			}

			if(WasKillRequested(frResult))
				return TSubTaskBase::eSubResult_KillRequest;
		}
		while (bRetry);

		return TSubTaskBase::eSubResult_Continue;
	}

	TSubTaskBase::ESubOperationResult TFilesystemFeedbackWrapper::GetFileInfoFB(const TSmartPath& pathCurrent, TFileInfoPtr& spFileInfo, const TBasePathDataPtr& spBasePath, bool& bSkip)
	{
		bool bRetry = false;
		do
		{
			bRetry = false;

			// read attributes of src file/folder
			DWORD dwLastError = ERROR_SUCCESS;
			try
			{
				m_spFilesystem->GetFileInfo(pathCurrent, spFileInfo, spBasePath);
				return TSubTaskBase::eSubResult_Continue;
			}
			catch (const TFileException& e)
			{
				dwLastError = e.GetNativeError();
			}

			TFeedbackResult frResult = m_spFeedbackHandler->FileError(pathCurrent.ToWString(), TString(), EFileError::eFastMoveError, dwLastError);
			switch (frResult.GetResult())
			{
			case EFeedbackResult::eResult_Cancel:
				return TSubTaskBase::eSubResult_CancelRequest;

			case EFeedbackResult::eResult_Retry:
				bRetry = true;
				break;

			case EFeedbackResult::eResult_Pause:
				return TSubTaskBase::eSubResult_PauseRequest;

			case EFeedbackResult::eResult_Skip:
				bSkip = true;
				return TSubTaskBase::eSubResult_Continue;

			default:
				BOOST_ASSERT(FALSE);		// unknown result
				THROW_CORE_EXCEPTION(eErr_UnhandledCase);
			}

			if(WasKillRequested(frResult))
				return TSubTaskBase::eSubResult_KillRequest;
		}
		while(bRetry);

		return TSubTaskBase::eSubResult_Continue;
	}
}