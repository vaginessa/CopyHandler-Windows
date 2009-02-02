/***************************************************************************
*   Copyright (C) 2001-2008 by J�zef Starosczyk                           *
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

////////////////////////////////////////////////////////////////////////////
// CTask members

CTask::CTask(chcore::IFeedbackHandler* piFeedbackHandler, const TASK_CREATE_DATA *pCreateData) :
m_log(),
m_piFeedbackHandler(piFeedbackHandler),
m_files(m_clipboard)
{
	BOOST_ASSERT(piFeedbackHandler);

	m_nCurrentIndex=0;
	m_iLastProcessedIndex=-1;
	m_nStatus=ST_NULL_STATUS;
	m_bsSizes.m_uiDefaultSize=65536;
	m_bsSizes.m_uiOneDiskSize=4194304;
	m_bsSizes.m_uiTwoDisksSize=262144;
	m_bsSizes.m_uiCDSize=262144;
	m_bsSizes.m_uiLANSize=65536;
	m_pThread=NULL;
	m_nPriority=THREAD_PRIORITY_NORMAL;
	m_nProcessed=0;
	m_nAll=0;
	m_pnTasksProcessed=pCreateData->pTasksProcessed;
	m_pnTasksAll=pCreateData->pTasksAll;
	m_bKill=false;
	m_bKilled=true;
	m_pcs=pCreateData->pcs;
	m_pfnTaskProc=pCreateData->pfnTaskProc;
	m_lTimeElapsed=0;
	m_lLastTime=-1;
	m_puiOperationsPending=pCreateData->puiOperationsPending;
	m_bQueued=false;
	m_ucCopies=1;
	m_ucCurrentCopy=0;
	m_uiResumeInterval=0;
	m_plFinished=pCreateData->plFinished;
	m_bForce=false;
	m_bContinue=false;
	m_bSaved=false;

	m_iIdentical=-1;
	m_iDestinationLess=-1;
	m_iDestinationGreater=-1;
	m_iMissingInput=-1;
	m_iOutputError=-1;
	m_iMoveFile=-1;

	TCHAR xx[16];
	_itot((int)time(NULL), xx, 10);
	m_strUniqueName=xx;
}

CTask::~CTask()
{
	KillThread();
	if(m_piFeedbackHandler)
		m_piFeedbackHandler->Delete();
}

// m_clipboard
int	CTask::AddClipboardData(CClipboardEntry* pEntry)
{
	m_cs.Lock();
	int retval=m_clipboard.Add(pEntry);
	m_cs.Unlock();

	return retval;
}

CClipboardEntry* CTask::GetClipboardData(int nIndex)
{
	m_cs.Lock();
	CClipboardEntry* pEntry=m_clipboard.GetAt(nIndex);
	m_cs.Unlock();

	return pEntry;
}

int CTask::GetClipboardDataSize()
{
	m_cs.Lock();
	int rv=m_clipboard.GetSize();
	m_cs.Unlock();

	return rv;
}

int CTask::ReplaceClipboardStrings(CString strOld, CString strNew)
{
	// small chars to make comparing case insensitive
	strOld.MakeLower();

	CString strText;
	int iOffset;
	int iCount=0;
	m_cs.Lock();
	for (int i=0;i<m_clipboard.GetSize();i++)
	{
		CClipboardEntry* pEntry=m_clipboard.GetAt(i);
		strText=pEntry->GetPath();
		strText.MakeLower();
		iOffset=strText.Find(strOld, 0);
		if (iOffset != -1)
		{
			// found
			strText=pEntry->GetPath();
			strText=strText.Left(iOffset)+strNew+strText.Mid(iOffset+strOld.GetLength());
			pEntry->SetPath(strText);
			iCount++;
		}
	}
	m_cs.Unlock();

	return iCount;
}

// m_files
int CTask::FilesAddDir(const CString strDirName, const CFiltersArray* pFilters, int iSrcIndex,
					   const bool bRecurse, const bool bIncludeDirs)
{
	// this uses much of memory, but resolves problem critical section hungs and m_bKill
	CFileInfoArray fa(m_clipboard);

	fa.AddDir(strDirName, pFilters, iSrcIndex, bRecurse, bIncludeDirs, &m_bKill);

	m_cs.Lock();

	m_files.Append(fa);

	m_cs.Unlock();

	return 0;
}

int CTask::FilesAdd(CFileInfo fi)
{
	int rv=-1;
	m_cs.Lock();
	if (fi.IsDirectory() || m_afFilters.Match(fi))
		rv=m_files.Add(fi);
	m_cs.Unlock();

	return rv;
}	

CFileInfo CTask::FilesGetAt(int nIndex)
{
	m_cs.Lock();
	CFileInfo info=m_files.GetAt(nIndex);
	m_cs.Unlock();

	return info;
}

CFileInfo& CTask::FilesGetAtCurrentIndex()
{
	m_cs.Lock();
	CFileInfo& info=m_files.GetAt(m_nCurrentIndex);
	m_cs.Unlock();
	return info;
}

void CTask::FilesRemoveAll()
{
	m_cs.Lock();
	m_files.RemoveAll();
	m_cs.Unlock();
}

int CTask::FilesGetSize()
{
	m_cs.Lock();
	int nSize=m_files.GetSize();
	m_cs.Unlock();

	return nSize;
}

// m_nCurrentIndex
void CTask::IncreaseCurrentIndex()
{
	m_cs.Lock();
	++m_nCurrentIndex;
	m_cs.Unlock();
}

int CTask::GetCurrentIndex()
{
	m_cs.Lock();
	int nIndex=m_nCurrentIndex;
	m_cs.Unlock();

	return nIndex;
}

void CTask::SetCurrentIndex(int nIndex)
{
	m_cs.Lock();
	m_nCurrentIndex=nIndex;
	m_cs.Unlock();
}

// m_strDestPath - adds '\\'
void CTask::SetDestPath(LPCTSTR lpszPath)
{
	m_dpDestPath.SetPath(lpszPath);
}

// guaranteed '\\'
const CDestPath& CTask::GetDestPath()
{
	return m_dpDestPath;
}

int CTask::GetDestDriveNumber()
{
	return m_dpDestPath.GetDriveNumber();
}

// m_nStatus
void CTask::SetStatus(UINT nStatus, UINT nMask)
{
	m_cs.Lock();
	m_nStatus &= ~nMask;
	m_nStatus |= nStatus;
	m_cs.Unlock();
}

UINT CTask::GetStatus(UINT nMask)
{
	m_cs.Lock();
	UINT nStatus=m_nStatus;
	m_cs.Unlock();

	return (nStatus & nMask);
}

// m_nBufferSize
void CTask::SetBufferSizes(const BUFFERSIZES* bsSizes)
{
	m_cs.Lock();
	m_bsSizes=*bsSizes;
	m_bSaved=false;
	m_cs.Unlock();
}

const BUFFERSIZES* CTask::GetBufferSizes()
{
	m_cs.Lock();
	const BUFFERSIZES* pbsSizes=&m_bsSizes;
	m_cs.Unlock();

	return pbsSizes;
}

int CTask::GetCurrentBufferIndex()
{
	int rv=0;
	m_cs.Lock();
	int iSize=m_files.GetSize();
	if (iSize > 0 && m_nCurrentIndex != -1)
		rv=m_bsSizes.m_bOnlyDefault ? 0 : m_files.GetAt((m_nCurrentIndex < iSize) ? m_nCurrentIndex : 0).GetBufferIndex();
	m_cs.Unlock();

	return rv;
}

// m_pThread
// m_nPriority
int  CTask::GetPriority()
{
	m_cs.Lock();
	int nPriority=m_nPriority;
	m_cs.Unlock();
	return nPriority;
}

void CTask::SetPriority(int nPriority)
{
	m_cs.Lock();
	m_nPriority=nPriority;
	m_bSaved=false;
	if (m_pThread != NULL)
	{
		TRACE("Changing thread priority");
		m_pThread->SuspendThread();
		m_pThread->SetThreadPriority(nPriority);
		m_pThread->ResumeThread();
	}
	m_cs.Unlock();
}

// m_nProcessed
void CTask::IncreaseProcessedSize(__int64 nSize)
{
	m_cs.Lock();
	m_nProcessed+=nSize;
	m_cs.Unlock();
}

void CTask::SetProcessedSize(__int64 nSize)
{
	m_cs.Lock();
	m_nProcessed=nSize;
	m_cs.Unlock();
}

__int64 CTask::GetProcessedSize()
{
	m_cs.Lock();
	__int64 nSize=m_nProcessed;
	m_cs.Unlock();

	return nSize;
}

// m_nAll
void CTask::SetAllSize(__int64 nSize)
{
	m_cs.Lock();
	m_nAll=nSize;
	m_cs.Unlock();
}

__int64 CTask::GetAllSize()
{
	m_cs.Lock();
	__int64 nAll=m_nAll;
	m_cs.Unlock();

	return nAll;
}

void CTask::CalcAllSize()
{
	m_cs.Lock();
	m_nAll=0;

	int nSize=m_files.GetSize();
	CFileInfo* pFiles=m_files.GetData();

	for (int i=0;i<nSize;i++)
		m_nAll+=pFiles[i].GetLength64();

	m_nAll*=m_ucCopies;

	m_cs.Unlock();
}

void CTask::CalcProcessedSize()
{
	m_cs.Lock();
	m_nProcessed=0;

	CFileInfo* pFiles=m_files.GetData();
	if(pFiles)
	{
		// count all from previous passes
		if(m_ucCopies)
			m_nProcessed+=m_ucCurrentCopy*(m_nAll/m_ucCopies);
		else
			m_nProcessed+=m_ucCurrentCopy*m_nAll;

		for (int i=0;i<m_nCurrentIndex;i++)
			m_nProcessed+=pFiles[i].GetLength64();
		IncreaseProcessedTasksSize(m_nProcessed);
	}

	m_cs.Unlock();
}

// m_pnTasksProcessed
void CTask::IncreaseProcessedTasksSize(__int64 nSize)
{
	//	m_cs.Lock();
	m_pcs->Lock();
	(*m_pnTasksProcessed)+=nSize;
	m_pcs->Unlock();
	//	m_cs.Unlock();
}

void CTask::DecreaseProcessedTasksSize(__int64 nSize)
{
	//	m_cs.Lock();
	m_pcs->Lock();
	(*m_pnTasksProcessed)-=nSize;
	m_pcs->Unlock();
	//	m_cs.Unlock();
}

// m_pnTasksAll
void CTask::IncreaseAllTasksSize(__int64 nSize)
{
	//	m_cs.Lock();
	m_pcs->Lock();
	(*m_pnTasksAll)+=nSize;
	m_pcs->Unlock();
	//	m_cs.Unlock();
}

void CTask::DecreaseAllTasksSize(__int64 nSize)
{
	//	m_cs.Lock();
	m_pcs->Lock();
	(*m_pnTasksAll)-=nSize;
	m_pcs->Unlock();
	//	m_cs.Unlock();
}

// m_bKill
/*inline*/ void CTask::SetKillFlag(bool bKill)
{
	m_cs.Lock();
	m_bKill=bKill;
	m_cs.Unlock();
}

