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
#include "Stdafx.h"
#include "task.h"
#include "StringHelpers.h"
#include "../common/FileSupport.h"
#include "ch.h"
#include "FeedbackHandler.h"
#include <boost/serialization/serialization.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/make_shared.hpp>
#include <fstream>

// assume max sectors of 4kB (for rounding)
#define MAXSECTORSIZE			4096

///////////////////////////////////////////////////////////////////////
// CProcessingException

CProcessingException::CProcessingException(int iType, UINT uiFmtID, DWORD dwError, ...)
{
	// std values
	m_iType=iType;
	m_dwError=dwError;

	// format some text
	CString strFormat=GetResManager().LoadString(uiFmtID);
	ExpandFormatString(&strFormat, dwError);

	// get param list
	va_list marker;
	va_start(marker, dwError);
	m_strErrorDesc.FormatV(strFormat, marker);
	va_end(marker);
}

CProcessingException::CProcessingException(int iType, DWORD dwError, const tchar_t* pszDesc)
{
	// std values
	m_iType=iType;
	m_dwError=dwError;

	// format some text
	m_strErrorDesc = pszDesc;
}

////////////////////////////////////////////////////////////////////////////
// CTask members
CTask::CTask(chcore::IFeedbackHandler* piFeedbackHandler, size_t stSessionUniqueID, TTasksGlobalStats& tGlobalStats) :
	m_log(),
	m_piFeedbackHandler(piFeedbackHandler),
	m_files(m_clipboard),
	m_stCurrentIndex(0),
	m_stLastProcessedIndex(0),
	m_nStatus(ST_NULL_STATUS),
	m_pThread(NULL),
	m_nPriority(THREAD_PRIORITY_NORMAL),
	m_nProcessed(0),
	m_nAll(0),
	m_bKill(false),
	m_bKilled(true),
	m_lTimeElapsed(0),
	m_lLastTime(-1),
	m_bQueued(false),
	m_ucCopies(1),
	m_ucCurrentCopy(0),
	m_uiResumeInterval(0),
	m_bForce(false),
	m_bContinue(false),
	m_bSaved(false),
	m_lOsError(0),
	m_stSessionUniqueID(stSessionUniqueID),
	m_rtGlobalStats(tGlobalStats),
	m_bRegisteredAsRunning(false)

{
	BOOST_ASSERT(piFeedbackHandler);

	m_bsSizes.m_uiDefaultSize=65536;
	m_bsSizes.m_uiOneDiskSize=4194304;
	m_bsSizes.m_uiTwoDisksSize=262144;
	m_bsSizes.m_uiCDSize=262144;
	m_bsSizes.m_uiLANSize=65536;

	_itot((int)time(NULL), m_strUniqueName.GetBufferSetLength(16), 10);
	m_strUniqueName.ReleaseBuffer();
}

CTask::~CTask()
{
	KillThread();
	if(m_piFeedbackHandler)
		m_piFeedbackHandler->Delete();
	UnregisterTaskAsRunningNL();
}

// m_clipboard
void CTask::AddClipboardData(const CClipboardEntryPtr& spEntry)
{
	m_clipboard.Add(spEntry);
}

CClipboardEntryPtr CTask::GetClipboardData(size_t stIndex)
{
   return m_clipboard.GetAt(stIndex);
}

size_t CTask::GetClipboardDataSize()
{
   return m_clipboard.GetSize();
}

int CTask::ReplaceClipboardStrings(CString strOld, CString strNew)
{
	return m_clipboard.ReplacePathsPrefix(strOld, strNew);
}

// m_files
int CTask::FilesAddDir(CString strDirName, size_t stSrcIndex, bool bRecurse, bool bIncludeDirs)
{
	WIN32_FIND_DATA wfd;
	CString strText;

	// append '\\' at the end of path if needed
	if(strDirName.Right(1) != _T("\\"))
		strDirName += _T("\\");

	strText = strDirName + _T("*");

	// Iterate through dirs & files
	HANDLE hFind = FindFirstFile(strText, &wfd);
	if(hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if(!(wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				CFileInfoPtr spFileInfo(boost::make_shared<CFileInfo>());
				spFileInfo->SetClipboard(&m_clipboard);	// this is the link table (CClipboardArray)
				
				spFileInfo->Create(&wfd, strDirName, stSrcIndex);
				if(m_afFilters.Match(spFileInfo))
					m_files.AddFileInfo(spFileInfo);
			}
			else if(_tcscmp(wfd.cFileName, _T(".")) != 0 && _tcscmp(wfd.cFileName, _T("..")) != 0)
			{
				if(bIncludeDirs)
				{
					CFileInfoPtr spFileInfo(boost::make_shared<CFileInfo>());
					spFileInfo->SetClipboard(&m_clipboard);	// this is the link table (CClipboardArray)

					// Add directory itself
					spFileInfo->Create(&wfd, strDirName, stSrcIndex);
					m_files.AddFileInfo(spFileInfo);
				}
				if(bRecurse)
				{
					strText = strDirName + wfd.cFileName + _T("\\");
					// Recurse Dirs
					FilesAddDir(strText, stSrcIndex, bRecurse, bIncludeDirs);
				}
			}
		}
		while(!m_bKill && (FindNextFile(hFind, &wfd)));
		
		FindClose(hFind);
	}

	return 0;
}

void CTask::FilesAdd(const CFileInfoPtr& spFileInfo)
{
	if(spFileInfo->IsDirectory() || m_afFilters.Match(spFileInfo))
		m_files.AddFileInfo(spFileInfo);
}

CFileInfoPtr CTask::FilesGetAt(size_t stIndex)
{
	return m_files.GetAt(stIndex);
}

CFileInfoPtr CTask::FilesGetAtCurrentIndex()
{
	size_t stCurrentIndex = 0;

	m_lock.lock_shared();
	stCurrentIndex = m_stCurrentIndex;
	m_lock.unlock_shared();

	return m_files.GetAt(m_stCurrentIndex);
}

void CTask::FilesRemoveAll()
{
	m_files.Clear();
}

size_t CTask::FilesGetSize()
{
	return m_files.GetSize();
}

// m_stCurrentIndex
void CTask::IncreaseCurrentIndex()
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	++m_stCurrentIndex;
}

size_t CTask::GetCurrentIndex()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_stCurrentIndex;
}

void CTask::SetCurrentIndex(size_t stIndex)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_stCurrentIndex = stIndex;
}

// m_strDestPath - adds '\\'
void CTask::SetDestPath(LPCTSTR lpszPath)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_dpDestPath.SetPath(lpszPath);
}

// guaranteed '\\'
const CDestPath& CTask::GetDestPath()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_dpDestPath;
}

int CTask::GetDestDriveNumber()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_dpDestPath.GetDriveNumber();
}

// m_nStatus
void CTask::SetStatus(UINT nStatus, UINT nMask)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_nStatus &= ~nMask;
	m_nStatus |= nStatus;
}

UINT CTask::GetStatus(UINT nMask)
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_nStatus & nMask;
}

// m_nBufferSize
void CTask::SetBufferSizes(const BUFFERSIZES* bsSizes)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_bsSizes=*bsSizes;
	m_bSaved=false;
}

const BUFFERSIZES* CTask::GetBufferSizes()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return &m_bsSizes;
}

int CTask::GetCurrentBufferIndex()
{
	int rv = 0;

	boost::shared_lock<boost::shared_mutex> lock(m_lock);

	size_t stSize = m_files.GetSize();
	if(stSize > 0 && m_stCurrentIndex != -1)
		rv = m_bsSizes.m_bOnlyDefault ? 0 : m_files.GetAt((m_stCurrentIndex < stSize) ? m_stCurrentIndex : 0)->GetBufferIndex();

	return rv;
}

// m_pThread
// m_nPriority
int CTask::GetPriority()
{
   boost::shared_lock<boost::shared_mutex> lock(m_lock);

   return m_nPriority;
}

void CTask::SetPriority(int nPriority)
{
	if(m_pThread != NULL)
	{
		m_pThread->SuspendThread();
		m_pThread->SetThreadPriority(nPriority);
		m_pThread->ResumeThread();
	}
	
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_nPriority = nPriority;
	m_bSaved = false;
}

// m_nProcessed
void CTask::IncreaseProcessedSize(__int64 nSize)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_nProcessed += nSize;
}

void CTask::SetProcessedSize(__int64 nSize)
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_nProcessed = nSize;
}

__int64 CTask::GetProcessedSize()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_nProcessed;
}

// m_nAll
void CTask::SetAllSize(__int64 nSize)
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_nAll=nSize;
}

__int64 CTask::GetAllSize()
{
   boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_nAll;
}

void CTask::CalcAllSize()
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_nAll=0;

	size_t nSize = m_files.GetSize();
	for (size_t i = 0; i < nSize; i++)
	{
		m_nAll += m_files.GetAt(i)->GetLength64();
	}

	m_nAll *= m_ucCopies;
}

void CTask::CalcProcessedSize()
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_nProcessed = 0;

	// count all from previous passes
	if(m_ucCopies)
		m_nProcessed += m_ucCurrentCopy*(m_nAll/m_ucCopies);
	else
		m_nProcessed += m_ucCurrentCopy*m_nAll;

	for(size_t stIndex = 0; stIndex < m_stCurrentIndex; ++stIndex)
	{
		m_nProcessed += m_files.GetAt(stIndex)->GetLength64();
	}
	m_rtGlobalStats.IncreaseGlobalTasksPosition(m_nProcessed);
}

void CTask::RegisterTaskAsRunning()
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	if(!m_bRegisteredAsRunning)
	{
		m_bRegisteredAsRunning = true;
		m_rtGlobalStats.IncreaseRunningTasks();
	}
}

void CTask::RegisterTaskAsRunningNL()
{
	if(!m_bRegisteredAsRunning)
	{
		m_bRegisteredAsRunning = true;
		m_rtGlobalStats.IncreaseRunningTasks();
	}
}

void CTask::UnregisterTaskAsRunning()
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	if(m_bRegisteredAsRunning)
	{
		m_bRegisteredAsRunning = false;
		m_rtGlobalStats.DecreaseRunningTasks();
	}
}

void CTask::UnregisterTaskAsRunningNL()
{
	if(m_bRegisteredAsRunning)
	{
		m_bRegisteredAsRunning = false;
		m_rtGlobalStats.DecreaseRunningTasks();
	}
}

// m_bKill
void CTask::SetKillFlag(bool bKill)
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_bKill=bKill;
}

bool CTask::GetKillFlag()
{
   boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_bKill;
}

// m_bKilled
void CTask::SetKilledFlag(bool bKilled)
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_bKilled=bKilled;
}

bool CTask::GetKilledFlag()
{
   boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_bKilled;
}

// m_strUniqueName

CString CTask::GetUniqueName()
{
   boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_strUniqueName;
}

