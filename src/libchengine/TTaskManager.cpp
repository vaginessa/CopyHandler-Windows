/***************************************************************************
*   Copyright (C) 2001-2010 by Jozef Starosczyk                           *
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
#include "stdafx.h"
#include "TTaskManager.h"
#include "TTask.h"

#include "TTaskStatsSnapshot.h"
#include "TTaskManagerStatsSnapshot.h"
#include "TTaskInfo.h"
#include "../liblogger/TLoggerPaths.h"
#include "../liblogger/TAsyncMultiLogger.h"
#include "../libchcore/TCoreException.h"
#include "../libserializer/TFakeFileSerializer.h"

using namespace chcore;
using namespace string;
using namespace serializer;

namespace chengine
{
	////////////////////////////////////////////////////////////////////////////////
	// TTaskManager members
	TTaskManager::TTaskManager(const ISerializerFactoryPtr& spSerializerFactory,
		const IFeedbackHandlerFactoryPtr& spFeedbackHandlerFactory,
		const TSmartPath& pathLogDir,
		const logger::TMultiLoggerConfigPtr& spMultiLoggerConfig,
		const logger::TLogFileDataPtr& spLogFileData,
		bool bForceRecreateSerializer) :
		m_pathLogDir(pathLogDir),
		m_spFeedbackFactory(spFeedbackHandlerFactory),
		m_spSerializerFactory(spSerializerFactory),
		m_spMultiLoggerConfig(spMultiLoggerConfig),
		m_spLog(logger::MakeLogger(spLogFileData, L"TaskManager"))
	{
		if(!spFeedbackHandlerFactory)
			throw TCoreException(eErr_InvalidPointer, L"spFeedbackHandlerFactory", LOCATION);
		if (!spSerializerFactory)
			throw TCoreException(eErr_InvalidPointer, L"spSerializerFactory", LOCATION);
		if (!spMultiLoggerConfig)
			throw TCoreException(eErr_InvalidPointer, L"spMultiLoggerConfig", LOCATION);
		m_spSerializer = m_spSerializerFactory->CreateTaskManagerSerializer(bForceRecreateSerializer);
	}

	TTaskManager::~TTaskManager()
	{
		// NOTE: do not delete the feedback factory, since we are not responsible for releasing it
	}

	TTaskPtr TTaskManager::CreateTask(const TTaskDefinition& tTaskDefinition)
	{
		IFeedbackHandlerPtr spHandler = m_spFeedbackFactory->Create();
		TSmartPath pathLog = CreateTaskLogPath(tTaskDefinition.GetTaskName());
		logger::TLogFileDataPtr spLogFileData = logger::TAsyncMultiLogger::GetInstance()->CreateLoggerData(pathLog.ToString(), m_spMultiLoggerConfig);

		ISerializerPtr spSerializer = m_spSerializerFactory->CreateTaskSerializer(spLogFileData, tTaskDefinition.GetTaskName());

		TTaskPtr spTask(new TTask(spSerializer, spHandler, tTaskDefinition, spLogFileData));

		spTask->Store(true);

		Add(spTask);

		return spTask;
	}

	TTaskPtr TTaskManager::ImportTask(const TSmartPath& strTaskPath)
	{
		// load task definition from the new location
		TTaskDefinition tTaskDefinition;
		tTaskDefinition.Load(strTaskPath);

		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			const TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();

			if (spTask->GetTaskName() == tTaskDefinition.GetTaskName())
				throw TCoreException(eErr_TaskAlreadyExists, L"Task with specified name already exist", LOCATION);
		}

		return CreateTask(tTaskDefinition);
	}

	size_t TTaskManager::GetSize() const
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		return m_tTasks.GetCount();
	}

	TTaskPtr TTaskManager::GetAt(size_t nIndex) const
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		const TTaskInfoEntry& rInfo = m_tTasks.GetAt(nIndex);
		return rInfo.GetTask();
	}

	TTaskPtr TTaskManager::GetTaskByTaskID(taskid_t tTaskID) const
	{
		if (tTaskID == NoTaskID)
			return TTaskPtr();

		TTaskInfoEntry tEntry;

		boost::shared_lock<boost::shared_mutex> lock(m_lock);

		if (!m_tTasks.GetByTaskID(tTaskID, tEntry))
			return TTaskPtr();

		return tEntry.GetTask();
	}

	void TTaskManager::Add(const TTaskPtr& spNewTask)
	{
		if (!spNewTask)
			throw TCoreException(eErr_InvalidArgument, L"spNewTask", LOCATION);

		boost::unique_lock<boost::shared_mutex> lock(m_lock);

		int iOrder = 1;
		if (!m_tTasks.IsEmpty())
		{
			const TTaskInfoEntry& rEntry = m_tTasks.GetAt(m_tTasks.GetCount() - 1);
			iOrder = rEntry.GetOrder() + 1;
		}

		m_tTasks.Add(spNewTask->GetSerializer()->GetLocation(), spNewTask->GetLogPath(), iOrder, spNewTask);

		spNewTask->OnRegisterTask();
	}

	void TTaskManager::ClearBeforeExit()
	{
		StopAllTasks();

		// ensure everything is stored so that we can resume processing in the future
		Store(true);

		// now remove all tasks without serializing anymore (prevent accidental
		// serialization)
		{
			boost::unique_lock<boost::shared_mutex> lock(m_lock);
			m_tTasks.Clear();
			m_tTasks.ClearModifications();
		}
	}

	void TTaskManager::RemoveAllFinished()
	{
		// separate scope for locking
		{
			boost::unique_lock<boost::shared_mutex> lock(m_lock);

			size_t stIndex = m_tTasks.GetCount();
			while (stIndex--)
			{
				TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
				TTaskPtr spTask = rEntry.GetTask();
				if (!spTask)
					throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);

				// delete only when the thread is finished
				ETaskCurrentState eState = spTask->GetTaskState();

				if ((eState == eTaskState_Finished || eState == eTaskState_Cancelled || eState == eTaskState_LoadError))
				{
					spTask->KillThread();

					spTask->OnUnregisterTask();

					RemoveFilesForTask(spTask);

					m_tTasks.RemoveAt(stIndex);
				}
			}
		}
	}

	void TTaskManager::RemoveFinished(const TTaskPtr& spSelTask)
	{
		// separate scope for locking
		{
			boost::unique_lock<boost::shared_mutex> lock(m_lock);

			size_t stIndex = m_tTasks.GetCount();
			while (stIndex--)
			{
				TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
				TTaskPtr spTask = rEntry.GetTask();
				if (!spTask)
					throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);

				// delete only when the thread is finished
				if (spTask == spSelTask)
				{
					ETaskCurrentState eState = spTask->GetTaskState();

					if (eState == eTaskState_Finished || eState == eTaskState_Cancelled || eState == eTaskState_LoadError)
					{
						spTask->KillThread();

						spTask->OnUnregisterTask();

						RemoveFilesForTask(spTask);

						m_tTasks.RemoveAt(stIndex);
					}
					break;
				}
			}
		}
	}

	void TTaskManager::StopAllTasks()
	{
		boost::unique_lock<boost::shared_mutex> lock(m_lock);

		StopAllTasksNL();
	}

	void TTaskManager::ResumeWaitingTasks(size_t stMaxRunningTasks)
	{
		size_t stRunningCount = GetCountOfRunningTasks();

		boost::shared_lock<boost::shared_mutex> lock(m_lock);

		size_t stTasksToRun = stMaxRunningTasks == 0 ? std::numeric_limits<size_t>::max() : stMaxRunningTasks;

		if (stRunningCount < stTasksToRun)
		{
			stTasksToRun -= stRunningCount;

			for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
			{
				TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
				TTaskPtr spTask = rEntry.GetTask();
				if (!spTask)
					throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);

				// turn on some thread - find something with wait state
				if (spTask->GetTaskState() == eTaskState_Waiting)
				{
					spTask->SetContinueFlagNL(true);
					if (--stTasksToRun == 0)
						break;
				}
			}
		}
	}

	void TTaskManager::TasksBeginProcessing()
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();
			if (!spTask)
				throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);
			spTask->BeginProcessing();
		}
	}

	void TTaskManager::TasksPauseProcessing()
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();
			if (!spTask)
				throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);
			spTask->PauseProcessing();
		}
	}

	void TTaskManager::TasksResumeProcessing()
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();
			if (!spTask)
				throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);
			spTask->ResumeProcessing();
		}
	}

	void TTaskManager::TasksRestartProcessing()
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();
			if (!spTask)
				throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);
			spTask->RestartProcessing();
		}
	}

	bool TTaskManager::TasksRetryProcessing()
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		bool bChanged = false;
		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();
			if (!spTask)
				throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);
			if (spTask->RetryProcessing())
				bChanged = true;
		}

		return bChanged;
	}

	void TTaskManager::TasksCancelProcessing()
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();
			if (!spTask)
				throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);
			spTask->CancelProcessing();
		}
	}

	bool TTaskManager::AreAllFinished()
	{
		bool bFlag = true;

		if (GetCountOfRunningTasks() != 0)
			bFlag = false;
		else
		{
			boost::shared_lock<boost::shared_mutex> lock(m_lock);
			for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
			{
				TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
				TTaskPtr spTask = rEntry.GetTask();
				if (!spTask)
					throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);

				ETaskCurrentState eState = spTask->GetTaskState();
				bFlag = (eState == eTaskState_Finished || eState == eTaskState_Cancelled || eState == eTaskState_Paused || eState == eTaskState_Error || eState == eTaskState_LoadError);

				if (!bFlag)
					break;
			}
		}

		return bFlag;
	}

	void TTaskManager::GetStatsSnapshot(TTaskManagerStatsSnapshotPtr& spSnapshot) const
	{
		if (!spSnapshot)
			throw TCoreException(eErr_InvalidArgument, L"spSnapshot", LOCATION);

		spSnapshot->Clear();

		boost::shared_lock<boost::shared_mutex> lock(m_lock);

		size_t stRunningTasks = 0;
		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			const TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();
			if (!spTask)
				throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);

			TTaskStatsSnapshotPtr spStats(new TTaskStatsSnapshot);
			spTask->GetStatsSnapshot(spStats);
			spStats->SetTaskID(rEntry.GetObjectID());

			if (spStats->IsTaskRunning() && spStats->GetTaskState())
				++stRunningTasks;

			spSnapshot->AddTaskStats(spStats);
		}

		spSnapshot->SetRunningTasks(stRunningTasks);
	}

	size_t TTaskManager::GetCountOfRunningTasks() const
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);

		TTaskStatsSnapshot tTaskStats;

		size_t stRunningTasks = 0;

		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			const TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();
			if (!spTask)
				throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);

			if (spTask->IsRunning() && spTask->GetTaskState() == eTaskState_Processing)
				++stRunningTasks;
		}

		return stRunningTasks;
	}

	void TTaskManager::StopAllTasksNL()
	{
		// kill all unfinished tasks - send kill request
		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();
			if (!spTask)
				throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);
			spTask->RequestStopThread();
		}

		// wait for finishing
		for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
		{
			TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
			TTaskPtr spTask = rEntry.GetTask();
			if (!spTask)
				throw TCoreException(eErr_InvalidPointer, L"spTask", LOCATION);
			spTask->KillThread();
		}
	}

	void TTaskManager::Store(bool bForce)
	{
		TSimpleTimer timer(true);

		// store this container information
		{
			ISerializerContainerPtr spContainer = m_spSerializer->GetContainer(_T("tasks"));

			boost::shared_lock<boost::shared_mutex> lock(m_lock);
			m_tTasks.Store(spContainer);

			for (size_t stIndex = 0; stIndex != m_tTasks.GetCount(); ++stIndex)
			{
				TTaskPtr spTask = m_tTasks.GetAt(stIndex).GetTask();
				spTask->Store(bForce);
			}
		}

		// store obsolete info
		{
			ISerializerContainerPtr spContainer = m_spSerializer->GetContainer(_T("obsolete_tasks"));

			boost::shared_lock<boost::shared_mutex> lock(m_lock);
			m_tObsoleteFiles.Store(spContainer);
		}

		unsigned long long ullGatherTime = timer.Checkpoint(); ullGatherTime;

		m_spSerializer->Flush();

		unsigned long long ullFlushTime = timer.Stop(); ullFlushTime;
		//DBTRACE2(_T("TaskManager::Store() - gather: %I64u ms, flush: %I64u ms\n"), ullGatherTime, ullFlushTime);
	}

	void TTaskManager::Load()
	{
		// load list of tasks (without loading tasks themselves)
		{
			boost::unique_lock<boost::shared_mutex> lock(m_lock);

			if (!m_tTasks.IsEmpty())
				throw TCoreException(eErr_InternalProblem, L"Task list not empty when trying to load", LOCATION);

			ISerializerContainerPtr spContainer = m_spSerializer->GetContainer(_T("tasks"));
			m_tTasks.Load(spContainer);
		}

		// load list of task files to delete
		{
			boost::unique_lock<boost::shared_mutex> lock(m_lock);

			ISerializerContainerPtr spContainer = m_spSerializer->GetContainer(_T("obsolete_tasks"));
			m_tObsoleteFiles.Load(spContainer);		// loader also tries to delete files
		}

		// retrieve information about tasks to load
		std::vector<std::tuple<object_id_t, TSmartPath, TSmartPath> > vObjects;
		{
			boost::shared_lock<boost::shared_mutex> lock(m_lock);

			for (size_t stIndex = 0; stIndex < m_tTasks.GetCount(); ++stIndex)
			{
				TTaskInfoEntry& rEntry = m_tTasks.GetAt(stIndex);
				if (!rEntry.GetTask())
					vObjects.push_back(std::make_tuple(rEntry.GetObjectID(), rEntry.GetTaskSerializeLocation(), rEntry.GetTaskLogPath()));
			}
		}

		for (const auto& rInfo : vObjects)
		{
			IFeedbackHandlerPtr spHandler = m_spFeedbackFactory->Create();
			ISerializerPtr spSerializer;

			TString pathLog = std::get<2>(rInfo).ToWString();
			TString pathSerializer = std::get<1>(rInfo).ToWString();
			logger::TLogFileDataPtr spLogFileData = logger::TAsyncMultiLogger::GetInstance()->CreateLoggerData(pathLog.c_str(), m_spMultiLoggerConfig);

			try
			{
				spSerializer = m_spSerializerFactory->CreateTaskSerializer(spLogFileData, pathSerializer);
			}
			catch (const std::exception&)
			{
				// ignore the exception
			}

			if (!spSerializer)
				spSerializer = std::make_shared<TFakeFileSerializer>(std::get<1>(rInfo));

			TTaskPtr spTask = TTask::Load(spSerializer, spHandler, spLogFileData);

			boost::unique_lock<boost::shared_mutex> lock(m_lock);

			TTaskInfoEntry& rInfoEntry = m_tTasks.GetAtOid(std::get<0>(rInfo));
			rInfoEntry.SetTask(spTask);
		}
	}

	TSmartPath TTaskManager::CreateTaskLogPath(const TString& strTaskUuid) const
	{
		TSmartPath pathLog = m_pathLogDir + PathFromWString(TString(strTaskUuid + _T(".log")));
		return pathLog;
	}

	void TTaskManager::RemoveFilesForTask(const TTaskPtr& spTask)
	{
		m_tObsoleteFiles.DeleteObsoleteFile(spTask->GetSerializer()->GetLocation());

		logger::TLoggerPaths logPaths;
		spTask->GetLogPaths(logPaths);
		for(size_t stLogIndex = 0; stLogIndex != logPaths.GetCount(); ++stLogIndex)
		{
			m_tObsoleteFiles.DeleteObsoleteFile(PathFromString(logPaths.GetAt(stLogIndex)));
		}
	}
}