bool CTask::GetKillFlag()
{
	m_cs.Lock();
	bool bKill=m_bKill;
	m_cs.Unlock();

	return bKill;
}

// m_bKilled
/*inline*/ void CTask::SetKilledFlag(bool bKilled)
{
	m_cs.Lock();
	m_bKilled=bKilled;
	m_cs.Unlock();
}

/*inline*/ bool CTask::GetKilledFlag()
{
	m_cs.Lock();
	bool bKilled=m_bKilled;
	m_cs.Unlock();

	return bKilled;
}

// m_strUniqueName

CString CTask::GetUniqueName()
{
	m_cs.Lock();
	CString name=m_strUniqueName;
	m_cs.Unlock();

	return name;
}

void CTask::Load(icpf::archive& ar, bool bData)
{
	m_cs.Lock();
	try
	{
		if (bData)
		{
			m_clipboard.Serialize(ar, bData);

			m_files.Load(ar, false);
			m_dpDestPath.Serialize(ar);
			ar>>m_strUniqueName;
			m_afFilters.Serialize(ar);
			ar>>m_ucCopies;
		}
		else
		{
			int data;
			unsigned long part;

			ar>>data;
			m_nCurrentIndex=data;
			ar>>data;
			m_nStatus=data;
			ar>>m_lOsError;
			ar>>m_strErrorDesc;
			m_bsSizes.Serialize(ar);
			ar>>m_nPriority;

			ar>>part;
			m_nAll=(static_cast<unsigned __int64>(part) << 32);
			ar>>part;
			m_nAll|=part;
			// czas
			ar>>m_lTimeElapsed;

			ar>>part;
			m_nProcessed=(static_cast<unsigned __int64>(part) << 32);
			ar>>part;
			m_nProcessed|=part;

			ar>>m_ucCurrentCopy;

			m_clipboard.Serialize(ar, bData);
			m_files.Load(ar, true);

			unsigned char ucTmp;
			ar>>ucTmp;
			m_bSaved=ucTmp != 0;
		}
	}
	catch(icpf::exception&)
	{
		m_cs.Unlock();
		throw;
	}
	m_cs.Unlock();
}