void CTask::Load(const CString& strPath, bool bData)
{
	std::ifstream ifs(strPath, ios_base::in | ios_base::binary);
	boost::archive::binary_iarchive ar(ifs);

   boost::unique_lock<boost::shared_mutex> lock(m_lock);
	if(bData)
	{
		m_clipboard.Load(ar, 0, bData);

		m_files.Load(ar, 0, false);

		ar >> m_dpDestPath;

		ar >> m_strUniqueName;
		ar >> m_afFilters;
		ar >> m_ucCopies;
	}
	else
	{
		size_t stData = 0;
		UINT uiData = 0;

		ar >> stData;
		m_stCurrentIndex = stData;
		ar >> uiData;
		m_nStatus = uiData;
		ar >> m_lOsError;

		ar >> m_strErrorDesc;

		ar >> m_bsSizes;
		ar >> m_nPriority;
		ar >> m_nAll;
		ar >> m_lTimeElapsed;

		ar >> m_nProcessed;
		ar >> m_ucCurrentCopy;

		m_clipboard.Load(ar, 0, bData);
		m_files.Load(ar, 0, true);

		ar >> m_bSaved;
	}
}

void CTask::Store(bool bData)
{
   boost::shared_lock<boost::shared_mutex> lock(m_lock);
	BOOST_ASSERT(!m_strTaskBasePath.empty());
	if(m_strTaskBasePath.empty())
		THROW(_t("Missing task path."), 0, 0, 0);

   if(!bData && m_bSaved)
	{
		TRACE("Saving locked - file not saved\n");
		return;
	}

	if(!bData && !m_bSaved && ( (m_nStatus & ST_STEP_MASK) == ST_FINISHED || (m_nStatus & ST_STEP_MASK) == ST_CANCELLED
		|| (m_nStatus & ST_WORKING_MASK) == ST_PAUSED ))
	{
		TRACE("Last save - saving blocked\n");
		m_bSaved = true;
	}

	CString strPath = m_strTaskBasePath.c_str() + GetUniqueNameNL() + (bData ? _T(".atd") : _T(".atp"));

	std::ofstream ofs(strPath, ios_base::out | ios_base::binary);
	boost::archive::binary_oarchive ar(ofs);

	if(bData)
	{
		m_clipboard.Store(ar, 0, bData);

		if(GetStatusNL(ST_STEP_MASK) > ST_SEARCHING)
			m_files.Store(ar, 0, false);
		else
		{
			size_t st(0);
			ar << st;
		}

		ar << m_dpDestPath;
		ar << m_strUniqueName;
		ar << m_afFilters;
		ar << m_ucCopies;
	}
	else
	{
		size_t stCurrentIndex = m_stCurrentIndex;
		ar << stCurrentIndex;
		UINT uiStatus = (m_nStatus & ST_WRITE_MASK);
		ar << uiStatus;
		ar << m_lOsError;

		ar << m_strErrorDesc;

		ar << m_bsSizes;
		ar << m_nPriority;
		ar << m_nAll;
		ar << m_lTimeElapsed;

		ar << m_nProcessed;
		ar << m_ucCurrentCopy;

		m_clipboard.Store(ar, 0, bData);
		if(GetStatusNL(ST_STEP_MASK) > ST_SEARCHING)
			m_files.Store(ar, 0, true);
		else
		{
			size_t st(0);
			ar << st;
		}
		ar << m_bSaved;
	}
}

void CTask::KillThread()
{
	if(!GetKilledFlag())	// protection from recalling Cleanup
	{
		SetKillFlag();
		while(!GetKilledFlag())
			Sleep(10);

		// cleanup
		CleanupAfterKill();
	}
}

void CTask::BeginProcessing()
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);
	if(m_pThread != NULL)
		return;

	// create new thread
	m_uiResumeInterval=0;	// just in case
	m_bSaved=false;			// save
	SetKillFlagNL(false);
	SetKilledFlagNL(false);
	m_pThread = AfxBeginThread(ThrdProc, this, GetPriorityNL());
}

void CTask::ResumeProcessing()
{
	// the same as retry but less demanding
	if( (GetStatus(ST_WORKING_MASK) & ST_PAUSED) && GetStatus(ST_STEP_MASK) != ST_FINISHED
		&& GetStatus(ST_STEP_MASK) != ST_CANCELLED)
	{
		SetStatus(0, ST_ERROR);
		BeginProcessing();
	}
}

bool CTask::RetryProcessing(bool bOnlyErrors/*=false*/, UINT uiInterval)
{
	// retry used to auto-resume, after loading
	if( (GetStatus(ST_WORKING_MASK) == ST_ERROR || (!bOnlyErrors && GetStatus(ST_WORKING_MASK) != ST_PAUSED))
		&& GetStatus(ST_STEP_MASK) != ST_FINISHED && GetStatus(ST_STEP_MASK) != ST_CANCELLED)
	{
		if(uiInterval != 0)
		{
			m_uiResumeInterval+=uiInterval;
			if(m_uiResumeInterval < (UINT)GetConfig().get_signed_num(PP_CMAUTORETRYINTERVAL))
				return false;
			else
				m_uiResumeInterval=0;
		}

		SetStatus(0, ST_ERROR);
		BeginProcessing();
		return true;
	}
	return false;
}

void CTask::RestartProcessing()
{
	KillThread();
	SetStatus(0, ST_ERROR);
	SetStatus(ST_NULL_STATUS, ST_STEP_MASK);
	m_lTimeElapsed=0;
	SetCurrentIndex(0);
	SetCurrentCopy(0);
	BeginProcessing();
}

void CTask::PauseProcessing()
{
	if(GetStatus(ST_STEP_MASK) != ST_FINISHED && GetStatus(ST_STEP_MASK) != ST_CANCELLED)
	{
		KillThread();
		SetStatus(ST_PAUSED, ST_WORKING_MASK);
		SetLastProcessedIndex(GetCurrentIndex());
		m_bSaved=false;
	}
}

void CTask::CancelProcessing()
{
	// change to ST_CANCELLED
	if(GetStatus(ST_STEP_MASK) != ST_FINISHED)
	{
		KillThread();
		SetStatus(ST_CANCELLED, ST_STEP_MASK);
		SetStatus(0, ST_ERROR);
		m_bSaved=false;
	}
}

void CTask::GetMiniSnapshot(TASK_MINI_DISPLAY_DATA *pData)
{
   boost::shared_lock<boost::shared_mutex> lock(m_lock);
	if(m_stCurrentIndex >= 0 && m_stCurrentIndex < m_files.GetSize())
		pData->m_spFileInfo = m_files.GetAt(m_stCurrentIndex);
	else
	{
		if(m_files.GetSize() > 0)
		{
			pData->m_spFileInfo = m_files.GetAt(0);
			pData->m_spFileInfo->SetFilePath(pData->m_spFileInfo->GetFullFilePath());
			pData->m_spFileInfo->SetSrcIndex(-1);
		}
		else
		{
			if(m_clipboard.GetSize() > 0)
			{
				pData->m_spFileInfo->SetFilePath(m_clipboard.GetAt(0)->GetPath());
				pData->m_spFileInfo->SetSrcIndex(-1);
			}
			else
			{
				pData->m_spFileInfo->SetFilePath(GetResManager().LoadString(IDS_NONEINPUTFILE_STRING));
				pData->m_spFileInfo->SetSrcIndex(-1);
			}
		}
	}

	pData->m_uiStatus=m_nStatus;

	// percents
	size_t stSize = m_files.GetSize() * m_ucCopies;
	size_t stIndex = m_stCurrentIndex + m_ucCurrentCopy * m_files.GetSize();
	if(m_nAll != 0 && !((m_nStatus & ST_SPECIAL_MASK) & ST_IGNORE_CONTENT))
		pData->m_nPercent=static_cast<int>( (static_cast<double>(m_nProcessed)*100.0)/static_cast<double>(m_nAll) );
	else
   {
      if(stSize != 0)
         pData->m_nPercent=static_cast<int>( static_cast<double>(stIndex)*100.0/static_cast<double>(stSize) );
      else
         pData->m_nPercent=0;
   }
}

void CTask::GetSnapshot(TASK_DISPLAY_DATA *pData)
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);

	if(m_stCurrentIndex >= 0 && m_stCurrentIndex < m_files.GetSize())
		pData->m_spFileInfo = m_files.GetAt(m_stCurrentIndex);
	else
	{
		if(m_files.GetSize() > 0)
		{
			pData->m_spFileInfo = m_files.GetAt(0);
			pData->m_spFileInfo->SetFilePath(pData->m_spFileInfo->GetFullFilePath());
			pData->m_spFileInfo->SetSrcIndex(-1);
		}
		else
		{
			if(m_clipboard.GetSize() > 0)
			{
				pData->m_spFileInfo->SetFilePath(m_clipboard.GetAt(0)->GetPath());
				pData->m_spFileInfo->SetSrcIndex(-1);
			}
			else
			{
				pData->m_spFileInfo->SetFilePath(GetResManager().LoadString(IDS_NONEINPUTFILE_STRING));
				pData->m_spFileInfo->SetSrcIndex(-1);
			}
		}
	}

	pData->m_pbsSizes=&m_bsSizes;
	pData->m_nPriority=m_nPriority;
	pData->m_pdpDestPath=&m_dpDestPath;
	pData->m_pafFilters=&m_afFilters;
	pData->m_dwOsErrorCode=m_lOsError;
	pData->m_strErrorDesc=m_strErrorDesc;
	pData->m_uiStatus=m_nStatus;
	pData->m_stIndex=m_stCurrentIndex+m_ucCurrentCopy*m_files.GetSize();
	pData->m_ullProcessedSize=m_nProcessed;
	pData->m_stSize=m_files.GetSize()*m_ucCopies;
	pData->m_ullSizeAll=m_nAll;
	pData->m_ucCurrentCopy=static_cast<unsigned char>(m_ucCurrentCopy+1);	// visual aspect
	pData->m_ucCopies=m_ucCopies;
	pData->m_pstrUniqueName=&m_strUniqueName;

	if(m_files.GetSize() > 0 && m_stCurrentIndex != -1)
		pData->m_iCurrentBufferIndex=m_bsSizes.m_bOnlyDefault ? 0 : m_files.GetAt((m_stCurrentIndex < m_files.GetSize()) ? m_stCurrentIndex : 0)->GetBufferIndex();
	else
		pData->m_iCurrentBufferIndex=0;

	// percents
	if(m_nAll != 0 && !((m_nStatus & ST_SPECIAL_MASK) & ST_IGNORE_CONTENT))
		pData->m_nPercent=static_cast<int>( (static_cast<double>(m_nProcessed)*100.0)/static_cast<double>(m_nAll) );
	else
		if(pData->m_stSize != 0)
			pData->m_nPercent=static_cast<int>( static_cast<double>(pData->m_stIndex)*100.0/static_cast<double>(pData->m_stSize) );
		else
			pData->m_nPercent=0;

	// status string
	// first
	if( (m_nStatus & ST_WORKING_MASK) == ST_ERROR )
	{
		GetResManager().LoadStringCopy(IDS_STATUS0_STRING+4, pData->m_szStatusText, _MAX_PATH);
		_tcscat(pData->m_szStatusText, _T("/"));
	}
	else if( (m_nStatus & ST_WORKING_MASK) == ST_PAUSED )
	{
		GetResManager().LoadStringCopy(IDS_STATUS0_STRING+5, pData->m_szStatusText, _MAX_PATH);
		_tcscat(pData->m_szStatusText, _T("/"));
	}
	else if( (m_nStatus & ST_STEP_MASK) == ST_FINISHED )
	{
		GetResManager().LoadStringCopy(IDS_STATUS0_STRING+3, pData->m_szStatusText, _MAX_PATH);
		_tcscat(pData->m_szStatusText, _T("/"));
	}
	else if( (m_nStatus & ST_WAITING_MASK) == ST_WAITING )
	{
		GetResManager().LoadStringCopy(IDS_STATUS0_STRING+9, pData->m_szStatusText, _MAX_PATH);
		_tcscat(pData->m_szStatusText, _T("/"));
	}
	else if( (m_nStatus & ST_STEP_MASK) == ST_CANCELLED )
	{
		GetResManager().LoadStringCopy(IDS_STATUS0_STRING+8, pData->m_szStatusText, _MAX_PATH);
		_tcscat(pData->m_szStatusText, _T("/"));
	}
	else
		_tcscpy(pData->m_szStatusText, _T(""));

	// second part
	if( (m_nStatus & ST_STEP_MASK) == ST_DELETING )
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+6));
	else if( (m_nStatus & ST_STEP_MASK) == ST_SEARCHING )
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+0));
	else if((m_nStatus & ST_OPERATION_MASK) == ST_COPY )
	{
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+1));
		if(!m_afFilters.IsEmpty())
			_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_FILTERING_STRING));
	}
	else if( (m_nStatus & ST_OPERATION_MASK) == ST_MOVE )
	{
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+2));
		if(!m_afFilters.IsEmpty())
			_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_FILTERING_STRING));
	}
	else
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+7));

	// third part
	if( (m_nStatus & ST_SPECIAL_MASK) & ST_IGNORE_DIRS )
	{
		_tcscat(pData->m_szStatusText, _T("/"));
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+10));
	}
	if( (m_nStatus & ST_SPECIAL_MASK) & ST_IGNORE_CONTENT )
	{
		_tcscat(pData->m_szStatusText, _T("/"));
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+11));
	}

	// count of copies
	if(m_ucCopies > 1)
	{
		_tcscat(pData->m_szStatusText, _T("/"));
		TCHAR xx[4];
		_tcscat(pData->m_szStatusText, _itot(m_ucCopies, xx, 10));
		if(m_ucCopies < 5)
			_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_COPYWORDLESSFIVE_STRING));
		else
			_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_COPYWORDMOREFOUR_STRING));
	}

	// time
	UpdateTimeNL();
	pData->m_lTimeElapsed=m_lTimeElapsed;
}

