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
#include "TOverlappedReaderWriter.h"
#include "TOverlappedDataBuffer.h"
#include "TCoreException.h"
#include "ErrorCodes.h"
#include <array>
#include <atltrace.h>

namespace chcore
{
	TOverlappedReaderWriter::TOverlappedReaderWriter(const logger::TLogFileDataPtr& spLogFileData, const TOverlappedMemoryPoolPtr& spMemoryPool,
		unsigned long long ullFilePos, DWORD dwChunkSize) :
		m_spLog(logger::MakeLogger(spLogFileData, L"DataBuffer")),
		m_spMemoryPool(spMemoryPool),
		m_tReader(spLogFileData, spMemoryPool->GetBufferList(), ullFilePos, dwChunkSize),
		m_tWriter(spLogFileData, m_tReader.GetFinishedQueue(), ullFilePos),
		m_eventAllBuffersAccountedFor(true, true)
	{
		if(!spMemoryPool)
			throw TCoreException(eErr_InvalidArgument, L"spMemoryPool", LOCATION);
	}

	TOverlappedReaderWriter::~TOverlappedReaderWriter()
	{
	}

	TOverlappedDataBuffer* TOverlappedReaderWriter::GetEmptyBuffer()
	{
		TOverlappedDataBuffer* pBuffer = m_tReader.GetEmptyBuffer();
		if(pBuffer)
		{
			pBuffer->SetParam(this);
			UpdateAllBuffersAccountedFor();
		}

		return pBuffer;
	}

	void TOverlappedReaderWriter::AddEmptyBuffer(TOverlappedDataBuffer* pBuffer, bool bKeepPosition)
	{
		if(!pBuffer)
			throw TCoreException(eErr_InvalidPointer, L"pBuffer", LOCATION);

		LOG_TRACE(m_spLog) << L"Queuing buffer " << pBuffer << L" as empty; buffer-order: " << pBuffer->GetFilePosition() <<
			L", requested-data-size: " << pBuffer->GetRequestedDataSize() <<
			L", real-data-size: " << pBuffer->GetRealDataSize() <<
			L", file-position: " << pBuffer->GetFilePosition() <<
			L", error-code: " << pBuffer->GetErrorCode() <<
			L", status-code: " << pBuffer->GetStatusCode() <<
			L", is-last-part: " << pBuffer->IsLastPart();

		m_tReader.AddEmptyBuffer(pBuffer, bKeepPosition);
		UpdateAllBuffersAccountedFor();
	}

	TOverlappedDataBuffer* TOverlappedReaderWriter::GetFailedReadBuffer()
	{
		TOverlappedDataBuffer* pBuffer = m_tReader.GetFailedReadBuffer();

		if(pBuffer)
			UpdateAllBuffersAccountedFor();

		return pBuffer;
	}

	void TOverlappedReaderWriter::AddFailedReadBuffer(TOverlappedDataBuffer* pBuffer)
	{
		if(!pBuffer)
			throw TCoreException(eErr_InvalidPointer, L"pBuffer", LOCATION);

		LOG_TRACE(m_spLog) << L"Queuing buffer " << pBuffer << L" as failed-read; buffer-order: " << pBuffer->GetFilePosition() <<
			L", requested-data-size: " << pBuffer->GetRequestedDataSize() <<
			L", real-data-size: " << pBuffer->GetRealDataSize() <<
			L", file-position: " << pBuffer->GetFilePosition() <<
			L", error-code: " << pBuffer->GetErrorCode() <<
			L", status-code: " << pBuffer->GetStatusCode() <<
			L", is-last-part: " << pBuffer->IsLastPart();

		m_tReader.AddFailedReadBuffer(pBuffer);
		UpdateAllBuffersAccountedFor();
	}

	TOverlappedDataBuffer* TOverlappedReaderWriter::GetWriteBuffer()
	{
		TOverlappedDataBuffer* pBuffer = m_tWriter.GetWriteBuffer();

		if(pBuffer)
		{
			pBuffer->SetParam(this);

			UpdateAllBuffersAccountedFor();
		}

		return pBuffer;
	}

	void TOverlappedReaderWriter::AddFinishedReadBuffer(TOverlappedDataBuffer* pBuffer)
	{
		if(!pBuffer)
			throw TCoreException(eErr_InvalidPointer, L"pBuffer", LOCATION);

		LOG_TRACE(m_spLog) << L"Queuing buffer " << pBuffer << L" as finished-read; buffer-order: " << pBuffer->GetFilePosition() <<
			L", requested-data-size: " << pBuffer->GetRequestedDataSize() <<
			L", real-data-size: " << pBuffer->GetRealDataSize() <<
			L", file-position: " << pBuffer->GetFilePosition() <<
			L", error-code: " << pBuffer->GetErrorCode() <<
			L", status-code: " << pBuffer->GetStatusCode() <<
			L", is-last-part: " << pBuffer->IsLastPart();
		m_tReader.AddFullBuffer(pBuffer);

		UpdateAllBuffersAccountedFor();
	}