void CTask::Store(bool bData)
{
	m_cs.Lock();
	BOOST_ASSERT(!m_strTaskBasePath.empty());
	if(m_strTaskBasePath.empty())
	{
		m_cs.Unlock();
		THROW(_t("Missing task path."), 0, 0, 0);
	}
	if (!bData && m_bSaved)
	{
		m_cs.Unlock();
		TRACE("Saving locked - file not saved\n");
		return;
	}

	if (!bData && !m_bSaved && ( (m_nStatus & ST_STEP_MASK) == ST_FINISHED || (m_nStatus & ST_STEP_MASK) == ST_CANCELLED
		|| (m_nStatus & ST_WORKING_MASK) == ST_PAUSED ))
	{
		TRACE("Last save - saving blocked\n");
		m_bSaved=true;
	}

	try
	{
		CString strPath = m_strTaskBasePath.c_str() + GetUniqueName()+( (bData) ? _T(".atd") : _T(".atp") );
		icpf::archive ar;
		ar.open(strPath, FA_WRITE | FA_CREATE | FA_TRUNCATE);
		ar.datablock_begin();

		if (bData)
		{
			m_clipboard.Serialize(ar, bData);

			if (GetStatus(ST_STEP_MASK) > ST_SEARCHING)
				m_files.Store(ar, false);
			else
				ar<<static_cast<int>(0);

			m_dpDestPath.Serialize(ar);
			ar<<m_strUniqueName;
			m_afFilters.Serialize(ar);
			ar<<m_ucCopies;
		}
		else
		{
			ar<<m_nCurrentIndex;
			ar<<(UINT)(m_nStatus & ST_WRITE_MASK);
			ar<<m_lOsError;
			ar<<m_strErrorDesc;
			m_bsSizes.Serialize(ar);
			ar<<m_nPriority;
			ar<<static_cast<unsigned long>((m_nAll & 0xffffffff00000000) >> 32);
			ar<<static_cast<unsigned long>(m_nAll & 0x00000000ffffffff);
			ar<<m_lTimeElapsed;
			ar<<static_cast<unsigned long>((m_nProcessed & 0xffffffff00000000) >> 32);
			ar<<static_cast<unsigned long>(m_nProcessed & 0x00000000ffffffff);
			ar<<m_ucCurrentCopy;
			m_clipboard.Serialize(ar, bData);
			if (GetStatus(ST_STEP_MASK) > ST_SEARCHING)
				m_files.Store(ar, true);
			else
				ar<<static_cast<int>(0);
			ar<<(unsigned char)m_bSaved;
		}
		ar.datablock_end();
		ar.close();
	}
	catch(icpf::exception& /*e*/)
	{
		m_cs.Unlock();
		return;
	}
	m_cs.Unlock();
}

