//******************************************************************************
//   Copyright (C) 2001-2008 by Jozef Starosczyk
//   ixen@copyhandler.com
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Library General Public License
//   (version 2) as published by the Free Software Foundation;
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU Library General Public
//   License along with this program; if not, write to the
//   Free Software Foundation, Inc.,
//   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
/// @file ClipboardMonitor.cpp
/// @brief Contains the implementation of clipboard monitor package.
//******************************************************************************
#include "stdafx.h"
#include "TWorkerThreadController.h"
#include "ClipboardMonitor.h"
#include "ch.h"
#include "task.h"
#include "CfgProperties.h"
#include "charvect.h"
#include "FolderDialog.h"
#include "task.h"
#include "ShutdownDlg.h"

CClipboardMonitor CClipboardMonitor::S_ClipboardMonitor;

CClipboardMonitor::CClipboardMonitor()
{
}

CClipboardMonitor::~CClipboardMonitor()
{
	Stop();
}

void CClipboardMonitor::StartMonitor(CTaskArray* pTasks)
{
	CClipboardMonitor::S_ClipboardMonitor.Start(pTasks);
}

void CClipboardMonitor::StopMonitor()
{
	return CClipboardMonitor::S_ClipboardMonitor.Stop();
}

void CClipboardMonitor::Start(CTaskArray* pTasks)
{
	m_pTasks = pTasks;

	m_threadWorker.StartThread(&CClipboardMonitor::ClipboardMonitorProc, this);
}

void CClipboardMonitor::Stop()
{
	m_threadWorker.StopThread();
}