/*inline*/ void CTask::CleanupAfterKill()
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);

   CleanupAfterKillNL();
}

void CTask::DeleteProgress(LPCTSTR lpszDirectory)
{
   m_lock.lock_shared();

   CString strDel1 = lpszDirectory+m_strUniqueName+_T(".atd");
   CString strDel2 = lpszDirectory+m_strUniqueName+_T(".atp");
   CString strDel3 = lpszDirectory+m_strUniqueName+_T(".log");

   m_lock.unlock_shared();

   DeleteFile(strDel1);
   DeleteFile(strDel2);
   DeleteFile(strDel3);
}

void CTask::SetOsErrorCode(DWORD dwError, LPCTSTR lpszErrDesc)
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_lOsError=dwError;
	m_strErrorDesc=lpszErrDesc;
}

void CTask::UpdateTime()
{
   boost::unique_lock<boost::shared_mutex> lock(m_lock);

   UpdateTimeNL();
}

void CTask::SetFilters(const CFiltersArray* pFilters)
{
	BOOST_ASSERT(pFilters);
	if(!pFilters)
		THROW(_T("Invalid argument"), 0, 0, 0);

	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_afFilters = *pFilters;
}

bool CTask::CanBegin()
{
	bool bRet=true;
	boost::unique_lock<boost::shared_mutex> lock(m_lock);

	if(GetContinueFlagNL() || GetForceFlagNL())
	{
		RegisterTaskAsRunningNL();
		SetForceFlagNL(false);
		SetContinueFlagNL(false);
	}
	else
		bRet = false;

	return bRet;
}

void CTask::SetCopies(unsigned char ucCopies)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_ucCopies=ucCopies;
}

unsigned char CTask::GetCopies()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_ucCopies;
}

void CTask::SetCurrentCopy(unsigned char ucCopy)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_ucCurrentCopy=ucCopy;
}

unsigned char CTask::GetCurrentCopy()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_ucCurrentCopy;
}

void CTask::SetLastProcessedIndex(size_t stIndex)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_stLastProcessedIndex = stIndex;
}

size_t CTask::GetLastProcessedIndex()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_stLastProcessedIndex;
}

bool CTask::GetRequiredFreeSpace(ull_t *pullNeeded, ull_t *pullAvailable)
{
	*pullNeeded=GetAllSize()-GetProcessedSize(); // it'd be nice to round up to take cluster size into consideration,
	// but GetDiskFreeSpace returns flase values

	// get free space
	if(!GetDynamicFreeSpace(GetDestPath().GetPath(), pullAvailable, NULL))
		return true;

	return (*pullNeeded <= *pullAvailable);
}

void CTask::SetTaskPath(const tchar_t* pszDir)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_strTaskBasePath = pszDir;
}

const tchar_t* CTask::GetTaskPath() const
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_strTaskBasePath.c_str();
}

void CTask::SetForceFlag(bool bFlag)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_bForce=bFlag;
}

bool CTask::GetForceFlag()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_bForce;
}

void CTask::SetContinueFlag(bool bFlag)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_bContinue=bFlag;
}

bool CTask::GetContinueFlag()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_bContinue;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTask::SetFileDirectoryTime(LPCTSTR lpszName, const CFileInfoPtr& spFileInfo)
{
	TAutoFileHandle hFile = CreateFile(lpszName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | (spFileInfo->IsDirectory() ? FILE_FLAG_BACKUP_SEMANTICS : 0), NULL);
	if(hFile == INVALID_HANDLE_VALUE)
		return false;

	BOOL bResult = (!SetFileTime(hFile, &spFileInfo->GetCreationTime(), &spFileInfo->GetLastAccessTime(), &spFileInfo->GetLastWriteTime()));

	if(!hFile.Close())
		return false;

	return bResult != 0;
}

// m_stCurrentIndex
void CTask::IncreaseCurrentIndexNL()
{
	++m_stCurrentIndex;
}

size_t CTask::GetCurrentIndexNL()
{
	return m_stCurrentIndex;
}

void CTask::SetCurrentIndexNL(size_t stIndex)
{
	m_stCurrentIndex = stIndex;
}

// m_strDestPath - adds '\\'
void CTask::SetDestPathNL(LPCTSTR lpszPath)
{
	m_dpDestPath.SetPath(lpszPath);
}

// guaranteed '\\'
const CDestPath& CTask::GetDestPathNL()
{
	return m_dpDestPath;
}

int CTask::GetDestDriveNumberNL()
{
	return m_dpDestPath.GetDriveNumber();
}

// m_nStatus
void CTask::SetStatusNL(UINT nStatus, UINT nMask)
{
	m_nStatus &= ~nMask;
	m_nStatus |= nStatus;
}

UINT CTask::GetStatusNL(UINT nMask)
{
	return m_nStatus & nMask;
}

// m_nBufferSize
void CTask::SetBufferSizesNL(const BUFFERSIZES* bsSizes)
{
	m_bsSizes = *bsSizes;
	m_bSaved = false;
}

const BUFFERSIZES* CTask::GetBufferSizesNL()
{
	return &m_bsSizes;
}

int CTask::GetCurrentBufferIndexNL()
{
	int rv = 0;

	size_t stSize = m_files.GetSize();
	if(stSize > 0 && m_stCurrentIndex != -1)
		rv = m_bsSizes.m_bOnlyDefault ? 0 : m_files.GetAt((m_stCurrentIndex < stSize) ? m_stCurrentIndex : 0)->GetBufferIndex();

	return rv;
}

// m_pThread
// m_nPriority
int CTask::GetPriorityNL()
{
	return m_nPriority;
}

void CTask::SetPriorityNL(int nPriority)
{
	if(m_pThread != NULL)
	{
		m_pThread->SuspendThread();
		m_pThread->SetThreadPriority(nPriority);
		m_pThread->ResumeThread();
	}

	m_nPriority = nPriority;
	m_bSaved = false;
}

// m_nProcessed
void CTask::IncreaseProcessedSizeNL(__int64 nSize)
{
	m_nProcessed += nSize;
}

void CTask::SetProcessedSizeNL(__int64 nSize)
{
	m_nProcessed = nSize;
}

__int64 CTask::GetProcessedSizeNL()
{
	return m_nProcessed;
}

// m_nAll
void CTask::SetAllSizeNL(__int64 nSize)
{
	m_nAll = nSize;
}

__int64 CTask::GetAllSizeNL()
{
	return m_nAll;
}

void CTask::CalcAllSizeNL()
{
	m_nAll = 0;

	size_t nSize = m_files.GetSize();
	for (size_t i = 0; i < nSize; i++)
	{
		m_nAll += m_files.GetAt(i)->GetLength64();
	}

	m_nAll *= m_ucCopies;
}

void CTask::SetKillFlagNL(bool bKill)
{
	m_bKill = bKill;
}

bool CTask::GetKillFlagNL()
{
	return m_bKill;
}

void CTask::SetKilledFlagNL(bool bKilled)
{
	m_bKilled = bKilled;
}

bool CTask::GetKilledFlagNL()
{
	return m_bKilled;
}

void CTask::CleanupAfterKillNL()
{
	m_pThread = NULL;
	UpdateTimeNL();
}

void CTask::UpdateTimeNL()
{
	if(m_lLastTime != -1)
	{
		long lVal = (long)time(NULL);
		m_lTimeElapsed += lVal - m_lLastTime;
		m_lLastTime = lVal;
	}
}

CString CTask::GetUniqueNameNL()
{
	return m_strUniqueName;
}

void CTask::SetForceFlagNL(bool bFlag)
{
	m_bForce=bFlag;
}

bool CTask::GetForceFlagNL()
{
	return m_bForce;
}

void CTask::SetContinueFlagNL(bool bFlag)
{
	m_bContinue=bFlag;
}

bool CTask::GetContinueFlagNL()
{
	return m_bContinue;
}