/*inline*/ void CTask::KillThread()
{
	if (!GetKilledFlag())	// protection from recalling Cleanup
	{
		SetKillFlag();
		while (!GetKilledFlag())
			Sleep(10);

		// cleanup
		CleanupAfterKill();
	}
}

void CTask::BeginProcessing()
{
	m_cs.Lock();
	if (m_pThread != NULL)
	{
		m_cs.Unlock();
		return;
	}
	m_cs.Unlock();

	// create new thread
	m_uiResumeInterval=0;	// just in case
	m_bSaved=false;			// save
	SetKillFlag(false);
	SetKilledFlag(false);
	CWinThread* pThread=AfxBeginThread(m_pfnTaskProc, this, GetPriority());

	m_cs.Lock();
	m_pThread=pThread;
	m_cs.Unlock();
}

void CTask::ResumeProcessing()
{
	// the same as retry but less demanding
	if ( (GetStatus(ST_WORKING_MASK) & ST_PAUSED) && GetStatus(ST_STEP_MASK) != ST_FINISHED
		&& GetStatus(ST_STEP_MASK) != ST_CANCELLED)
	{
		SetStatus(0, ST_ERROR);
		BeginProcessing();
	}
}

bool CTask::RetryProcessing(bool bOnlyErrors/*=false*/, UINT uiInterval)
{
	// retry used to auto-resume, after loading
	if ( (GetStatus(ST_WORKING_MASK) == ST_ERROR || (!bOnlyErrors && GetStatus(ST_WORKING_MASK) != ST_PAUSED))
		&& GetStatus(ST_STEP_MASK) != ST_FINISHED && GetStatus(ST_STEP_MASK) != ST_CANCELLED)
	{
		if (uiInterval != 0)
		{
			m_uiResumeInterval+=uiInterval;
			if (m_uiResumeInterval < (UINT)GetConfig().get_signed_num(PP_CMAUTORETRYINTERVAL))
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
	if (GetStatus(ST_STEP_MASK) != ST_FINISHED && GetStatus(ST_STEP_MASK) != ST_CANCELLED)
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
	if (GetStatus(ST_STEP_MASK) != ST_FINISHED)
	{
		KillThread();
		SetStatus(ST_CANCELLED, ST_STEP_MASK);
		SetStatus(0, ST_ERROR);
		m_bSaved=false;
	}
}

void CTask::GetMiniSnapshot(TASK_MINI_DISPLAY_DATA *pData)
{
	m_cs.Lock();
	if (m_nCurrentIndex >= 0 && m_nCurrentIndex < m_files.GetSize())
		pData->m_fi=m_files.GetAt(m_nCurrentIndex);
	else
	{
		if (m_files.GetSize() > 0)
		{
			pData->m_fi=m_files.GetAt(0);
			pData->m_fi.SetFilePath(pData->m_fi.GetFullFilePath());
			pData->m_fi.SetSrcIndex(-1);
		}
		else
		{
			if (m_clipboard.GetSize() > 0)
			{
				pData->m_fi.SetFilePath(m_clipboard.GetAt(0)->GetPath());
				pData->m_fi.SetSrcIndex(-1);
			}
			else
			{
				pData->m_fi.SetFilePath(GetResManager().LoadString(IDS_NONEINPUTFILE_STRING));
				pData->m_fi.SetSrcIndex(-1);
			}
		}
	}

	pData->m_uiStatus=m_nStatus;

	// percents
	int iSize=m_files.GetSize()*m_ucCopies;
	int iIndex=m_nCurrentIndex+m_ucCurrentCopy*m_files.GetSize();
	if (m_nAll != 0 && !((m_nStatus & ST_SPECIAL_MASK) & ST_IGNORE_CONTENT))
		pData->m_nPercent=static_cast<int>( (static_cast<double>(m_nProcessed)*100.0)/static_cast<double>(m_nAll) );
	else
		if (iSize != 0)
			pData->m_nPercent=static_cast<int>( static_cast<double>(iIndex)*100.0/static_cast<double>(iSize) );
		else
			pData->m_nPercent=0;

	m_cs.Unlock();
}

void CTask::GetSnapshot(TASK_DISPLAY_DATA *pData)
{
	m_cs.Lock();
	if (m_nCurrentIndex >= 0 && m_nCurrentIndex < m_files.GetSize())
		pData->m_fi=m_files.GetAt(m_nCurrentIndex);
	else
	{
		if (m_files.GetSize() > 0)
		{
			pData->m_fi=m_files.GetAt(0);
			pData->m_fi.SetFilePath(pData->m_fi.GetFullFilePath());
			pData->m_fi.SetSrcIndex(-1);
		}
		else
		{
			if (m_clipboard.GetSize() > 0)
			{
				pData->m_fi.SetFilePath(m_clipboard.GetAt(0)->GetPath());
				pData->m_fi.SetSrcIndex(-1);
			}
			else
			{
				pData->m_fi.SetFilePath(GetResManager().LoadString(IDS_NONEINPUTFILE_STRING));
				pData->m_fi.SetSrcIndex(-1);
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
	pData->m_iIndex=m_nCurrentIndex+m_ucCurrentCopy*m_files.GetSize();
	pData->m_ullProcessedSize=m_nProcessed;
	pData->m_iSize=m_files.GetSize()*m_ucCopies;
	pData->m_ullSizeAll=m_nAll;
	pData->m_ucCurrentCopy=static_cast<unsigned char>(m_ucCurrentCopy+1);	// visual aspect
	pData->m_ucCopies=m_ucCopies;
	pData->m_pstrUniqueName=&m_strUniqueName;

	if (m_files.GetSize() > 0 && m_nCurrentIndex != -1)
		pData->m_iCurrentBufferIndex=m_bsSizes.m_bOnlyDefault ? 0 : m_files.GetAt((m_nCurrentIndex < m_files.GetSize()) ? m_nCurrentIndex : 0).GetBufferIndex();
	else
		pData->m_iCurrentBufferIndex=0;

	// percents
	if (m_nAll != 0 && !((m_nStatus & ST_SPECIAL_MASK) & ST_IGNORE_CONTENT))
		pData->m_nPercent=static_cast<int>( (static_cast<double>(m_nProcessed)*100.0)/static_cast<double>(m_nAll) );
	else
		if (pData->m_iSize != 0)
			pData->m_nPercent=static_cast<int>( static_cast<double>(pData->m_iIndex)*100.0/static_cast<double>(pData->m_iSize) );
		else
			pData->m_nPercent=0;

	// status string
	// first
	if ( (m_nStatus & ST_WORKING_MASK) == ST_ERROR )
	{
		GetResManager().LoadStringCopy(IDS_STATUS0_STRING+4, pData->m_szStatusText, _MAX_PATH);
		_tcscat(pData->m_szStatusText, _T("/"));
	}
	else if ( (m_nStatus & ST_WORKING_MASK) == ST_PAUSED )
	{
		GetResManager().LoadStringCopy(IDS_STATUS0_STRING+5, pData->m_szStatusText, _MAX_PATH);
		_tcscat(pData->m_szStatusText, _T("/"));
	}
	else if ( (m_nStatus & ST_STEP_MASK) == ST_FINISHED )
	{
		GetResManager().LoadStringCopy(IDS_STATUS0_STRING+3, pData->m_szStatusText, _MAX_PATH);
		_tcscat(pData->m_szStatusText, _T("/"));
	}
	else if ( (m_nStatus & ST_WAITING_MASK) == ST_WAITING )
	{
		GetResManager().LoadStringCopy(IDS_STATUS0_STRING+9, pData->m_szStatusText, _MAX_PATH);
		_tcscat(pData->m_szStatusText, _T("/"));
	}
	else if ( (m_nStatus & ST_STEP_MASK) == ST_CANCELLED )
	{
		GetResManager().LoadStringCopy(IDS_STATUS0_STRING+8, pData->m_szStatusText, _MAX_PATH);
		_tcscat(pData->m_szStatusText, _T("/"));
	}
	else
		_tcscpy(pData->m_szStatusText, _T(""));

	// second part
	if ( (m_nStatus & ST_STEP_MASK) == ST_DELETING )
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+6));
	else if ( (m_nStatus & ST_STEP_MASK) == ST_SEARCHING )
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+0));
	else if ((m_nStatus & ST_OPERATION_MASK) == ST_COPY )
	{
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+1));
		if(!m_afFilters.IsEmpty())
			_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_FILTERING_STRING));
	}
	else if ( (m_nStatus & ST_OPERATION_MASK) == ST_MOVE )
	{
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+2));
		if(!m_afFilters.IsEmpty())
			_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_FILTERING_STRING));
	}
	else
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+7));

	// third part
	if ( (m_nStatus & ST_SPECIAL_MASK) & ST_IGNORE_DIRS )
	{
		_tcscat(pData->m_szStatusText, _T("/"));
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+10));
	}
	if ( (m_nStatus & ST_SPECIAL_MASK) & ST_IGNORE_CONTENT )
	{
		_tcscat(pData->m_szStatusText, _T("/"));
		_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_STATUS0_STRING+11));
	}

	// count of copies
	if (m_ucCopies > 1)
	{
		_tcscat(pData->m_szStatusText, _T("/"));
		TCHAR xx[4];
		_tcscat(pData->m_szStatusText, _itot(m_ucCopies, xx, 10));
		if (m_ucCopies < 5)
			_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_COPYWORDLESSFIVE_STRING));
		else
			_tcscat(pData->m_szStatusText, GetResManager().LoadString(IDS_COPYWORDMOREFOUR_STRING));
	}

	// time
	UpdateTime();
	pData->m_lTimeElapsed=m_lTimeElapsed;

	m_cs.Unlock();
}

