#include "stdafx.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "../TOverlappedDataBufferQueue.h"
#include "../TOverlappedDataBuffer.h"
#include "../TCoreException.h"

using namespace chcore;

#define EXPECT_TIMEOUT(handle)\
	{\
		DWORD dwResult = WaitForSingleObject(handle, 0); \
		EXPECT_EQ(WAIT_TIMEOUT, dwResult); \
	}

#define EXPECT_SIGNALED(handle)\
	{\
		DWORD dwResult = WaitForSingleObject(handle, 0); \
		EXPECT_EQ(WAIT_OBJECT_0 + 0, dwResult); \
	}


TEST(TOverlappedDataBufferQueueTests, DefaultConstructor_SanityTest)
{
	TOverlappedDataBufferQueue queue;

	EXPECT_EQ(nullptr, queue.GetEmptyBuffer());
	EXPECT_EQ(nullptr, queue.GetFullBuffer());
	EXPECT_EQ(nullptr, queue.GetFinishedBuffer());

	EXPECT_NE(nullptr, queue.GetEventReadPossibleHandle());
	EXPECT_NE(nullptr, queue.GetEventWritePossibleHandle());
	EXPECT_NE(nullptr, queue.GetEventWriteFinishedHandle());

	EXPECT_TIMEOUT(queue.GetEventReadPossibleHandle());
	EXPECT_TIMEOUT(queue.GetEventWritePossibleHandle());
	EXPECT_TIMEOUT(queue.GetEventWriteFinishedHandle());

	EXPECT_FALSE(queue.IsDataSourceFinished());
	EXPECT_FALSE(queue.IsDataWritingFinished());

	EXPECT_EQ(0, queue.GetTotalBufferCount());
	EXPECT_EQ(0, queue.GetSingleBufferSize());
}

TEST(TOverlappedDataBufferQueueTests, AllocatingConstructor_SanityTest)
{
	TOverlappedDataBufferQueue queue(3, 32768);

	EXPECT_NE(nullptr, queue.GetEmptyBuffer());
	EXPECT_EQ(nullptr, queue.GetFullBuffer());
	EXPECT_EQ(nullptr, queue.GetFinishedBuffer());

	EXPECT_NE(nullptr, queue.GetEventReadPossibleHandle());
	EXPECT_NE(nullptr, queue.GetEventWritePossibleHandle());
	EXPECT_NE(nullptr, queue.GetEventWriteFinishedHandle());

	EXPECT_SIGNALED(queue.GetEventReadPossibleHandle());
	EXPECT_TIMEOUT(queue.GetEventWritePossibleHandle());
	EXPECT_TIMEOUT(queue.GetEventWriteFinishedHandle());

	EXPECT_FALSE(queue.IsDataSourceFinished());
	EXPECT_FALSE(queue.IsDataWritingFinished());
}

TEST(TOverlappedDataBufferQueueTests, AllocatingConstructor_CheckBufferSizes)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	EXPECT_EQ(3, queue.GetTotalBufferCount());
	EXPECT_EQ(32768, queue.GetSingleBufferSize());

	EXPECT_EQ(32768, pBuffers[0]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[1]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[2]->GetBufferSize());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
TEST(TOverlappedDataBufferQueueTests, ReinitializeBuffer_FailsWithBuffersInUse)
{
	TOverlappedDataBufferQueue queue(3, 32768);

	queue.GetEmptyBuffer();

	EXPECT_THROW(queue.ReinitializeBuffers(3, 65536), TCoreException);
}

TEST(TOverlappedDataBufferQueueTests, ReinitializeBuffer_ZeroLengthBuffers)
{
	TOverlappedDataBufferQueue queue(3, 32768);

	EXPECT_THROW(queue.ReinitializeBuffers(3, 0), TCoreException);
}

TEST(TOverlappedDataBufferQueueTests, ReinitializeBuffer_SameSizeSameCount)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	queue.ReinitializeBuffers(3, 32768);

	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	EXPECT_EQ(3, queue.GetTotalBufferCount());
	EXPECT_EQ(32768, queue.GetSingleBufferSize());

	EXPECT_EQ(32768, pBuffers[0]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[1]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[2]->GetBufferSize());
}

