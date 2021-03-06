// ============================================================================
//  Copyright (C) 2001-2016 by Jozef Starosczyk
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
#include "TLogFileData.h"
#include "TLoggerPaths.h"

namespace logger
{
	TLogFileData::TLogFileData() :
		m_spHasEntriesEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr), CloseHandle),
		m_spLoggerConfig(std::make_shared<TMultiLoggerConfig>()),
		m_spLogFile()
	{
	}

	TLogFileData::TLogFileData(PCTSTR pszLogPath, const TMultiLoggerConfigPtr& spLoggerConfig, const TLoggerRotationInfoPtr& spRotationInfo) :
		m_spHasEntriesEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr), CloseHandle),
		m_spLoggerConfig(spLoggerConfig),
		m_spLogFile(std::make_unique<internal::TLogFile>(pszLogPath, spRotationInfo))
	{
		if(m_spHasEntriesEvent.get() == INVALID_HANDLE_VALUE)
			throw std::runtime_error("Cannot create file data event");
		if(!spLoggerConfig)
			throw std::runtime_error("spLoggerConfig");
	}

	TMultiLoggerConfigPtr TLogFileData::GetMultiLoggerConfig() const
	{
		return m_spLoggerConfig;
	}

	void TLogFileData::GetAllLogPaths(TLoggerPaths& rLoggerPaths) const
	{
		rLoggerPaths.Clear();

		for(const std::wstring& strPath : m_spLogFile->GetRotatedLogs())
		{
			rLoggerPaths.Add(strPath.c_str());
		}

		rLoggerPaths.Add(m_spLogFile->GetLogPath().c_str());
	}

	TLoggerPaths TLogFileData::GetMainLogPath() const
	{
		TLoggerPaths loggerPaths;
		loggerPaths.Add(m_spLogFile->GetLogPath().c_str());
		return loggerPaths;
	}

	std::shared_ptr<void> TLogFileData::GetEntriesEvent() const
	{
		return m_spHasEntriesEvent;
	}

	void TLogFileData::DisableLogging()
	{
		m_bLoggingEnabled = false;
	}

	void TLogFileData::PushLogEntry(std::wstring strLine)
	{
		if(m_spLogFile && m_bLoggingEnabled)
		{
			boost::unique_lock<boost::shared_mutex> lock(m_mutex);
			m_listEntries.push_back(strLine);
			SetEvent(m_spHasEntriesEvent.get());
		}
	}

	void TLogFileData::StoreLogEntries()
	{
		if(m_spLogFile)
		{
			std::list<std::wstring> listEntries;

			{
				boost::unique_lock<boost::shared_mutex> lock(m_mutex);
				std::swap(listEntries, m_listEntries);
				ResetEvent(m_spHasEntriesEvent.get());
			}

			m_spLogFile->Write(listEntries);
		}
		else
			ResetEvent(m_spHasEntriesEvent.get());
	}

	void TLogFileData::CloseUnusedFile()
	{
		if(m_spLogFile)
			m_spLogFile->CloseIfUnused();
	}
}