/*inline*/ void CTask::CleanupAfterKill()
{
	m_cs.Lock();
	m_pThread=NULL;
	UpdateTime();
	m_lLastTime=-1;
	m_cs.Unlock();
}

void CTask::DeleteProgress(LPCTSTR lpszDirectory)
{
	m_cs.Lock();
	DeleteFile(lpszDirectory+m_strUniqueName+_T(".atd"));
	DeleteFile(lpszDirectory+m_strUniqueName+_T(".atp"));
	DeleteFile(lpszDirectory+m_strUniqueName+_T(".log"));
	m_cs.Unlock();
}

void CTask::SetOsErrorCode(DWORD dwError, LPCTSTR lpszErrDesc)
{
	m_cs.Lock();
	m_lOsError=dwError;
	m_strErrorDesc=lpszErrDesc;
	m_cs.Unlock();
}

void CTask::UpdateTime()
{
	m_cs.Lock();
	if (m_lLastTime != -1)
	{
		long lVal=(long)time(NULL);
		m_lTimeElapsed+=lVal-m_lLastTime;
		m_lLastTime=lVal;
	}
	m_cs.Unlock();
}

void CTask::DecreaseOperationsPending(UINT uiBy)
{
	m_pcs->Lock();
	if (m_bQueued)
	{
		TRACE("Decreasing operations pending by %lu\n", uiBy);
		(*m_puiOperationsPending)-=uiBy;
		m_bQueued=false;
	}
	m_pcs->Unlock();
}