// searching for files
void CTask::RecurseDirectories()
{
	// log
	m_log.logi(_T("Searching for files..."));

	// update status
	SetStatus(ST_SEARCHING, ST_STEP_MASK);

	// delete the content of m_files
	FilesRemoveAll();

	// enter some data to m_files
	size_t stSize = GetClipboardDataSize();	// size of m_clipboard
	int iDestDrvNumber = GetDestDriveNumber();
	bool bIgnoreDirs = (GetStatus(ST_SPECIAL_MASK) & ST_IGNORE_DIRS) != 0;
	bool bForceDirectories = (GetStatus(ST_SPECIAL_MASK) & ST_FORCE_DIRS) != 0;
	bool bMove = GetStatus(ST_OPERATION_MASK) == ST_MOVE;
	CFileInfoPtr spFileInfo(boost::make_shared<CFileInfo>());
	spFileInfo->SetClipboard(GetClipboard());

	// add everything
	ictranslate::CFormat fmt;
	bool bRetry = true;
	bool bSkipInputPath = false;

	for(size_t i = 0; i < stSize ; i++)
	{
		bSkipInputPath = false;
		bRetry = false;

		// try to get some info about the input path; let user know if the path does not exist.
		do
		{
			// read attributes of src file/folder
			bool bExists = spFileInfo->Create(GetClipboardData(i)->GetPath(), i);
			if(!bExists)
			{
				chcore::IFeedbackHandler* piFeedbackHandler = GetFeedbackHandler();
				BOOST_ASSERT(piFeedbackHandler);

				CString strSrcFile = GetClipboardData(i)->GetPath();
				FEEDBACK_FILEERROR ferr = { (PCTSTR)strSrcFile, NULL, eFastMoveError, ERROR_FILE_NOT_FOUND };
				CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileError, &ferr);
				switch(frResult)
				{
				case CFeedbackHandler::eResult_Cancel:
					throw new CProcessingException(E_CANCEL);
					break;
				case CFeedbackHandler::eResult_Retry:
					bRetry = true;
					continue;
					break;
				case CFeedbackHandler::eResult_Pause:
					throw new CProcessingException(E_PAUSE);
					break;
				case CFeedbackHandler::eResult_Skip:
					bSkipInputPath = true;
					bRetry = false;
					break;		// just do nothing
				default:
					BOOST_ASSERT(FALSE);		// unknown result
					throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
				}
			}
		}
		while(bRetry);

		// if we have chosen to skip the input path then there's nothing to do
		if(bSkipInputPath)
			continue;

		// log
		fmt.SetFormat(_T("Adding file/folder (clipboard) : %path ..."));
		fmt.SetParam(_t("%path"), GetClipboardData(i)->GetPath());
		m_log.logi(fmt);

		// found file/folder - check if the dest name has been generated
		if(GetClipboardData(i)->GetDestinationPathsCount() == 0)
		{
			// generate something - if dest folder == src folder - search for copy
			if(GetDestPath().GetPath() == spFileInfo->GetFileRoot())
			{
				CString strSubst;
				FindFreeSubstituteName(spFileInfo->GetFullFilePath(), GetDestPath().GetPath(), &strSubst);
				GetClipboardData(i)->AddDestinationPath(strSubst);
			}
			else
				GetClipboardData(i)->AddDestinationPath(spFileInfo->GetFileName());
		}

		// add if needed
		if(spFileInfo->IsDirectory())
		{
			// add if folder's aren't ignored
			if(!bIgnoreDirs && !bForceDirectories)
			{
				FilesAdd(spFileInfo);

				// log
				fmt.SetFormat(_T("Added folder %path"));
				fmt.SetParam(_t("%path"), spFileInfo->GetFullFilePath());
				m_log.logi(fmt);
			}

			// don't add folder contents when moving inside one disk boundary
			if(bIgnoreDirs || !bMove || GetCopies() > 1 || iDestDrvNumber == -1
				|| iDestDrvNumber != spFileInfo->GetDriveNumber() || CFileInfo::Exist(spFileInfo->GetDestinationPath(GetDestPath().GetPath(), 0, ((int)bForceDirectories) << 1)) )
			{
				// log
				fmt.SetFormat(_T("Recursing folder %path"));
				fmt.SetParam(_t("%path"), spFileInfo->GetFullFilePath());
				m_log.logi(fmt);

				// no movefile possibility - use CustomCopyFile
				GetClipboardData(i)->SetMove(false);

				FilesAddDir(spFileInfo->GetFullFilePath(), i, true, !bIgnoreDirs || bForceDirectories);
			}

			// check for kill need
			if(GetKillFlag())
			{
				// log
				m_log.logi(_T("Kill request while adding data to files array (RecurseDirectories)"));
				throw new CProcessingException(E_KILL_REQUEST);
			}
		}
		else
		{
			if(bMove && GetCopies() == 1 && iDestDrvNumber != -1 && iDestDrvNumber == spFileInfo->GetDriveNumber() &&
				!CFileInfo::Exist(spFileInfo->GetDestinationPath(GetDestPath().GetPath(), 0, ((int)bForceDirectories) << 1)) )
			{
				// if moving within one partition boundary set the file size to 0 so the overall size will
				// be ok
				spFileInfo->SetLength64(0);
			}
			else
				GetClipboardData(i)->SetMove(false);	// no MoveFile

			FilesAdd(spFileInfo);		// file - add

			// log
			fmt.SetFormat(_T("Added file %path"));
			fmt.SetParam(_t("%path"), spFileInfo->GetFullFilePath());
			m_log.logi(fmt);
		}
	}

	// calc size of all files
	CalcAllSize();

	// update *m_pnTasksAll;
	m_rtGlobalStats.IncreaseGlobalTasksSize(GetAllSize());

	// change state to ST_COPYING - finished searching for files
	SetStatus(ST_COPYING, ST_STEP_MASK);

	// save task status
	Store(true);
	Store(false);

	// log
	m_log.logi(_T("Searching for files finished"));
}

// delete files - after copying
void CTask::DeleteFiles()
{
	// log
	m_log.logi(_T("Deleting files (DeleteFiles)..."));

	chcore::IFeedbackHandler* piFeedbackHandler = GetFeedbackHandler();
	BOOST_ASSERT(piFeedbackHandler);

	// current processed path
	BOOL bSuccess;
	CFileInfoPtr spFileInfo;
	ictranslate::CFormat fmt;

	// index points to 0 or next item to process
	size_t stIndex = GetCurrentIndex();
	while(stIndex < FilesGetSize())
	{
		// set index in pTask to currently deleted element
		SetCurrentIndex(stIndex);

		// check for kill flag
		if(GetKillFlag())
		{
			// log
			m_log.logi(_T("Kill request while deleting files (Delete Files)"));
			throw new CProcessingException(E_KILL_REQUEST);
		}

		// current processed element
		spFileInfo = FilesGetAt(FilesGetSize() - stIndex - 1);
		if(!(spFileInfo->GetFlags() & FIF_PROCESSED))
		{
			++stIndex;
			continue;
		}

		// delete data
		if(spFileInfo->IsDirectory())
		{
			if(!GetConfig().get_bool(PP_CMPROTECTROFILES))
				SetFileAttributes(spFileInfo->GetFullFilePath(), FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_DIRECTORY);
			bSuccess=RemoveDirectory(spFileInfo->GetFullFilePath());
		}
		else
		{
			// set files attributes to normal - it'd slow processing a bit, but it's better.
			if(!GetConfig().get_bool(PP_CMPROTECTROFILES))
				SetFileAttributes(spFileInfo->GetFullFilePath(), FILE_ATTRIBUTE_NORMAL);
			bSuccess=DeleteFile(spFileInfo->GetFullFilePath());
		}

		// operation failed
		DWORD dwLastError=GetLastError();
		if(!bSuccess && dwLastError != ERROR_PATH_NOT_FOUND && dwLastError != ERROR_FILE_NOT_FOUND)
		{
			// log
			fmt.SetFormat(_T("Error #%errno while deleting file/folder %path"));
			fmt.SetParam(_t("%errno"), dwLastError);
			fmt.SetParam(_t("%path"), spFileInfo->GetFullFilePath());
			m_log.loge(fmt);

			CString strFile = spFileInfo->GetFullFilePath();
			FEEDBACK_FILEERROR ferr = { (PCTSTR)strFile, NULL, eDeleteError, dwLastError };
			CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileError, &ferr);
			switch(frResult)
			{
			case CFeedbackHandler::eResult_Cancel:
				m_log.logi(_T("Cancel request while deleting file."));
				throw new CProcessingException(E_CANCEL);
				break;
			case CFeedbackHandler::eResult_Retry:
				continue;	// no stIndex bump, since we are trying again
				break;
			case CFeedbackHandler::eResult_Pause:
				throw new CProcessingException(E_PAUSE);
				break;
			case CFeedbackHandler::eResult_Skip:
				break;		// just do nothing
			default:
				BOOST_ASSERT(FALSE);		// unknown result
				throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
			}
		}

		++stIndex;
	}//while

	// change status to finished
	SetStatus(ST_FINISHED, ST_STEP_MASK);

	// add 1 to current index - looks better
	IncreaseCurrentIndex();

	// log
	m_log.logi(_T("Deleting files finished"));
}