TEST(TOverlappedDataBufferQueueTests, ReinitializeBuffer_IncreaseSize)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	queue.ReinitializeBuffers(3, 65536);

	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	EXPECT_EQ(3, queue.GetTotalBufferCount());
	EXPECT_EQ(65536, queue.GetSingleBufferSize());

	EXPECT_EQ(65536, pBuffers[0]->GetBufferSize());
	EXPECT_EQ(65536, pBuffers[1]->GetBufferSize());
	EXPECT_EQ(65536, pBuffers[2]->GetBufferSize());
}

TEST(TOverlappedDataBufferQueueTests, ReinitializeBuffer_DecreaseSize)
{
	TOverlappedDataBufferQueue queue(3, 65536);
	queue.ReinitializeBuffers(3, 32768);

	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	EXPECT_EQ(3, queue.GetTotalBufferCount());
	EXPECT_EQ(32768, queue.GetSingleBufferSize());

	EXPECT_EQ(32768, pBuffers[0]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[1]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[2]->GetBufferSize());
}

TEST(TOverlappedDataBufferQueueTests, ReinitializeBuffer_IncreaseCount)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	queue.ReinitializeBuffers(5, 32768);

	EXPECT_EQ(5, queue.GetTotalBufferCount());
	EXPECT_EQ(32768, queue.GetSingleBufferSize());

	TOverlappedDataBuffer* pBuffers[5] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	EXPECT_EQ(32768, pBuffers[0]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[1]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[2]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[3]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[4]->GetBufferSize());
}

TEST(TOverlappedDataBufferQueueTests, ReinitializeBuffer_DecreaseCount)
{
	TOverlappedDataBufferQueue queue(5, 32768);
	queue.ReinitializeBuffers(3, 32768);

	EXPECT_EQ(3, queue.GetTotalBufferCount());
	EXPECT_EQ(32768, queue.GetSingleBufferSize());

	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	EXPECT_EQ(32768, pBuffers[0]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[1]->GetBufferSize());
	EXPECT_EQ(32768, pBuffers[2]->GetBufferSize());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
TEST(TOverlappedDataBufferQueueTests, GetEmptyBuffer)
{
	TOverlappedDataBufferQueue queue(3, 32768);

	EXPECT_SIGNALED(queue.GetEventReadPossibleHandle());

	EXPECT_NE(nullptr, queue.GetEmptyBuffer());
	EXPECT_SIGNALED(queue.GetEventReadPossibleHandle());

	EXPECT_NE(nullptr, queue.GetEmptyBuffer());
	EXPECT_SIGNALED(queue.GetEventReadPossibleHandle());

	EXPECT_NE(nullptr, queue.GetEmptyBuffer());
	EXPECT_TIMEOUT(queue.GetEventReadPossibleHandle());

	EXPECT_EQ(nullptr, queue.GetEmptyBuffer());
}

TEST(TOverlappedDataBufferQueueTests, AddEmptyBuffer)
{
	TOverlappedDataBufferQueue queue(3, 32768);

	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	EXPECT_TIMEOUT(queue.GetEventReadPossibleHandle());

	queue.AddEmptyBuffer(pBuffers[0]);
	EXPECT_SIGNALED(queue.GetEventReadPossibleHandle());

	queue.AddEmptyBuffer(pBuffers[1]);
	EXPECT_SIGNALED(queue.GetEventReadPossibleHandle());

	queue.AddEmptyBuffer(pBuffers[2]);
	EXPECT_SIGNALED(queue.GetEventReadPossibleHandle());
}

TEST(TOverlappedDataBufferQueueTests, AddEmptyBuffer_Null)
{
	TOverlappedDataBufferQueue queue(3, 32768);

	EXPECT_THROW(queue.AddEmptyBuffer(nullptr), TCoreException);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
TEST(TOverlappedDataBufferQueueTests, AddFullBuffer_GetFullBuffer)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffer = queue.GetEmptyBuffer();

	queue.AddFullBuffer(pBuffer);
	EXPECT_SIGNALED(queue.GetEventWritePossibleHandle());

	pBuffer = queue.GetFullBuffer();
	EXPECT_TIMEOUT(queue.GetEventWritePossibleHandle());
}

TEST(TOverlappedDataBufferQueueTests, GetFullBuffer_WrongOrder)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	queue.AddFullBuffer(pBuffers[1]);
	EXPECT_EQ(nullptr, queue.GetFullBuffer());

	queue.AddFullBuffer(pBuffers[2]);
	EXPECT_EQ(nullptr, queue.GetFullBuffer());

	queue.AddFullBuffer(pBuffers[0]);
	EXPECT_NE(nullptr, queue.GetFullBuffer());
}