void CTask::IncreaseOperationsPending(UINT uiBy)
{
	TRACE("Trying to increase operations pending...\n");
	if (!m_bQueued)
	{
		TRACE("Increasing operations pending by %lu\n", uiBy);
		m_pcs->Lock();
		(*m_puiOperationsPending)+=uiBy;
		m_pcs->Unlock();
		m_bQueued=true;
	}
}

const CFiltersArray* CTask::GetFilters()
{
	return &m_afFilters;
}

void CTask::SetFilters(const CFiltersArray* pFilters)
{
	BOOST_ASSERT(pFilters);
	if(!pFilters)
		return;

	m_cs.Lock();
	m_afFilters = *pFilters;
	m_cs.Unlock();
}

bool CTask::CanBegin()
{
	bool bRet=true;
	m_cs.Lock();
	if (GetContinueFlag() || GetForceFlag())
	{
		TRACE("New operation Begins... continue: %d, force: %d\n", GetContinueFlag(), GetForceFlag());
		IncreaseOperationsPending();
		SetForceFlag(false);
		SetContinueFlag(false);
	}
	else
		bRet=false;
	m_cs.Unlock();

	return bRet;
}

void CTask::SetCopies(unsigned char ucCopies)
{
	m_cs.Lock();
	m_ucCopies=ucCopies;
	m_cs.Unlock();
}

unsigned char CTask::GetCopies()
{
	unsigned char ucCopies;
	m_cs.Lock();
	ucCopies=m_ucCopies;
	m_cs.Unlock();

	return ucCopies;
}

void CTask::SetCurrentCopy(unsigned char ucCopy)
{
	m_cs.Lock();
	m_ucCurrentCopy=ucCopy;
	m_cs.Unlock();
}

unsigned char CTask::GetCurrentCopy()
{
	m_cs.Lock();
	unsigned char ucCopy=m_ucCurrentCopy;
	m_cs.Unlock();

	return ucCopy;
}

void CTask::SetLastProcessedIndex(int iIndex)
{
	m_cs.Lock();
	m_iLastProcessedIndex=iIndex;
	m_cs.Unlock();
}

int CTask::GetLastProcessedIndex()
{
	int iIndex;
	m_cs.Lock();
	iIndex=m_iLastProcessedIndex;
	m_cs.Unlock();

	return iIndex;
}

bool CTask::GetRequiredFreeSpace(ull_t *pullNeeded, ull_t *pullAvailable)
{
	*pullNeeded=GetAllSize()-GetProcessedSize(); // it'd be nice to round up to take cluster size into consideration,
	// but GetDiskFreeSpace returns flase values

	// get free space
	if (!GetDynamicFreeSpace(GetDestPath().GetPath(), pullAvailable, NULL))
		return true;

	return (*pullNeeded <= *pullAvailable);
}

void CTask::SetTaskPath(const tchar_t* pszDir)
{
	m_cs.Lock();
	m_strTaskBasePath = pszDir;
	m_cs.Unlock();
}

const tchar_t* CTask::GetTaskPath() const
{
	const tchar_t* pszText = NULL;
	m_cs.Lock();
	pszText = m_strTaskBasePath.c_str();
	m_cs.Unlock();

	return pszText;
}

void CTask::SetForceFlag(bool bFlag)
{
	m_cs.Lock();
	m_bForce=bFlag;
	m_cs.Unlock();
}

bool CTask::GetForceFlag()
{
	return m_bForce;
}

void CTask::SetContinueFlag(bool bFlag)
{
	m_cs.Lock();
	m_bContinue=bFlag;
	m_cs.Unlock();
}

bool CTask::GetContinueFlag()
{
	return m_bContinue;
}

////////////////////////////////////////////////////////////////////////////////
// CTaskArray members
CTaskArray::CTaskArray() :
CArray<CTask*, CTask*>(),
m_uhRange(0),
m_uhPosition(0),
m_uiOperationsPending(0),
m_lFinished(0),
m_piFeedbackFactory(NULL)
{
}