void CTask::CustomCopyFile(CUSTOM_COPY_PARAMS* pData)
{
	TAutoFileHandle hSrc = INVALID_HANDLE_VALUE,
					hDst = INVALID_HANDLE_VALUE;
	ictranslate::CFormat fmt;
	bool bRetry = false;

	try
	{
		// do we copy rest or recopy ?
		bool bCopyRest = false;

		// Data regarding dest file
		CFileInfoPtr spDestFileInfo(boost::make_shared<CFileInfo>());
		bool bExist = spDestFileInfo->Create(pData->strDstFile, std::numeric_limits<size_t>::max());

		chcore::IFeedbackHandler* piFeedbackHandler = GetFeedbackHandler();
		BOOST_ASSERT(piFeedbackHandler);

		SetLastProcessedIndex(std::numeric_limits<size_t>::max());

		// if dest file size >0 - we can do something more than usual
		if(bExist)
		{
			// src and dst files are the same
			FEEDBACK_ALREADYEXISTS feedStruct = { pData->spSrcFile, spDestFileInfo };
			CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileAlreadyExists, &feedStruct);
			// check for dialog result
			switch(frResult)
			{
			case CFeedbackHandler::eResult_Overwrite:
				{
					bCopyRest = false;
					break;
				}
			case CFeedbackHandler::eResult_CopyRest:
				{
					bCopyRest = true;
					break;
				}
			case CFeedbackHandler::eResult_Skip:
				{
					IncreaseProcessedSize(pData->spSrcFile->GetLength64());
					m_rtGlobalStats.IncreaseGlobalTasksPosition(pData->spSrcFile->GetLength64());
					pData->bProcessed = false;
					return;
				}
			case CFeedbackHandler::eResult_Cancel:
				{
					// log
					if(GetConfig().get_bool(PP_CMCREATELOG))
					{
						fmt.SetFormat(_T("Cancel request while checking result of dialog before opening source file %path (CustomCopyFile)"));
						fmt.SetParam(_t("%path"), pData->spSrcFile->GetFullFilePath());
						m_log.logi(fmt);
					}
					throw new CProcessingException(E_CANCEL);
					break;
				}
			case CFeedbackHandler::eResult_Pause:
				{
					throw new CProcessingException(E_PAUSE);
					break;
				}
			default:
				{
					BOOST_ASSERT(FALSE);		// unknown result
					throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
					break;
				}
			}
		}// bExist

		// change attributes of a dest file
		if(!GetConfig().get_bool(PP_CMPROTECTROFILES))
			SetFileAttributes(pData->strDstFile, FILE_ATTRIBUTE_NORMAL);

		// first or second pass ? only for FFNB
		bool bFirstPass = true;

		// check size of src file to know whether use flag FILE_FLAG_NOBUFFERING
l_start:
		bool bNoBuffer=(bFirstPass && GetConfig().get_bool(PP_BFUSENOBUFFERING) && pData->spSrcFile->GetLength64() >= (unsigned long long)GetConfig().get_signed_num(PP_BFBOUNDARYLIMIT));

		// refresh data about file
		if(!bFirstPass)
			bExist = spDestFileInfo->Create(pData->strDstFile, std::numeric_limits<size_t>::max());

		// open src
		do
		{
			bRetry = false;

			hSrc = CreateFile(pData->spSrcFile->GetFullFilePath(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | (bNoBuffer ? FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH : 0), NULL);
			if(hSrc == INVALID_HANDLE_VALUE)
			{
				DWORD dwLastError=GetLastError();
				CString strFile = pData->spSrcFile->GetFullFilePath();
				FEEDBACK_FILEERROR feedStruct = { (PCTSTR)strFile, NULL, eCreateError, dwLastError };
				CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileError, &feedStruct);

				switch (frResult)
				{
				case CFeedbackHandler::eResult_Skip:
					IncreaseProcessedSize(pData->spSrcFile->GetLength64());
					m_rtGlobalStats.IncreaseGlobalTasksPosition(pData->spSrcFile->GetLength64());
					pData->bProcessed = false;
					return;
					break;
				case CFeedbackHandler::eResult_Cancel:
					// log
					fmt.SetFormat(_T("Cancel request [error %errno] while opening source file %path (CustomCopyFile)"));
					fmt.SetParam(_t("%errno"), dwLastError);
					fmt.SetParam(_t("%path"), pData->spSrcFile->GetFullFilePath());
					m_log.loge(fmt);
					throw new CProcessingException(E_CANCEL);
					break;
				case CFeedbackHandler::eResult_Pause:
					throw new CProcessingException(E_PAUSE);
					break;
				case CFeedbackHandler::eResult_Retry:
					// log
					fmt.SetFormat(_T("Retrying [error %errno] to open source file %path (CustomCopyFile)"));
					fmt.SetParam(_t("%errno"), dwLastError);
					fmt.SetParam(_t("%path"), pData->spSrcFile->GetFullFilePath());
					m_log.loge(fmt);
					bRetry = true;
					break;
				default:
					{
						BOOST_ASSERT(FALSE);		// unknown result
						throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
						break;
					}
				}
			}
		}
		while(bRetry);

		// open dest
		do 
		{
			bRetry = false;

			hDst = CreateFile(pData->strDstFile, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN | (bNoBuffer ? FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH : 0), NULL);
			if(hDst == INVALID_HANDLE_VALUE)
			{
				DWORD dwLastError=GetLastError();
				CString strFile = pData->strDstFile;

				FEEDBACK_FILEERROR feedStruct = { (PCTSTR)strFile, NULL, eCreateError, dwLastError };
				CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileError, &feedStruct);
				switch (frResult)
				{
				case CFeedbackHandler::eResult_Retry:
					// change attributes
					if(!GetConfig().get_bool(PP_CMPROTECTROFILES))
						SetFileAttributes(pData->strDstFile, FILE_ATTRIBUTE_NORMAL);

					// log
					fmt.SetFormat(_T("Retrying [error %errno] to open destination file %path (CustomCopyFile)"));
					fmt.SetParam(_t("%errno"), dwLastError);
					fmt.SetParam(_t("%path"), pData->strDstFile);
					m_log.loge(fmt);
					bRetry = true;
					break;
				case CFeedbackHandler::eResult_Cancel:
					// log
					fmt.SetFormat(_T("Cancel request [error %errno] while opening destination file %path (CustomCopyFile)"));
					fmt.SetParam(_t("%errno"), dwLastError);
					fmt.SetParam(_t("%path"), pData->strDstFile);
					m_log.loge(fmt);
					throw new CProcessingException(E_CANCEL);
					break;
				case CFeedbackHandler::eResult_Skip:
					IncreaseProcessedSize(pData->spSrcFile->GetLength64());
					m_rtGlobalStats.IncreaseGlobalTasksPosition(pData->spSrcFile->GetLength64());
					pData->bProcessed = false;
					return;
					break;
				case CFeedbackHandler::eResult_Pause:
					throw new CProcessingException(E_PAUSE);
					break;
				default:
					{
						BOOST_ASSERT(FALSE);		// unknown result
						throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
						break;
					}
				}
			}
		}
		while(bRetry);

		// seeking
		DWORD dwLastError = 0;
		if(!pData->bOnlyCreate)
		{
			if(bCopyRest)	// if copy rest
			{
				if(!bFirstPass || (bExist && spDestFileInfo->GetLength64() > 0))
				{
					// try to move file pointers to the end
					ULONGLONG ullMove = (bNoBuffer ? ROUNDDOWN(spDestFileInfo->GetLength64(), MAXSECTORSIZE) : spDestFileInfo->GetLength64());
					bool bRetry = true;
					while(bRetry)
					{
						if(SetFilePointer64(hSrc, ullMove, FILE_BEGIN) == -1 || SetFilePointer64(hDst, ullMove, FILE_BEGIN) == -1)
						{
							dwLastError = GetLastError();
							// log
							fmt.SetFormat(_T("Error %errno while moving file pointers of %srcpath and %dstpath to %pos"));
							fmt.SetParam(_t("%errno"), dwLastError);
							fmt.SetParam(_t("%srcpath"), pData->spSrcFile->GetFullFilePath());
							fmt.SetParam(_t("%dstpath"), pData->strDstFile);
							fmt.SetParam(_t("%pos"), ullMove);
							m_log.loge(fmt);

							CString strSrcFile = pData->spSrcFile->GetFullFilePath();
							CString strDstFile = pData->strDstFile;
							FEEDBACK_FILEERROR ferr = { (PCTSTR)strSrcFile, (PCTSTR)strDstFile, eSeekError, dwLastError };
							CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileError, &ferr);
							switch(frResult)
							{
							case CFeedbackHandler::eResult_Cancel:
								throw new CProcessingException(E_CANCEL);
								break;
							case CFeedbackHandler::eResult_Retry:
								continue;
								break;
							case CFeedbackHandler::eResult_Pause:
								throw new CProcessingException(E_PAUSE);
								break;
							case CFeedbackHandler::eResult_Skip:
								bRetry = false;
								IncreaseProcessedSize(pData->spSrcFile->GetLength64());
								m_rtGlobalStats.IncreaseGlobalTasksPosition(pData->spSrcFile->GetLength64());
								pData->bProcessed = false;
								return;
							default:
								BOOST_ASSERT(FALSE);		// unknown result
								throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
							}
						}
						else
						{
							bRetry = false;
							// file pointers moved - so we have skipped some work - update positions
							if(bFirstPass)
							{
								IncreaseProcessedSize(ullMove);
								m_rtGlobalStats.IncreaseGlobalTasksPosition(ullMove);
							}
						}
					}
				}
			}
			else
			{
				bool bRetry = true;
				while(bRetry && !SetEndOfFile(hDst))
				{
					// log
					dwLastError=GetLastError();
					fmt.SetFormat(_T("Error %errno while setting size of file %path to 0"));
					fmt.SetParam(_t("%errno"), dwLastError);
					fmt.SetParam(_t("%path"), pData->strDstFile);
					m_log.loge(fmt);

					FEEDBACK_FILEERROR ferr = { (PCTSTR)pData->strDstFile, NULL, eResizeError, dwLastError };
					CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileError, &ferr);
					switch(frResult)
					{
					case CFeedbackHandler::eResult_Cancel:
						throw new CProcessingException(E_CANCEL);
						break;
					case CFeedbackHandler::eResult_Retry:
						continue;
						break;
					case CFeedbackHandler::eResult_Pause:
						throw new CProcessingException(E_PAUSE);
						break;
					case CFeedbackHandler::eResult_Skip:
						bRetry = false;
						break;		// just do nothing
					default:
						BOOST_ASSERT(FALSE);		// unknown result
						throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
					}
				}
			}

			// copying
			unsigned long ulToRead = 0, ulRead = 0, ulWritten = 0;
			int iBufferIndex = 0;
			do
			{
				// kill flag checks
				if(GetKillFlag())
				{
					// log
					fmt.SetFormat(_T("Kill request while main copying file %srcpath -> %dstpath"));
					fmt.SetParam(_t("%srcpath"), pData->spSrcFile->GetFullFilePath());
					fmt.SetParam(_t("%dstpath"), pData->strDstFile);
					m_log.logi(fmt);
					throw new CProcessingException(E_KILL_REQUEST);
				}

				// recreate buffer if needed
				if(!(*pData->dbBuffer.GetSizes() == *GetBufferSizes()))
				{
					// log
					const BUFFERSIZES *pbs1=pData->dbBuffer.GetSizes(), *pbs2=GetBufferSizes();

					fmt.SetFormat(_T("Changing buffer size from [Def:%defsize, One:%onesize, Two:%twosize, CD:%cdsize, LAN:%lansize] to [Def:%defsize2, One:%onesize2, Two:%twosize2, CD:%cdsize2, LAN:%lansize2] wile copying %srcfile -> %dstfile (CustomCopyFile)"));

					fmt.SetParam(_t("%defsize"), pbs1->m_uiDefaultSize);
					fmt.SetParam(_t("%onesize"), pbs1->m_uiOneDiskSize);
					fmt.SetParam(_t("%twosize"), pbs1->m_uiTwoDisksSize);
					fmt.SetParam(_t("%cdsize"), pbs1->m_uiCDSize);
					fmt.SetParam(_t("%lansize"), pbs1->m_uiLANSize);
					fmt.SetParam(_t("%defsize2"), pbs2->m_uiDefaultSize);
					fmt.SetParam(_t("%onesize2"), pbs2->m_uiOneDiskSize);
					fmt.SetParam(_t("%twosize2"), pbs2->m_uiTwoDisksSize);
					fmt.SetParam(_t("%cdsize2"), pbs2->m_uiCDSize);
					fmt.SetParam(_t("%lansize2"), pbs2->m_uiLANSize);
					fmt.SetParam(_t("%srcfile"), pData->spSrcFile->GetFullFilePath());
					fmt.SetParam(_t("%dstfile"), pData->strDstFile);

					m_log.logi(fmt);
					SetBufferSizes(pData->dbBuffer.Create(GetBufferSizes()));
				}

				// establish count of data to read
				iBufferIndex=GetBufferSizes()->m_bOnlyDefault ? 0 : pData->spSrcFile->GetBufferIndex();
				ulToRead=bNoBuffer ? ROUNDUP(pData->dbBuffer.GetSizes()->m_auiSizes[iBufferIndex], MAXSECTORSIZE) : pData->dbBuffer.GetSizes()->m_auiSizes[iBufferIndex];

				// read
				bool bRetry = true;
				while(bRetry && !ReadFile(hSrc, pData->dbBuffer, ulToRead, &ulRead, NULL))
				{
					// log
					dwLastError=GetLastError();
					fmt.SetFormat(_T("Error %errno while trying to read %count bytes from source file %path (CustomCopyFile)"));
					fmt.SetParam(_t("%errno"), dwLastError);
					fmt.SetParam(_t("%count"), ulToRead);
					fmt.SetParam(_t("%path"), pData->spSrcFile->GetFullFilePath());
					m_log.loge(fmt);

					CString strFile = pData->spSrcFile->GetFullFilePath();
					FEEDBACK_FILEERROR ferr = { (PCTSTR)strFile, NULL, eReadError, dwLastError };
					CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileError, &ferr);
					switch(frResult)
					{
					case CFeedbackHandler::eResult_Cancel:
						throw new CProcessingException(E_CANCEL);
						break;
					case CFeedbackHandler::eResult_Retry:
						continue;
						break;
					case CFeedbackHandler::eResult_Pause:
						throw new CProcessingException(E_PAUSE);
						break;
					case CFeedbackHandler::eResult_Skip:
						bRetry = false;
						// TODO: correct the skip length handling
						IncreaseProcessedSize(pData->spSrcFile->GetLength64());
						m_rtGlobalStats.IncreaseGlobalTasksPosition(pData->spSrcFile->GetLength64());
						pData->bProcessed = false;
						return;
					default:
						BOOST_ASSERT(FALSE);		// unknown result
						throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
					}
				}

				// change count of stored data
				if(bNoBuffer && (ROUNDUP(ulRead, MAXSECTORSIZE)) != ulRead)
				{
					// we need to copy rest so do the second pass
					// close files
					hSrc.Close();
					hDst.Close();

					// second pass
					bFirstPass=false;
					bCopyRest=true;		// nedd to copy rest

					goto l_start;
				}

				// write
				bRetry = true;
				while(bRetry && !WriteFile(hDst, pData->dbBuffer, ulRead, &ulWritten, NULL) || ulWritten != ulRead)
				{
					// log
					dwLastError=GetLastError();
					fmt.SetFormat(_T("Error %errno while trying to write %count bytes to destination file %path (CustomCopyFile)"));
					fmt.SetParam(_t("%errno"), dwLastError);
					fmt.SetParam(_t("%count"), ulRead);
					fmt.SetParam(_t("%path"), pData->strDstFile);
					m_log.loge(fmt);

					CString strFile = pData->strDstFile;
					FEEDBACK_FILEERROR ferr = { (PCTSTR)strFile, NULL, eWriteError, dwLastError };
					CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileError, &ferr);
					switch(frResult)
					{
					case CFeedbackHandler::eResult_Cancel:
						throw new CProcessingException(E_CANCEL);
						break;
					case CFeedbackHandler::eResult_Retry:
						continue;
						break;
					case CFeedbackHandler::eResult_Pause:
						throw new CProcessingException(E_PAUSE);
						break;
					case CFeedbackHandler::eResult_Skip:
						bRetry = false;
						// TODO: correct the skip length handling
						IncreaseProcessedSize(pData->spSrcFile->GetLength64());
						m_rtGlobalStats.IncreaseGlobalTasksPosition(pData->spSrcFile->GetLength64());
						pData->bProcessed = false;
						return;
					default:
						BOOST_ASSERT(FALSE);		// unknown result
						throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
					}
				}

				// increase count of processed data
				IncreaseProcessedSize(ulRead);
				m_rtGlobalStats.IncreaseGlobalTasksPosition(ulRead);
				//				TRACE("Read: %d, Written: %d\n", rd, ulWritten);
			}
			while(ulRead != 0);
		}
		else
		{
			// we don't copy contents, but need to increase processed size
			IncreaseProcessedSize(pData->spSrcFile->GetLength64());
			m_rtGlobalStats.IncreaseGlobalTasksPosition(pData->spSrcFile->GetLength64());
		}

		// close files
		hSrc.Close();
		hDst.Close();

		pData->bProcessed = true;
	}
	catch(...)
	{
		// close handles
		hSrc.Close();
		hDst.Close();

		throw;
	}
}