TEST(TOverlappedDataBufferQueueTests, AddFullBuffer_HandlingSrcEof)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	pBuffers[1]->SetLastPart(true);

	queue.AddFullBuffer(pBuffers[0]);
	EXPECT_FALSE(queue.IsDataSourceFinished());

	queue.AddFullBuffer(pBuffers[1]);
	EXPECT_TRUE(queue.IsDataSourceFinished());
}

TEST(TOverlappedDataBufferQueueTests, AddFullBuffer_HandlingDstEof)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	pBuffers[2]->SetLastPart(true);

	queue.AddFullBuffer(pBuffers[0]);
	queue.AddFullBuffer(pBuffers[1]);
	queue.AddFullBuffer(pBuffers[2]);

	TOverlappedDataBuffer* pBuffer = queue.GetFullBuffer();
	EXPECT_FALSE(queue.IsDataWritingFinished());
	pBuffer = queue.GetFullBuffer();
	EXPECT_FALSE(queue.IsDataWritingFinished());

	// getting the last buffer (marked as eof) causes setting the data-writing-finished flag
	pBuffer = queue.GetFullBuffer();
	EXPECT_TRUE(queue.IsDataWritingFinished());
}

TEST(TOverlappedDataBufferQueueTests, AddFullBuffer_Null)
{
	TOverlappedDataBufferQueue queue(3, 32768);

	EXPECT_THROW(queue.AddFullBuffer(nullptr), TCoreException);
}

TEST(TOverlappedDataBufferQueueTests, AddFullBuffer_SameBufferTwice)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffer = queue.GetEmptyBuffer();

	pBuffer->InitForRead(0, 1280);
	pBuffer->SetBytesTransferred(1230);
	pBuffer->SetStatusCode(0);

	queue.AddFullBuffer(pBuffer);
	EXPECT_THROW(queue.AddFullBuffer(pBuffer), TCoreException);
}

TEST(TOverlappedDataBufferQueueTests, GetFullBuffer_AddFullBuffer_OutOfOrder)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	pBuffers[0]->InitForRead(0, 1000);
	pBuffers[0]->SetBytesTransferred(1000);
	pBuffers[0]->SetStatusCode(0);

	pBuffers[1]->InitForRead(0, 1200);
	pBuffers[1]->SetBytesTransferred(1200);
	pBuffers[1]->SetStatusCode(0);

	pBuffers[2]->InitForRead(0, 1400);
	pBuffers[2]->SetBytesTransferred(800);
	pBuffers[2]->SetStatusCode(0);
	pBuffers[2]->SetLastPart(true);

	EXPECT_TIMEOUT(queue.GetEventWritePossibleHandle());

	queue.AddFullBuffer(pBuffers[1]);
	EXPECT_TIMEOUT(queue.GetEventWritePossibleHandle());

	queue.AddFullBuffer(pBuffers[2]);
	EXPECT_TIMEOUT(queue.GetEventWritePossibleHandle());

	queue.AddFullBuffer(pBuffers[0]);
	EXPECT_SIGNALED(queue.GetEventWritePossibleHandle());
}

///////////////////////////////////////////////////////////////////////////////////////////////////
TEST(TOverlappedDataBufferQueueTests, AddFinishedBuffer_OutOfOrder_Signals)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	pBuffers[0]->InitForRead(0, 1000);
	pBuffers[0]->SetBytesTransferred(1000);
	pBuffers[0]->SetStatusCode(0);

	pBuffers[1]->InitForRead(0, 1200);
	pBuffers[1]->SetBytesTransferred(1200);
	pBuffers[1]->SetStatusCode(0);

	pBuffers[2]->InitForRead(0, 1400);
	pBuffers[2]->SetBytesTransferred(800);
	pBuffers[2]->SetStatusCode(0);
	pBuffers[2]->SetLastPart(true);

	queue.AddFinishedBuffer(pBuffers[1]);
	EXPECT_TIMEOUT(queue.GetEventWriteFinishedHandle());
	queue.AddFinishedBuffer(pBuffers[2]);
	EXPECT_TIMEOUT(queue.GetEventWriteFinishedHandle());
	queue.AddFinishedBuffer(pBuffers[0]);
	EXPECT_SIGNALED(queue.GetEventWriteFinishedHandle());
}