CTaskArray::~CTaskArray()
{
	// NOTE: do not delete the feedback factory, since we are not responsible for releasing it
}

void CTaskArray::Create(chcore::IFeedbackHandlerFactory* piFeedbackHandlerFactory, UINT (*pfnTaskProc)(LPVOID pParam))
{
	BOOST_ASSERT(piFeedbackHandlerFactory && pfnTaskProc);

	m_tcd.pcs=&m_cs;
	m_tcd.pfnTaskProc=pfnTaskProc;
	m_tcd.pTasksAll=&m_uhRange;
	m_tcd.pTasksProcessed=&m_uhPosition;
	m_tcd.puiOperationsPending=&m_uiOperationsPending;
	m_tcd.plFinished=&m_lFinished;
	m_piFeedbackFactory = piFeedbackHandlerFactory;
}

CTask* CTaskArray::CreateTask()
{
	BOOST_ASSERT(m_piFeedbackFactory);
	if(!m_piFeedbackFactory)
		return NULL;

	chcore::IFeedbackHandler* piHandler = m_piFeedbackFactory->Create();
	if(!piHandler)
		return NULL;

	CTask* pTask = NULL;
	try
	{
		pTask = new CTask(piHandler, &m_tcd);
	}
	catch(...)
	{
		//		piHandler->Delete();
		throw;
	}

	return pTask;
}

int CTaskArray::GetSize( )
{
	m_cs.Lock();
	int nSize=m_nSize;
	m_cs.Unlock();

	return nSize;
}

int CTaskArray::GetUpperBound( )
{
	m_cs.Lock();
	int upper=m_nSize;
	m_cs.Unlock();

	return upper-1;
}

void CTaskArray::SetSize( int nNewSize, int nGrowBy )
{
	m_cs.Lock();
	(static_cast<CArray<CTask*, CTask*>*>(this))->SetSize(nNewSize, nGrowBy);
	m_cs.Unlock();
}

CTask* CTaskArray::GetAt( int nIndex )
{
	ASSERT(nIndex >= 0 && nIndex < m_nSize);
	m_cs.Lock();
	CTask* pTask=m_pData[nIndex];
	m_cs.Unlock();

	return pTask;
}

/*
void CTaskArray::SetAt( int nIndex, CTask* newElement )
{
m_cs.Lock();
ASSERT(nIndex >= 0 && nIndex < m_nSize);
m_uhRange-=m_pData[nIndex]->GetAllSize();	// subtract old element
m_pData[nIndex]=newElement;
m_uhRange+=m_pData[nIndex]->GetAllSize();	// add new
m_cs.Unlock();
}
*/

int CTaskArray::Add( CTask* newElement )
{
	if(!newElement)
		THROW(_t("Invalid argument"), 0, 0, 0);
	m_cs.Lock();
	// here we know load succeeded
	newElement->SetTaskPath(m_strTasksDir.c_str());

	m_uhRange+=newElement->GetAllSize();
	m_uhPosition+=newElement->GetProcessedSize();
	int pos=(static_cast<CArray<CTask*, CTask*>*>(this))->Add(newElement);
	m_cs.Unlock();

	return pos;
}

void CTaskArray::RemoveAt(int nIndex, int nCount)
{
	m_cs.Lock();
	for (int i=nIndex;i<nIndex+nCount;i++)
	{
		CTask* pTask=GetAt(i);

		// kill task if needed
		pTask->KillThread();

		m_uhRange-=pTask->GetAllSize();
		m_uhPosition-=pTask->GetProcessedSize();

		delete pTask;
	}

	// remove elements from array
	(static_cast<CArray<CTask*, CTask*>*>(this))->RemoveAt(nIndex, nCount);
	m_cs.Unlock();
}

void CTaskArray::RemoveAll()
{
	m_cs.Lock();
	CTask* pTask;

	for (int i=0;i<GetSize();i++)
		GetAt(i)->SetKillFlag();		// send an info about finishing

	// wait for finishing and get rid of it
	for (int i=0;i<GetSize();i++)
	{
		pTask=GetAt(i);

		// wait
		while (!pTask->GetKilledFlag())
			Sleep(10);

		pTask->CleanupAfterKill();

		m_uhRange-=pTask->GetAllSize();
		m_uhPosition-=pTask->GetProcessedSize();

		// delete data
		delete pTask;
	}

	(static_cast<CArray<CTask*, CTask*>*>(this))->RemoveAll();
	m_cs.Unlock();
}

void CTaskArray::RemoveAllFinished()
{
	m_cs.Lock();
	int i=GetSize();

	while (i)
	{
		CTask* pTask=GetAt(i-1);

		// delete only when the thread is finished
		if ( (pTask->GetStatus(ST_STEP_MASK) == ST_FINISHED || pTask->GetStatus(ST_STEP_MASK) == ST_CANCELLED)
			&& pTask->GetKilledFlag())
		{
			m_uhRange-=pTask->GetAllSize();
			m_uhPosition-=pTask->GetProcessedSize();

			// delete associated files
			pTask->DeleteProgress(m_strTasksDir.c_str());

			delete pTask;

			static_cast<CArray<CTask*, CTask*>*>(this)->RemoveAt(i-1);
		}

		--i;
	}

	m_cs.Unlock();
}