	TOverlappedDataBuffer* TOverlappedReaderWriter::GetFailedWriteBuffer()
	{
		TOverlappedDataBuffer* pBuffer = m_tWriter.GetFailedWriteBuffer();
		if(pBuffer)
			UpdateAllBuffersAccountedFor();

		return pBuffer;
	}

	void TOverlappedReaderWriter::AddFailedWriteBuffer(TOverlappedDataBuffer* pBuffer)
	{
		if(!pBuffer)
			throw TCoreException(eErr_InvalidPointer, L"pBuffer", LOCATION);

		LOG_TRACE(m_spLog) << L"Queuing buffer " << pBuffer << L" as failed-write; buffer-order: " << pBuffer->GetFilePosition() <<
			L", requested-data-size: " << pBuffer->GetRequestedDataSize() <<
			L", real-data-size: " << pBuffer->GetRealDataSize() <<
			L", file-position: " << pBuffer->GetFilePosition() <<
			L", error-code: " << pBuffer->GetErrorCode() <<
			L", status-code: " << pBuffer->GetStatusCode() <<
			L", is-last-part: " << pBuffer->IsLastPart();

		// overwrite error code (to avoid treating the buffer as failed read)
		pBuffer->SetErrorCode(ERROR_SUCCESS);
		m_tWriter.AddFailedWriteBuffer(pBuffer);

		UpdateAllBuffersAccountedFor();
	}

	TOverlappedDataBuffer* TOverlappedReaderWriter::GetFinishedWriteBuffer()
	{
		TOverlappedDataBuffer* pBuffer = m_tWriter.GetFinishedBuffer();

		if(pBuffer)
			UpdateAllBuffersAccountedFor();

		return pBuffer;
	}


	void TOverlappedReaderWriter::AddFinishedWriteBuffer(TOverlappedDataBuffer* pBuffer)
	{
		if (!pBuffer)
			throw TCoreException(eErr_InvalidPointer, L"pBuffer", LOCATION);

		LOG_TRACE(m_spLog) << L"Queuing buffer " << pBuffer << L" as finished-write; buffer-order: " << pBuffer->GetFilePosition() <<
			L", requested-data-size: " << pBuffer->GetRequestedDataSize() <<
			L", real-data-size: " << pBuffer->GetRealDataSize() <<
			L", file-position: " << pBuffer->GetFilePosition() <<
			L", error-code: " << pBuffer->GetErrorCode() <<
			L", status-code: " << pBuffer->GetStatusCode() <<
			L", is-last-part: " << pBuffer->IsLastPart();

		m_tWriter.AddFinishedBuffer(pBuffer);

		UpdateAllBuffersAccountedFor();
	}

	void TOverlappedReaderWriter::MarkFinishedBufferAsComplete(TOverlappedDataBuffer* pBuffer)
	{
		if(!pBuffer)
			throw TCoreException(eErr_InvalidPointer, L"pBuffer", LOCATION);

		LOG_TRACE(m_spLog) << L"Marking buffer " << pBuffer << L" as finalized-write; buffer-order: " << pBuffer->GetFilePosition() <<
			L", requested-data-size: " << pBuffer->GetRequestedDataSize() <<
			L", real-data-size: " << pBuffer->GetRealDataSize() <<
			L", file-position: " << pBuffer->GetFilePosition() <<
			L", error-code: " << pBuffer->GetErrorCode() <<
			L", status-code: " << pBuffer->GetStatusCode() <<
			L", is-last-part: " << pBuffer->IsLastPart();

		m_tWriter.MarkAsFinalized(pBuffer);
	}

	void TOverlappedReaderWriter::UpdateAllBuffersAccountedFor()
	{
		size_t stCurrentBuffers = m_spMemoryPool->GetAvailableBufferCount() + m_tReader.GetBufferCount() + m_tWriter.GetBufferCount();
		if (stCurrentBuffers == m_spMemoryPool->GetTotalBufferCount())
			m_eventAllBuffersAccountedFor.SetEvent();
		else
			m_eventAllBuffersAccountedFor.ResetEvent();
	}

	void TOverlappedReaderWriter::WaitForMissingBuffersAndResetState(HANDLE hKillEvent)
	{
		enum { eKillThread = 0, eAllBuffersReturned, eHandleCount };
		std::array<HANDLE, eHandleCount> arrHandles = { hKillEvent, m_eventAllBuffersAccountedFor.Handle() };

		bool bExit = false;
		while (!bExit)
		{
			DWORD dwResult = WaitForMultipleObjectsEx(eHandleCount, arrHandles.data(), false, INFINITE, true);
			switch (dwResult)
			{
			case STATUS_USER_APC:
				break;

			case WAIT_OBJECT_0 + eAllBuffersReturned:
				bExit = true;
				break;

			case WAIT_OBJECT_0 + eKillThread:
				bExit = true;
				break;
			}
		}

		m_tReader.ReleaseBuffers(m_spMemoryPool->GetBufferList());
		m_tWriter.ReleaseBuffers(m_spMemoryPool->GetBufferList());
	}
}