TEST(TOverlappedDataBufferQueueTests, GetFinishedBuffer_Signals)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	pBuffers[0]->InitForRead(0, 1000);
	pBuffers[0]->SetBytesTransferred(1000);
	pBuffers[0]->SetStatusCode(0);

	pBuffers[1]->InitForRead(0, 1200);
	pBuffers[1]->SetBytesTransferred(1200);
	pBuffers[1]->SetStatusCode(0);

	pBuffers[2]->InitForRead(0, 1400);
	pBuffers[2]->SetBytesTransferred(800);
	pBuffers[2]->SetStatusCode(0);
	pBuffers[2]->SetLastPart(true);

	queue.AddFinishedBuffer(pBuffers[1]);
	queue.AddFinishedBuffer(pBuffers[2]);
	queue.AddFinishedBuffer(pBuffers[0]);

	queue.GetFinishedBuffer();
	EXPECT_SIGNALED(queue.GetEventWriteFinishedHandle());
	queue.GetFinishedBuffer();
	EXPECT_SIGNALED(queue.GetEventWriteFinishedHandle());
	queue.GetFinishedBuffer();
	EXPECT_TIMEOUT(queue.GetEventWriteFinishedHandle());
}

TEST(TOverlappedDataBufferQueueTests, GetFinishedBuffer_WrongOrder)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	pBuffers[0]->InitForRead(0, 1000);
	pBuffers[0]->SetBytesTransferred(1000);
	pBuffers[0]->SetStatusCode(0);

	pBuffers[1]->InitForRead(0, 1200);
	pBuffers[1]->SetBytesTransferred(1200);
	pBuffers[1]->SetStatusCode(0);

	pBuffers[2]->InitForRead(0, 1400);
	pBuffers[2]->SetBytesTransferred(800);
	pBuffers[2]->SetStatusCode(0);
	pBuffers[2]->SetLastPart(true);

	queue.AddFinishedBuffer(pBuffers[1]);
	EXPECT_EQ(nullptr, queue.GetFinishedBuffer());

	queue.AddFinishedBuffer(pBuffers[2]);
	EXPECT_EQ(nullptr, queue.GetFinishedBuffer());

	queue.AddFinishedBuffer(pBuffers[0]);
	EXPECT_NE(nullptr, queue.GetFinishedBuffer());
}

TEST(TOverlappedDataBufferQueueTests, AddFinishedBuffer_Null)
{
	TOverlappedDataBufferQueue queue(3, 32768);

	EXPECT_THROW(queue.AddFinishedBuffer(nullptr), TCoreException);
}

TEST(TOverlappedDataBufferQueueTests, AddFinishedBuffer_SameBufferTwice)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffer = queue.GetEmptyBuffer();
	queue.AddFinishedBuffer(pBuffer);
	EXPECT_THROW(queue.AddFinishedBuffer(pBuffer), TCoreException);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
TEST(TOverlappedDataBufferQueueTests, DataSourceChanged_CleanupBuffers)
{
	TOverlappedDataBufferQueue queue(3, 32768);
	TOverlappedDataBuffer* pBuffers[3] = { queue.GetEmptyBuffer(), queue.GetEmptyBuffer(), queue.GetEmptyBuffer() };

	pBuffers[0]->SetLastPart(true);
	pBuffers[1]->SetLastPart(true);
	pBuffers[2]->SetLastPart(true);

	queue.AddFullBuffer(pBuffers[1]);
	queue.AddFullBuffer(pBuffers[2]);
	queue.AddFullBuffer(pBuffers[0]);

	// this tests if the buffers are properly cleaned up - if they're not, DataSourceChanged() throws an exception
	EXPECT_NO_THROW(queue.DataSourceChanged());
}

TEST(TOverlappedDataBufferQueueTests, DataSourceChanged_InvalidBufferCount)
{
	TOverlappedDataBufferQueue queue(3, 32768);

	queue.GetEmptyBuffer();

	// this tests if the buffers are properly cleaned up - if they're not, DataSourceChanged() throws an exception
	EXPECT_THROW(queue.DataSourceChanged(), TCoreException);
}