// function processes files/folders
void CTask::ProcessFiles()
{
	chcore::IFeedbackHandler* piFeedbackHandler = GetFeedbackHandler();
	BOOST_ASSERT(piFeedbackHandler);

	// log
	m_log.logi(_T("Processing files/folders (ProcessFiles)"));

	// count how much has been done (updates also a member in CTaskArray)
	CalcProcessedSize();

	// create a buffer of size m_nBufferSize
	CUSTOM_COPY_PARAMS ccp;
	ccp.bProcessed = false;
	ccp.bOnlyCreate=(GetStatus(ST_SPECIAL_MASK) & ST_IGNORE_CONTENT) != 0;
	ccp.dbBuffer.Create(GetBufferSizes());

	// helpers
	DWORD dwLastError = 0;

	// begin at index which wasn't processed previously
	size_t stSize = FilesGetSize();
	int iCopiesCount = GetCopies();
	bool bIgnoreFolders = (GetStatus(ST_SPECIAL_MASK) & ST_IGNORE_DIRS) != 0;
	bool bForceDirectories = (GetStatus(ST_SPECIAL_MASK) & ST_FORCE_DIRS) != 0;
	const CDestPath& dpDestPath = GetDestPath();

	// log
	const BUFFERSIZES* pbs = ccp.dbBuffer.GetSizes();

	ictranslate::CFormat fmt;
	fmt.SetFormat(_T("Processing files/folders (ProcessFiles):\r\n\tOnlyCreate: %create\r\n\tBufferSize: [Def:%defsize, One:%onesize, Two:%twosize, CD:%cdsize, LAN:%lansize]\r\n\tFiles/folders count: %filecount\r\n\tCopies count: %copycount\r\n\tIgnore Folders: %ignorefolders\r\n\tDest path: %dstpath\r\n\tCurrent pass (0-based): %currpass\r\n\tCurrent index (0-based): %currindex"));
	fmt.SetParam(_t("%create"), ccp.bOnlyCreate);
	fmt.SetParam(_t("%defsize"), pbs->m_uiDefaultSize);
	fmt.SetParam(_t("%onesize"), pbs->m_uiOneDiskSize);
	fmt.SetParam(_t("%twosize"), pbs->m_uiTwoDisksSize);
	fmt.SetParam(_t("%cdsize"), pbs->m_uiCDSize);
	fmt.SetParam(_t("%lansize"), pbs->m_uiLANSize);
	fmt.SetParam(_t("%filecount"), stSize);
	fmt.SetParam(_t("%copycount"), iCopiesCount);
	fmt.SetParam(_t("%ignorefolders"), bIgnoreFolders);
	fmt.SetParam(_t("%dstpath"), dpDestPath.GetPath());
	fmt.SetParam(_t("%currpass"), GetCurrentCopy());
	fmt.SetParam(_t("%currindex"), GetCurrentIndex());

	m_log.logi(fmt);

	for (unsigned char j=GetCurrentCopy();j<iCopiesCount;j++)
	{
		SetCurrentCopy(j);
		for (size_t i = GetCurrentIndex(); i < stSize; i++)
		{
			// should we kill ?
			if(GetKillFlag())
			{
				// log
				m_log.logi(_T("Kill request while processing file in ProcessFiles"));
				throw new CProcessingException(E_KILL_REQUEST);
			}

			// update m_stCurrentIndex, getting current CFileInfo
			SetCurrentIndex(i);
			CFileInfoPtr spFileInfo = FilesGetAtCurrentIndex();

			// set dest path with filename
			ccp.strDstFile = spFileInfo->GetDestinationPath(dpDestPath.GetPath(), j, ((int)bForceDirectories) << 1 | (int)bIgnoreFolders);

			// are the files/folders lie on the same partition ?
			bool bMove=GetStatus(ST_OPERATION_MASK) == ST_MOVE;
			if(bMove && dpDestPath.GetDriveNumber() != -1 && dpDestPath.GetDriveNumber() == spFileInfo->GetDriveNumber() && iCopiesCount == 1 && spFileInfo->GetMove())
			{
				bool bRetry = true;
				if(bRetry && !MoveFile(spFileInfo->GetFullFilePath(), ccp.strDstFile))
				{
					dwLastError=GetLastError();
					//log
					fmt.SetFormat(_T("Error %errno while calling MoveFile %srcpath -> %dstpath (ProcessFiles)"));
					fmt.SetParam(_t("%errno"), dwLastError);
					fmt.SetParam(_t("%srcpath"), spFileInfo->GetFullFilePath());
					fmt.SetParam(_t("%dstpath"), ccp.strDstFile);
					m_log.loge(fmt);

					CString strSrcFile = spFileInfo->GetFullFilePath();
					CString strDstFile = ccp.strDstFile;
					FEEDBACK_FILEERROR ferr = { (PCTSTR)strSrcFile, (PCTSTR)strDstFile, eFastMoveError, dwLastError };
					CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileError, &ferr);
					switch(frResult)
					{
					case CFeedbackHandler::eResult_Cancel:
						throw new CProcessingException(E_CANCEL);
						break;
					case CFeedbackHandler::eResult_Retry:
						continue;
						break;
					case CFeedbackHandler::eResult_Pause:
						throw new CProcessingException(E_PAUSE);
						break;
					case CFeedbackHandler::eResult_Skip:
						bRetry = false;
						break;		// just do nothing
					default:
						BOOST_ASSERT(FALSE);		// unknown result
						throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
					}
				}
				else
					spFileInfo->SetFlags(FIF_PROCESSED, FIF_PROCESSED);
			}
			else
			{
				// if folder - create it
				if(spFileInfo->IsDirectory())
				{
					bool bRetry = true;
					if(bRetry && !CreateDirectory(ccp.strDstFile, NULL) && (dwLastError=GetLastError()) != ERROR_ALREADY_EXISTS )
					{
						// log
						fmt.SetFormat(_T("Error %errno while calling CreateDirectory %path (ProcessFiles)"));
						fmt.SetParam(_t("%errno"), dwLastError);
						fmt.SetParam(_t("%path"), ccp.strDstFile);
						m_log.loge(fmt);

						CString strFile = ccp.strDstFile;
						FEEDBACK_FILEERROR ferr = { (PCTSTR)strFile, NULL, eCreateError, dwLastError };
						CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_FileError, &ferr);
						switch(frResult)
						{
						case CFeedbackHandler::eResult_Cancel:
							throw new CProcessingException(E_CANCEL);
							break;
						case CFeedbackHandler::eResult_Retry:
							continue;
							break;
						case CFeedbackHandler::eResult_Pause:
							throw new CProcessingException(E_PAUSE);
							break;
						case CFeedbackHandler::eResult_Skip:
							bRetry = false;
							break;		// just do nothing
						default:
							BOOST_ASSERT(FALSE);		// unknown result
							throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
						}
					}

					IncreaseProcessedSize(spFileInfo->GetLength64());
					m_rtGlobalStats.IncreaseGlobalTasksPosition(spFileInfo->GetLength64());
					spFileInfo->SetFlags(FIF_PROCESSED, FIF_PROCESSED);
				}
				else
				{
					// start copying/moving file
					ccp.spSrcFile = spFileInfo;
					ccp.bProcessed = false;

					// kopiuj dane
					CustomCopyFile(&ccp);
					spFileInfo->SetFlags(ccp.bProcessed ? FIF_PROCESSED : 0, FIF_PROCESSED);

					// if moving - delete file (only if config flag is set)
					if(bMove && spFileInfo->GetFlags() & FIF_PROCESSED && !GetConfig().get_bool(PP_CMDELETEAFTERFINISHED) && j == iCopiesCount-1)
					{
						if(!GetConfig().get_bool(PP_CMPROTECTROFILES))
							SetFileAttributes(spFileInfo->GetFullFilePath(), FILE_ATTRIBUTE_NORMAL);
						DeleteFile(spFileInfo->GetFullFilePath());	// there will be another try later, so I don't check
						// if succeeded
					}
				}

				// set a time
				if(GetConfig().get_bool(PP_CMSETDESTDATE))
					SetFileDirectoryTime(ccp.strDstFile, spFileInfo); // no error checking (but most probably it should be checked)

				// attributes
				if(GetConfig().get_bool(PP_CMSETDESTATTRIBUTES))
					SetFileAttributes(ccp.strDstFile, spFileInfo->GetAttributes());	// as above
			}
		}

		// current copy finished
		SetCurrentIndex(0);
	}

	// delete buffer - it's not needed
	ccp.dbBuffer.Delete();

	// change status
	if(GetStatus(ST_OPERATION_MASK) == ST_MOVE)
	{
		SetStatus(ST_DELETING, ST_STEP_MASK);
		// set the index to 0 before deleting
		SetCurrentIndex(0);
	}
	else
	{
		SetStatus(ST_FINISHED, ST_STEP_MASK);

		// to look better - increase current index by 1
		SetCurrentIndex(stSize);
	}
	// log
	m_log.logi(_T("Finished processing in ProcessFiles"));
}

