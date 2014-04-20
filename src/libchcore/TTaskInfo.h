// ============================================================================
//  Copyright (C) 2001-2013 by Jozef Starosczyk
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
#ifndef __TTASKINFO_H__
#define __TTASKINFO_H__

#include "libchcore.h"
#include "TString.h"
#include <boost/shared_ptr.hpp>
#include "TPath.h"
#include "TaskID.h"
#include "ISerializerContainer.h"
#include "TRemovedObjects.h"
#include <bitset>
#include "TSharedModificationTracker.h"

BEGIN_CHCORE_NAMESPACE

class TTask;
typedef boost::shared_ptr<TTask> TTaskPtr;

class LIBCHCORE_API TTaskInfoEntry
{
public:
	enum EModifications
	{
		eMod_None = 0,
		eMod_Added,
		eMod_TaskPath,
		eMod_Order,

		eMod_Last
	};

public:
	TTaskInfoEntry();
	TTaskInfoEntry(taskid_t tTaskID, const TSmartPath& pathTask, int iOrder, const TTaskPtr& spTask);

	size_t GetObjectID() const;

	TSmartPath GetTaskSerializeLocation() const;
	void SetTaskSerializeLocation(const TSmartPath& pathTask);

	TTaskPtr GetTask() const;
	void SetTask(const TTaskPtr& spTask);

	int GetOrder() const;
	void SetOrder(int iOrder);

	void Store(const ISerializerContainerPtr& spContainer) const;
	static void InitLoader(const IColumnsDefinitionPtr& spColumnDefs);
	void Load(const ISerializerRowReaderPtr& spRowReader);

	void ResetModifications();

private:
#pragma warning(push)
#pragma warning(disable:4251)
	size_t m_stObjectID;
	typedef std::bitset<eMod_Last> Bitset;
	mutable std::bitset<eMod_Last> m_setModifications;
	TSharedModificationTracker<TSmartPath, Bitset, eMod_TaskPath> m_pathSerializeLocation;
	TSharedModificationTracker<int, Bitset, eMod_Order> m_iOrder;

	TTaskPtr m_spTask;
#pragma warning(pop)
};

class LIBCHCORE_API TTaskInfoContainer
{
public:
	TTaskInfoContainer();

	void Add(const TSmartPath& strPath, int iOrder, const TTaskPtr& spTask);
	void RemoveAt(size_t stIndex);

	TTaskInfoEntry& GetAt(size_t stIndex);
	const TTaskInfoEntry& GetAt(size_t stIndex) const;

	TTaskInfoEntry& GetAtOid(size_t stObjectID);

	bool GetByTaskID(taskid_t tTaskID, TTaskInfoEntry& rInfo) const;

	size_t GetCount() const;
	bool IsEmpty() const;

	void Clear();

	// modifications management
	void Store(const ISerializerContainerPtr& spContainer) const;
	void Load(const ISerializerContainerPtr& spContainer);

	void ClearModifications();

private:
#pragma warning(push)
#pragma warning(disable:4251)
	std::vector<TTaskInfoEntry> m_vTaskInfos;
	mutable TRemovedObjects m_setRemovedTasks;
#pragma warning(pop)
	size_t m_stLastObjectID;
};

END_CHCORE_NAMESPACE

#endif