DWORD WINAPI CClipboardMonitor::ClipboardMonitorProc(LPVOID pParam)
{
	CClipboardMonitor* pData = (CClipboardMonitor*)pParam;

	// bufor
	TCHAR path[_MAX_PATH];
	//	UINT i;	// counter
	CTaskPtr spTask;	// ptr to a task
	CClipboardEntryPtr spEntry;

	// register clipboard format
	UINT nFormat=RegisterClipboardFormat(_T("Preferred DropEffect"));
	UINT uiCounter=0, uiShutCounter=0;

	icpf::config& rConfig = GetConfig();
	for(;;)
	{
		if (uiCounter == 0 && rConfig.get_bool(PP_PCLIPBOARDMONITORING) && IsClipboardFormatAvailable(CF_HDROP))
		{
			// get data from clipboard
			OpenClipboard(NULL);
			HANDLE handle=GetClipboardData(CF_HDROP);

			UINT nCount=DragQueryFile(static_cast<HDROP>(handle), 0xffffffff, NULL, 0);

			spTask = pData->m_pTasks->CreateTask();

			for (UINT i=0;i<nCount;i++)
			{
				DragQueryFile(static_cast<HDROP>(handle), i, path, _MAX_PATH);
				spEntry.reset(new CClipboardEntry);
				spEntry->SetPath(path);
				spTask->AddClipboardData(spEntry);
			}

			if (IsClipboardFormatAvailable(nFormat))
			{
				handle=GetClipboardData(nFormat);
				LPVOID addr=GlobalLock(handle);

				DWORD dwData=((DWORD*)addr)[0];
				if (dwData & DROPEFFECT_COPY)
					spTask->SetStatus(ST_COPY, ST_OPERATION_MASK);	// copy
				else if (dwData & DROPEFFECT_MOVE)
					spTask->SetStatus(ST_MOVE, ST_OPERATION_MASK);	// move

				GlobalUnlock(handle);
			}
			else
				spTask->SetStatus(ST_COPY, ST_OPERATION_MASK);	// default - copy

			EmptyClipboard();
			CloseClipboard();

			BUFFERSIZES bs;
			bs.m_bOnlyDefault=rConfig.get_bool(PP_BFUSEONLYDEFAULT);
			bs.m_uiDefaultSize=(UINT)rConfig.get_signed_num(PP_BFDEFAULT);
			bs.m_uiOneDiskSize=(UINT)rConfig.get_signed_num(PP_BFONEDISK);
			bs.m_uiTwoDisksSize=(UINT)rConfig.get_signed_num(PP_BFTWODISKS);
			bs.m_uiCDSize=(UINT)rConfig.get_signed_num(PP_BFCD);
			bs.m_uiLANSize=(UINT)rConfig.get_signed_num(PP_BFLAN);

			spTask->SetBufferSizes(&bs);
			spTask->SetPriority(boost::numeric_cast<int>(rConfig.get_signed_num(PP_CMDEFAULTPRIORITY)));

			// get dest folder
			CFolderDialog dlg;

			const tchar_t* pszPath = NULL;
			dlg.m_bdData.cvShortcuts.clear(true);
			size_t stCount = rConfig.get_value_count(PP_SHORTCUTS);
			for(size_t stIndex = 0; stIndex < stCount; stIndex++)
			{
				pszPath = rConfig.get_string(PP_SHORTCUTS, stIndex);
				dlg.m_bdData.cvShortcuts.push_back(pszPath);
			}

			dlg.m_bdData.cvRecent.clear(true);
			stCount = rConfig.get_value_count(PP_RECENTPATHS);
			for(size_t stIndex = 0; stIndex < stCount; stIndex++)
			{
				pszPath = rConfig.get_string(PP_RECENTPATHS, stIndex);
				dlg.m_bdData.cvRecent.push_back(pszPath);
			}

			dlg.m_bdData.bExtended=rConfig.get_bool(PP_FDEXTENDEDVIEW);
			dlg.m_bdData.cx=boost::numeric_cast<int>(rConfig.get_signed_num(PP_FDWIDTH));
			dlg.m_bdData.cy=boost::numeric_cast<int>(rConfig.get_signed_num(PP_FDHEIGHT));
			dlg.m_bdData.iView=boost::numeric_cast<int>(rConfig.get_signed_num(PP_FDSHORTCUTLISTSTYLE));
			dlg.m_bdData.bIgnoreDialogs=rConfig.get_bool(PP_FDIGNORESHELLDIALOGS);

			dlg.m_bdData.strInitialDir=(dlg.m_bdData.cvRecent.size() > 0) ? dlg.m_bdData.cvRecent.at(0) : _T("");

			int iStatus=spTask->GetStatus(ST_OPERATION_MASK);
			if (iStatus == ST_COPY)
				dlg.m_bdData.strCaption=GetResManager().LoadString(IDS_TITLECOPY_STRING);
			else if (iStatus == ST_MOVE)
				dlg.m_bdData.strCaption=GetResManager().LoadString(IDS_TITLEMOVE_STRING);
			else
				dlg.m_bdData.strCaption=GetResManager().LoadString(IDS_TITLEUNKNOWNOPERATION_STRING);
			dlg.m_bdData.strText=GetResManager().LoadString(IDS_MAINBROWSETEXT_STRING);

			// set count of data to display
			size_t stClipboardSize = spTask->GetClipboardDataSize();
			size_t stEntries = (stClipboardSize > 3) ? 2 : stClipboardSize;
			for(size_t i = 0; i < stEntries; i++)
			{
				dlg.m_bdData.strText += spTask->GetClipboardData(i)->GetPath() + _T("\n");
			}

			// add ...
			if (stEntries < stClipboardSize)
				dlg.m_bdData.strText+=_T("...");

			// show window
			INT_PTR iResult=dlg.DoModal();

			// set data to config
			rConfig.clear_array_values(PP_SHORTCUTS);
			for(char_vector::iterator it = dlg.m_bdData.cvShortcuts.begin(); it != dlg.m_bdData.cvShortcuts.end(); it++)
			{
				rConfig.set_string(PP_SHORTCUTS, (*it), icpf::property::action_add);
			}

			rConfig.clear_array_values(PP_RECENTPATHS);
			for(char_vector::iterator it = dlg.m_bdData.cvRecent.begin(); it != dlg.m_bdData.cvRecent.end(); it++)
			{
				rConfig.set_string(PP_RECENTPATHS, (*it), icpf::property::action_add);
			}

			rConfig.set_bool(PP_FDEXTENDEDVIEW, dlg.m_bdData.bExtended);
			rConfig.set_signed_num(PP_FDWIDTH, dlg.m_bdData.cx);
			rConfig.set_signed_num(PP_FDHEIGHT, dlg.m_bdData.cy);
			rConfig.set_signed_num(PP_FDSHORTCUTLISTSTYLE, dlg.m_bdData.iView);
			rConfig.set_bool(PP_FDIGNORESHELLDIALOGS, dlg.m_bdData.bIgnoreDialogs);
			rConfig.write(NULL);

			if(iResult != IDOK)
				spTask.reset();
			else
			{
				// get dest path
				CString strData;
				dlg.GetPath(strData);
				spTask->SetDestPath(strData);

				// get the relationship between src and dst paths
				for (size_t stIndex = 0; stIndex < spTask->GetClipboard()->GetSize(); ++stIndex)
				{
					spTask->GetClipboard()->GetAt(stIndex)->CalcBufferIndex(spTask->GetDestPath());
				}

				// add task to a list of tasks and start
				pData->m_pTasks->Add(spTask);

				// write spTask to a file
				spTask->Store(true);
				spTask->Store(false);

				// start processing
				spTask->BeginProcessing();
			}
		}

		// do we need to check for turning computer off
		if(uiShutCounter == 0 && GetConfig().get_bool(PP_PSHUTDOWNAFTREFINISHED))
		{
			if(pData->m_pTasks->AreAllFinished())
			{
				TRACE("Shut down windows\n");
				bool bShutdown=true;
				if (GetConfig().get_signed_num(PP_PTIMEBEFORESHUTDOWN) != 0)
				{
					CShutdownDlg dlg;
					dlg.m_iOverallTime = boost::numeric_cast<int>(GetConfig().get_signed_num(PP_PTIMEBEFORESHUTDOWN));
					if (dlg.m_iOverallTime < 0)
						dlg.m_iOverallTime=-dlg.m_iOverallTime;
					bShutdown=(dlg.DoModal() != IDCANCEL);
				}

				GetConfig().set_bool(PP_PSHUTDOWNAFTREFINISHED, false);
				GetConfig().write(NULL);
				if (bShutdown)
				{
					// adjust token privileges for NT
					HANDLE hToken=NULL;
					TOKEN_PRIVILEGES tp;
					if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken)
						&& LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid))
					{
						tp.PrivilegeCount=1;
						tp.Privileges[0].Attributes=SE_PRIVILEGE_ENABLED;

						AdjustTokenPrivileges(hToken, FALSE, &tp, NULL, NULL, NULL);
					}

					BOOL bExit=ExitWindowsEx(EWX_POWEROFF | EWX_SHUTDOWN | (GetConfig().get_bool(PP_PFORCESHUTDOWN) ? EWX_FORCE : 0), 0);
					if (bExit)
						return 1;
					else
					{
						// some kind of error
						ictranslate::CFormat fmt(GetResManager().LoadString(IDS_SHUTDOWNERROR_STRING));
						fmt.SetParam(_t("%errno"), GetLastError());
						AfxMessageBox(fmt, MB_ICONERROR | MB_OK | MB_SYSTEMMODAL);
					}
				}
			}
		}

		// sleep for some time
		const int iSleepCount=200;
		
		if(pData->m_threadWorker.KillRequested())
			break;
		
		uiCounter+=iSleepCount;
		uiShutCounter+=iSleepCount;
		if (uiCounter >= (UINT)GetConfig().get_signed_num(PP_PMONITORSCANINTERVAL))
			uiCounter=0;
		if (uiShutCounter >= 800)
			uiShutCounter=0;
	}

	TRACE("Monitoring clipboard proc aborted...\n");

	return 0;
}