void CTask::CheckForWaitState()
{
	// limiting operation count
	SetStatus(ST_WAITING, ST_WAITING_MASK);
	bool bContinue = false;
	while(!bContinue)
	{
		if(CanBegin())
		{
			SetStatus(0, ST_WAITING);
			bContinue=true;

			m_log.logi(_T("Finished waiting for begin permission"));

			//			return; // skips sleep and kill flag checking
		}

		Sleep(50);	// not to make it too hard for processor

		if(GetKillFlag())
		{
			// log
			m_log.logi(_T("Kill request while waiting for begin permission (wait state)"));
			throw new CProcessingException(E_KILL_REQUEST);
		}
	}
}

UINT CTask::ThrdProc(LPVOID pParam)
{
	TRACE("\n\nENTERING ThrdProc (new task started)...\n");

	CTask* pTask = static_cast<CTask*>(pParam);
	
	chcore::IFeedbackHandler* piFeedbackHandler = pTask->GetFeedbackHandler();

	tstring_t strPath = pTask->GetTaskPath();
	strPath += pTask->GetUniqueName()+_T(".log");

	pTask->m_log.init(strPath.c_str(), 262144, icpf::log_file::level_debug, false, false);

	// set thread boost
	HANDLE hThread=GetCurrentThread();
	::SetThreadPriorityBoost(hThread, GetConfig().get_bool(PP_CMDISABLEPRIORITYBOOST));

	CTime tm=CTime::GetCurrentTime();

	ictranslate::CFormat fmt;
	fmt.SetFormat(_T("\r\n# COPYING THREAD STARTED #\r\nBegan processing data (dd:mm:yyyy) %day.%month.%year at %hour:%minute.%second"));
	fmt.SetParam(_t("%year"), tm.GetYear());
	fmt.SetParam(_t("%month"), tm.GetMonth());
	fmt.SetParam(_t("%day"), tm.GetDay());
	fmt.SetParam(_t("%hour"), tm.GetHour());
	fmt.SetParam(_t("%minute"), tm.GetMinute());
	fmt.SetParam(_t("%second"), tm.GetSecond());
	pTask->m_log.logi(fmt);

	try
	{
		// to make the value stable
		bool bReadTasksSize = GetConfig().get_bool(PP_CMREADSIZEBEFOREBLOCKING);

		if(!bReadTasksSize)
			pTask->CheckForWaitState();	// operation limiting

		// set what's needed
		pTask->m_lLastTime=(long)time(NULL);	// last time (start counting)

		// search for files if needed
		if((pTask->GetStatus(ST_STEP_MASK) == ST_NULL_STATUS
			|| pTask->GetStatus(ST_STEP_MASK) == ST_SEARCHING))
		{
			// get rid of info about processed sizes
			pTask->m_rtGlobalStats.DecreaseGlobalProgressData(pTask->GetProcessedSize(), pTask->GetAllSize());
			pTask->SetProcessedSize(0);
			pTask->SetAllSize(0);

			// start searching
			pTask->RecurseDirectories();
		}

		// check for free space
		ull_t ullNeededSize = 0, ullAvailableSize = 0;
l_showfeedback:
		pTask->m_log.logi(_T("Checking for free space on destination disk..."));

		if(!pTask->GetRequiredFreeSpace(&ullNeededSize, &ullAvailableSize))
		{
			fmt.SetFormat(_T("Not enough free space on disk - needed %needsize bytes for data, available: %availablesize bytes."));
			fmt.SetParam(_t("%needsize"), ullNeededSize);
			fmt.SetParam(_t("%availablesize"), ullAvailableSize);
			pTask->m_log.logw(fmt);

			BOOST_ASSERT(piFeedbackHandler);

			if(pTask->GetClipboardDataSize() > 0)
			{
				CString strSrcPath = pTask->GetClipboardData(0)->GetPath();
				CString strDstPath = pTask->GetDestPath().GetPath();
				FEEDBACK_NOTENOUGHSPACE feedStruct = { ullNeededSize, (PCTSTR)strSrcPath, (PCTSTR)strDstPath };
				CFeedbackHandler::EFeedbackResult frResult = (CFeedbackHandler::EFeedbackResult)piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_NotEnoughSpace, &feedStruct);

				// default
				switch (frResult)
				{
				case CFeedbackHandler::eResult_Cancel:
					{
						pTask->m_log.logi(_T("Cancel request while checking for free space on disk."));
						throw new CProcessingException(E_CANCEL);
						break;
					}
				case CFeedbackHandler::eResult_Retry:
					pTask->m_log.logi(_T("Retrying to read drive's free space..."));
					goto l_showfeedback;
					break;
				case CFeedbackHandler::eResult_Skip:
					pTask->m_log.logi(_T("Ignored warning about not enough place on disk to copy data."));
					break;
				default:
					BOOST_ASSERT(FALSE);		// unknown result
					throw new CProcessingException(E_ERROR, 0, _t("Unknown feedback result type"));
					break;
				}
			}
		}

		if(bReadTasksSize)
		{
			pTask->UpdateTime();
			pTask->m_lLastTime=-1;

			pTask->CheckForWaitState();

			pTask->m_lLastTime=(long)time(NULL);
		}

		// Phase II - copying/moving
		if(pTask->GetStatus(ST_STEP_MASK) == ST_COPYING)
		{
			// decrease processed in ctaskarray - the rest will be done in ProcessFiles
			pTask->m_rtGlobalStats.DecreaseGlobalTasksPosition(pTask->GetProcessedSize());
			pTask->ProcessFiles();
		}

		// deleting data - III phase
		if(pTask->GetStatus(ST_STEP_MASK) == ST_DELETING)
			pTask->DeleteFiles();

		// refresh time
		pTask->UpdateTime();

		// save progress before killed
		pTask->Store(false);

		// we are ending
		pTask->UnregisterTaskAsRunning();

		// play sound
		piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_OperationFinished, NULL);

		tm=CTime::GetCurrentTime();
		fmt.SetFormat(_T("Finished processing data (dd:mm:yyyy) %day.%month.%year at %hour:%minute.%second"));
		fmt.SetParam(_t("%year"), tm.GetYear());
		fmt.SetParam(_t("%month"), tm.GetMonth());
		fmt.SetParam(_t("%day"), tm.GetDay());
		fmt.SetParam(_t("%hour"), tm.GetHour());
		fmt.SetParam(_t("%minute"), tm.GetMinute());
		fmt.SetParam(_t("%second"), tm.GetSecond());
		pTask->m_log.logi(fmt);

		// we have been killed - the last operation
		pTask->CleanupAfterKill();
		pTask->SetKilledFlag();
	}
	catch(CProcessingException* e)
	{
		// refresh time
		pTask->UpdateTime();

		// log
		fmt.SetFormat(_T("Caught exception in ThrdProc [last error: %errno, type: %type]"));
		fmt.SetParam(_t("%errno"), e->m_dwError);
		fmt.SetParam(_t("%type"), e->m_iType);
		pTask->m_log.loge(fmt);

		if(e->m_iType == E_ERROR)
			piFeedbackHandler->RequestFeedback(CFeedbackHandler::eFT_OperationError, NULL);

		// perform some adjustments depending on exception type
		switch(e->m_iType)
		{
		case E_ERROR:
			pTask->SetStatus(ST_ERROR, ST_WORKING_MASK);
			pTask->SetOsErrorCode(e->m_dwError, e->m_strErrorDesc);
			break;
		case E_CANCEL:
			pTask->SetStatus(ST_CANCELLED, ST_STEP_MASK);
			break;
		case E_PAUSE:
			pTask->SetStatus(ST_PAUSED, ST_PAUSED);
			break;
		}

		// change flags and calls cleanup for a task
		switch(pTask->GetStatus(ST_STEP_MASK))
		{
		case ST_NULL_STATUS:
		case ST_SEARCHING:
			// get rid of m_files contents
			pTask->FilesRemoveAll();

			// save state of a task
			pTask->Store(true);
			pTask->Store(false);

			break;
		case ST_COPYING:
		case ST_DELETING:
			pTask->Store(false);
			break;
		}

		if(pTask->GetStatus(ST_WAITING_MASK) & ST_WAITING)
			pTask->SetStatus(0, ST_WAITING);

		pTask->UnregisterTaskAsRunning();
		pTask->SetContinueFlag(false);
		pTask->SetForceFlag(false);
		pTask->SetKilledFlag();
		pTask->CleanupAfterKill();

		delete e;

		return 0xffffffff;	// almost like -1
	}

	TRACE("TASK FINISHED - exiting ThrdProc.\n");
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
// TTasksGlobalStats members

TTasksGlobalStats::TTasksGlobalStats() :
	m_ullGlobalTasksSize(0),
	m_ullGlobalTasksPosition(0),
	m_stRunningTasks(0)
{
}

TTasksGlobalStats::~TTasksGlobalStats()
{
}

void TTasksGlobalStats::IncreaseGlobalTasksSize(unsigned long long ullModify)
{
	m_lock.lock();
	m_ullGlobalTasksSize += ullModify;
	m_lock.unlock();
}

void TTasksGlobalStats::DecreaseGlobalTasksSize(unsigned long long ullModify)
{
	m_lock.lock();
	m_ullGlobalTasksSize -= ullModify;
	m_lock.unlock();
}

unsigned long long TTasksGlobalStats::GetGlobalTasksSize() const
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_ullGlobalTasksSize;
}

void TTasksGlobalStats::IncreaseGlobalTasksPosition(unsigned long long ullModify)
{
	m_lock.lock();
	m_ullGlobalTasksPosition += ullModify;
	m_lock.unlock();
}

void TTasksGlobalStats::DecreaseGlobalTasksPosition(unsigned long long ullModify)
{
	m_lock.lock();
	m_ullGlobalTasksPosition -= ullModify;
	m_lock.unlock();
}

unsigned long long TTasksGlobalStats::GetGlobalTasksPosition() const
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_ullGlobalTasksPosition;
}