void CTaskArray::RemoveFinished(CTask** pSelTask)
{
	m_cs.Lock();
	for (int i=0;i<GetSize();i++)
	{
		CTask* pTask=GetAt(i);

		if (pTask == *pSelTask && (pTask->GetStatus(ST_STEP_MASK) == ST_FINISHED || pTask->GetStatus(ST_STEP_MASK) == ST_CANCELLED))
		{
			// kill task if needed
			pTask->KillThread();

			m_uhRange-=pTask->GetAllSize();
			m_uhPosition-=pTask->GetProcessedSize();

			// delete associated files
			pTask->DeleteProgress(m_strTasksDir.c_str());

			// delete data
			delete pTask;

			static_cast<CArray<CTask*, CTask*>*>(this)->RemoveAt(i);

			m_cs.Unlock();
			return;
		}
	}
	m_cs.Unlock();
}

void CTaskArray::SaveData()
{
	m_cs.Lock();
	for (int i=0;i<m_nSize;i++)
		m_pData[i]->Store(true);
	m_cs.Unlock();
}

void CTaskArray::SaveProgress()
{
	m_cs.Lock();
	for (int i=0;i<m_nSize;i++)
		m_pData[i]->Store(false);
	m_cs.Unlock();
}

void CTaskArray::LoadDataProgress()
{
	m_cs.Lock();
	CFileFind finder;
	CTask* pTask;

	BOOL bWorking=finder.FindFile(CString(m_strTasksDir.c_str())+_T("*.atd"));
	while ( bWorking )
	{
		bWorking=finder.FindNextFile();

		// load data
		pTask = CreateTask();

		try
		{
			CString strPath=finder.GetFilePath();

			// load data file
			icpf::archive ar;
			ar.open(strPath, FA_READ);
			ar.datablock_begin();
			pTask->Load(ar, true);
			ar.datablock_end();
			ar.close();

			// load progress file
			strPath=strPath.Left(strPath.GetLength()-4);
			strPath+=_T(".atp");
			icpf::archive ar2;
			ar2.open(strPath, FA_READ);
			ar2.datablock_begin();
			pTask->Load(ar2, false);
			ar2.datablock_end();
			ar2.close();

			// add read task to array
			Add(pTask);
		}
		catch(icpf::exception&)
		{
			delete pTask;
		}
	}
	finder.Close();

	m_cs.Unlock();
}

void CTaskArray::TasksBeginProcessing()
{
	for (int i=0;i<GetSize();i++)
		GetAt(i)->BeginProcessing();
}

void CTaskArray::TasksPauseProcessing()
{
	for (int i=0;i<GetSize();i++)
		GetAt(i)->PauseProcessing();
}

void CTaskArray::TasksResumeProcessing()
{
	for (int i=0;i<GetSize();i++)
		GetAt(i)->ResumeProcessing();
}

void CTaskArray::TasksRestartProcessing()
{
	for (int i=0;i<GetSize();i++)
		GetAt(i)->RestartProcessing();
}

bool CTaskArray::TasksRetryProcessing(bool bOnlyErrors/*=false*/, UINT uiInterval)
{
	bool bChanged=false;
	for (int i=0;i<GetSize();i++)
	{
		if (GetAt(i)->RetryProcessing(bOnlyErrors, uiInterval))
			bChanged=true;
	}

	return bChanged;
}

void CTaskArray::TasksCancelProcessing()
{
	for (int i=0;i<GetSize();i++)
		GetAt(i)->CancelProcessing();
}

ull_t CTaskArray::GetPosition()
{
	m_cs.Lock();
	ull_t rv=m_uhPosition;
	m_cs.Unlock();

	return rv;
}

ull_t CTaskArray::GetRange()
{
	m_cs.Lock();
	ull_t rv=m_uhRange;
	m_cs.Unlock();

	return rv;
}

int CTaskArray::GetPercent()
{
	int pos;
	m_cs.Lock();
	if (m_uhRange != 0)
		pos=static_cast<int>((static_cast<double>(m_uhPosition)*100.0)/static_cast<double>(m_uhRange));
	else
		if (GetSize() != 0)		// if anything is in an array, but size of it is 0
			pos=100;
		else
			pos=0;
	m_cs.Unlock();

	return pos;
}

UINT CTaskArray::GetOperationsPending()
{
	m_cs.Lock();
	UINT uiOP=m_uiOperationsPending;
	m_cs.Unlock();
	return uiOP;
}

bool CTaskArray::IsFinished()
{
	bool bFlag=true;
	UINT uiStatus;

	m_cs.Lock();
	if (m_uiOperationsPending != 0)
		bFlag=false;
	else
	{
		for (int i=0;i<GetSize();i++)
		{
			uiStatus=GetAt(i)->GetStatus();
			bFlag=((uiStatus & ST_STEP_MASK) == ST_FINISHED || (uiStatus & ST_STEP_MASK) == ST_CANCELLED
				|| (uiStatus & ST_WORKING_MASK) == ST_PAUSED
				|| ((uiStatus & ST_WORKING_MASK) == ST_ERROR && !GetConfig().get_bool(PP_CMAUTORETRYONERROR)));
		}
	}

	m_cs.Unlock();
	return bFlag;
}