void TTasksGlobalStats::IncreaseGlobalProgressData(unsigned long long ullTasksPosition, unsigned long long ullTasksSize)
{
	m_lock.lock();
	m_ullGlobalTasksSize += ullTasksSize;
	m_ullGlobalTasksPosition += ullTasksPosition;
	m_lock.unlock();

}

void TTasksGlobalStats::DecreaseGlobalProgressData(unsigned long long ullTasksPosition, unsigned long long ullTasksSize)
{
	m_lock.lock();
	m_ullGlobalTasksSize += ullTasksSize;
	m_ullGlobalTasksPosition += ullTasksPosition;
	m_lock.unlock();
}

int TTasksGlobalStats::GetProgressPercents() const
{
	unsigned long long llPercent = 0;

	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	
	if(m_ullGlobalTasksSize != 0)
		llPercent = m_ullGlobalTasksPosition * 100 / m_ullGlobalTasksSize;

	return boost::numeric_cast<int>(llPercent);
}

void TTasksGlobalStats::IncreaseRunningTasks()
{
	m_lock.lock();
	++m_stRunningTasks;
	m_lock.unlock();
}

void TTasksGlobalStats::DecreaseRunningTasks()
{
	m_lock.lock();
	--m_stRunningTasks;
	m_lock.unlock();
}

size_t TTasksGlobalStats::GetRunningTasksCount() const
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_stRunningTasks;
}

////////////////////////////////////////////////////////////////////////////////
// CTaskArray members
CTaskArray::CTaskArray() :
	m_piFeedbackFactory(NULL),
	m_stNextSessionUniqueID(0)
{
}

CTaskArray::~CTaskArray()
{
	// NOTE: do not delete the feedback factory, since we are not responsible for releasing it
}

void CTaskArray::Create(chcore::IFeedbackHandlerFactory* piFeedbackHandlerFactory)
{
	BOOST_ASSERT(piFeedbackHandlerFactory);
	
	m_piFeedbackFactory = piFeedbackHandlerFactory;
}

CTaskPtr CTaskArray::CreateTask()
{
	BOOST_ASSERT(m_piFeedbackFactory);
	if(!m_piFeedbackFactory)
		return CTaskPtr();
	
	chcore::IFeedbackHandler* piHandler = m_piFeedbackFactory->Create();
	if(!piHandler)
		return CTaskPtr();
	
	CTaskPtr spTask(new CTask(piHandler, m_stNextSessionUniqueID++, m_globalStats));
	return spTask;
}

size_t CTaskArray::GetSize() const
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	return m_vTasks.size();
}

CTaskPtr CTaskArray::GetAt(size_t nIndex) const
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	
	_ASSERTE(nIndex >= 0 && nIndex < m_vTasks.size());
	if(nIndex >= m_vTasks.size())
		THROW(_t("Invalid argument"), 0, 0, 0);
	
	return m_vTasks.at(nIndex);
}

CTaskPtr CTaskArray::GetTaskBySessionUniqueID(size_t stSessionUniqueID) const
{
	CTaskPtr spFoundTask;
	
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	BOOST_FOREACH(const CTaskPtr& spTask, m_vTasks)
	{
		if(spTask->GetSessionUniqueID() == stSessionUniqueID)
		{
			spFoundTask = spTask;
			break;
		}
	}
	
	return spFoundTask;
}

size_t CTaskArray::Add(const CTaskPtr& spNewTask)
{
	if(!spNewTask)
		THROW(_t("Invalid argument"), 0, 0, 0);
	
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	// here we know load succeeded
	spNewTask->SetTaskPath(m_strTasksDir.c_str());
	
	m_vTasks.push_back(spNewTask);
	
	m_globalStats.IncreaseGlobalProgressData(spNewTask->GetProcessedSize(), spNewTask->GetAllSize());
	
	return m_vTasks.size() - 1;
}

void CTaskArray::RemoveAt(size_t stIndex, size_t stCount)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	
	_ASSERTE(stIndex >= m_vTasks.size() || stIndex + stCount > m_vTasks.size());
	if(stIndex >= m_vTasks.size() || stIndex + stCount > m_vTasks.size())
		THROW(_t("Invalid argument"), 0, 0, 0);
	
	for(std::vector<CTaskPtr>::iterator iterTask = m_vTasks.begin() + stIndex; iterTask != m_vTasks.begin() + stIndex + stCount; ++iterTask)
	{
		CTaskPtr& spTask = *iterTask;
		
		// kill task if needed
		spTask->KillThread();
		
		m_globalStats.DecreaseGlobalProgressData(spTask->GetProcessedSize(), spTask->GetAllSize());
	}

	// remove elements from array
	m_vTasks.erase(m_vTasks.begin() + stIndex, m_vTasks.begin() + stIndex + stCount);
}

void CTaskArray::RemoveAll()
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);

	StopAllTasksNL();

	m_vTasks.clear();
}

void CTaskArray::RemoveAllFinished()
{
	std::vector<CTaskPtr> vTasksToRemove;

	m_lock.lock();
	
	size_t stIndex = m_vTasks.size();
	while(stIndex--)
	{
		CTaskPtr spTask = m_vTasks.at(stIndex);
		
		// delete only when the thread is finished
		if((spTask->GetStatus(ST_STEP_MASK) == ST_FINISHED || spTask->GetStatus(ST_STEP_MASK) == ST_CANCELLED)
			&& spTask->GetKilledFlag())
		{
			m_globalStats.DecreaseGlobalProgressData(spTask->GetProcessedSize(), spTask->GetAllSize());
			
			vTasksToRemove.push_back(spTask);
			m_vTasks.erase(m_vTasks.begin() + stIndex);
		}
	}
	
	m_lock.unlock();

	BOOST_FOREACH(CTaskPtr& spTask, vTasksToRemove)
	{
		// delete associated files
		spTask->DeleteProgress(m_strTasksDir.c_str());
	}
}

void CTaskArray::RemoveFinished(const CTaskPtr& spSelTask)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	
	// this might be optimized by copying tasks to a local table in critical section, and then deleting progress files outside of the critical section
	for(std::vector<CTaskPtr>::iterator iterTask = m_vTasks.begin(); iterTask != m_vTasks.end(); ++iterTask)
	{
		CTaskPtr& spTask = *iterTask;
		
		if(spTask == spSelTask && (spTask->GetStatus(ST_STEP_MASK) == ST_FINISHED || spTask->GetStatus(ST_STEP_MASK) == ST_CANCELLED))
		{
			// kill task if needed
			spTask->KillThread();

			m_globalStats.DecreaseGlobalProgressData(spTask->GetProcessedSize(), spTask->GetAllSize());
			
			// delete associated files
			spTask->DeleteProgress(m_strTasksDir.c_str());
			
			m_vTasks.erase(iterTask);
			
			return;
		}
	}
}

void CTaskArray::StopAllTasks()
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	
	StopAllTasksNL();
}

void CTaskArray::ResumeWaitingTasks(size_t stMaxRunningTasks)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);

	if(stMaxRunningTasks == 0 || m_globalStats.GetRunningTasksCount() < stMaxRunningTasks)
	{
		BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
		{
			// turn on some thread - find something with wait state
			if(spTask->GetStatus(ST_WAITING_MASK) & ST_WAITING && (stMaxRunningTasks == 0 || m_globalStats.GetRunningTasksCount() < stMaxRunningTasks))
			{
				spTask->RegisterTaskAsRunning();
				spTask->SetContinueFlagNL(true);
			}
		}
	}
}

void CTaskArray::SaveData()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
	{
		spTask->Store(true);
	}
}

void CTaskArray::SaveProgress()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
	{
		spTask->Store(false);
	}
}

void CTaskArray::LoadDataProgress()
{
	CFileFind finder;
	CTaskPtr spTask;
	CString strPath;
	
	BOOL bWorking=finder.FindFile(CString(m_strTasksDir.c_str())+_T("*.atd"));
	while(bWorking)
	{
		bWorking = finder.FindNextFile();
		
		// load data
		spTask = CreateTask();
		try
		{
			strPath = finder.GetFilePath();
			
			spTask->Load(strPath, true);
			
			strPath = strPath.Left(strPath.GetLength() - 4);
			strPath += _T(".atp");
			
			spTask->Load(strPath, false);
			
			// add read task to array
			Add(spTask);
		}
		catch(std::exception& e)
		{
			CString strFmt;
			strFmt.Format(_T("Cannot load task data: %s (reason: %S)"), strPath, e.what());
			LOG_ERROR(strFmt);
		}
	}
	finder.Close();
}

void CTaskArray::TasksBeginProcessing()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
	{
		spTask->BeginProcessing();
	}
}

void CTaskArray::TasksPauseProcessing()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
	{
		spTask->PauseProcessing();
	}
}

void CTaskArray::TasksResumeProcessing()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
	{
		spTask->ResumeProcessing();
	}
}

void CTaskArray::TasksRestartProcessing()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
	{
		spTask->RestartProcessing();
	}
}

bool CTaskArray::TasksRetryProcessing(bool bOnlyErrors/*=false*/, UINT uiInterval)
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	bool bChanged=false;
	BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
	{
		if(spTask->RetryProcessing(bOnlyErrors, uiInterval))
			bChanged = true;
	}
	
	return bChanged;
}

void CTaskArray::TasksCancelProcessing()
{
	boost::shared_lock<boost::shared_mutex> lock(m_lock);
	BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
	{
		spTask->CancelProcessing();
	}
}

ull_t CTaskArray::GetPosition()
{
	return m_globalStats.GetGlobalTasksPosition();
}

ull_t CTaskArray::GetRange()
{
	return m_globalStats.GetGlobalTasksSize();
}

int CTaskArray::GetPercent()
{
	return m_globalStats.GetProgressPercents();
}

bool CTaskArray::AreAllFinished()
{
	bool bFlag=true;
	UINT uiStatus;
	
	if(m_globalStats.GetRunningTasksCount() != 0)
		bFlag = false;
	else
	{
      boost::shared_lock<boost::shared_mutex> lock(m_lock);
		BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
		{
			uiStatus = spTask->GetStatus();
			bFlag = ((uiStatus & ST_STEP_MASK) == ST_FINISHED || (uiStatus & ST_STEP_MASK) == ST_CANCELLED
			|| (uiStatus & ST_WORKING_MASK) == ST_PAUSED
			|| ((uiStatus & ST_WORKING_MASK) == ST_ERROR && !GetConfig().get_bool(PP_CMAUTORETRYONERROR)));

         if(!bFlag)
            break;
		}
	}
	
	return bFlag;
}

void CTaskArray::SetTasksDir(const tchar_t* pszPath)
{
	boost::unique_lock<boost::shared_mutex> lock(m_lock);
	m_strTasksDir = pszPath;
}

void CTaskArray::StopAllTasksNL()
{
	// kill all unfinished tasks - send kill request
	BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
	{
		spTask->SetKillFlagNL();
	}
	
	// wait for finishing
	BOOST_FOREACH(CTaskPtr& spTask, m_vTasks)
	{
		while(!spTask->GetKilledFlagNL())
			Sleep(10);
		spTask->CleanupAfterKillNL();
	}
}
