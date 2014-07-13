// ============================================================================
//  Copyright (C) 2001-2014 by Jozef Starosczyk
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
#include "TModPathContainer.h"
#include "TCoreException.h"
#include "ErrorCodes.h"
#include "TPathContainer.h"
#include "ISerializerRowData.h"

BEGIN_CHCORE_NAMESPACE

// ============================================================================
/// TModPathContainer::TModPathContainer
/// @date 2009/11/30
///
/// @brief     Constructs an empty path container object.
// ============================================================================
TModPathContainer::TModPathContainer() :
	m_vPaths(),
	m_oidNextObjectID(1)
{
}

// ============================================================================
/// TModPathContainer::TModPathContainer
/// @date 2009/11/30
///
/// @brief     Constructs the path container object from another path container.
/// @param[in] rSrcContainer - path container to copy paths from.
// ============================================================================
TModPathContainer::TModPathContainer(const TModPathContainer& rSrcContainer) :
	m_vPaths(rSrcContainer.m_vPaths),
	m_oidNextObjectID(rSrcContainer.m_oidNextObjectID)
{
}

// ============================================================================
/// TModPathContainer::~TModPathContainer
/// @date 2009/11/30
///
/// @brief     Destructs this path container object.
// ============================================================================
TModPathContainer::~TModPathContainer()
{
}

// ============================================================================
/// TModPathContainer::operator=
/// @date 2009/11/30
///
/// @brief     Assigns another path container object to this one.
/// @param[in] rSrcContainer - container with paths to copy from.
/// @return    Reference to this object.
// ============================================================================
TModPathContainer& TModPathContainer::operator=(const TModPathContainer& rSrcContainer)
{
	if(this != &rSrcContainer)
	{
		m_vPaths = rSrcContainer.m_vPaths;
		m_oidNextObjectID = rSrcContainer.m_oidNextObjectID;
	}

	return *this;
}

TModPathContainer& TModPathContainer::operator=(const TPathContainer& rSrcContainer)
{
	Clear(true);

	for(size_t stIndex = 0; stIndex < rSrcContainer.GetCount(); ++stIndex)
	{
		m_vPaths.insert(std::make_pair(m_oidNextObjectID++, TModificationTracker<TSmartPath>(rSrcContainer.GetAt(stIndex), true)));
	}

	return *this;
}

// ============================================================================
/// TModPathContainer::Add
/// @date 2009/11/30
///
/// @brief     Adds a path to the end of list.
/// @param[in] spPath - path to be added.
// ============================================================================
void TModPathContainer::Add(const TSmartPath& spPath)
{
	m_vPaths.insert(std::make_pair(m_oidNextObjectID++, TModificationTracker<TSmartPath>(spPath, true)));
}

// ============================================================================
/// TModPathContainer::GetAt
/// @date 2009/11/30
///
/// @brief     Retrieves path at specified index.
/// @param[in] stIndex - index at which to retrieve item.
/// @return    Reference to the path object.
// ============================================================================
const TSmartPath& TModPathContainer::GetAt(size_t stIndex) const
{
	if(stIndex > m_vPaths.size())
		THROW_CORE_EXCEPTION(eErr_BoundsExceeded);

	DataMap::const_iterator iter = m_vPaths.cbegin() + stIndex;
	return iter->second;
}

// ============================================================================
/// TModPathContainer::GetAt
/// @date 2009/11/30
///
/// @brief     Retrieves path at specified index.
/// @param[in] stIndex - index at which to retrieve item.
/// @return    Reference to the path object.
// ============================================================================
TSmartPath& TModPathContainer::GetAt(size_t stIndex)
{
	if(stIndex > m_vPaths.size())
		THROW_CORE_EXCEPTION(eErr_BoundsExceeded);

	DataMap::iterator iter = m_vPaths.begin() + stIndex;
	return iter->second.Modify();
}

object_id_t TModPathContainer::GetOidAt(size_t stIndex) const
{
	if(stIndex > m_vPaths.size())
		THROW_CORE_EXCEPTION(eErr_BoundsExceeded);

	DataMap::const_iterator iter = m_vPaths.begin() + stIndex;
	return iter->first;
}

// ============================================================================
/// chcore::TModPathContainer::SetAt
/// @date 2009/11/30
///
/// @brief     Sets a path at a specified index.
/// @param[in] stIndex - index at which to set the path.
/// @param[in] spPath -  path to be set.
// ============================================================================
void TModPathContainer::SetAt(size_t stIndex, const TSmartPath& spPath)
{
	if(stIndex > m_vPaths.size())
		THROW_CORE_EXCEPTION(eErr_BoundsExceeded);

	DataMap::iterator iter = m_vPaths.begin() + stIndex;
	iter->second = spPath;
}

// ============================================================================
/// chcore::TModPathContainer::DeleteAt
/// @date 2009/11/30
///
/// @brief     Removes a path from container at specified index.
/// @param[in] stIndex - index at which to delete.
// ============================================================================
void TModPathContainer::DeleteAt(size_t stIndex)
{
	if(stIndex > m_vPaths.size())
		THROW_CORE_EXCEPTION(eErr_BoundsExceeded);

	DataMap::iterator iterDel = m_vPaths.begin() + stIndex;
	m_setRemovedItems.Add(iterDel->first);
	m_vPaths.erase(iterDel);
}

// ============================================================================
/// chcore::TModPathContainer::Clear
/// @date 2009/11/30
///
/// @brief     Removes all paths from this container.
// ============================================================================
void TModPathContainer::Clear(bool bClearModificationsData)
{
	if(!bClearModificationsData)
	{
		for(DataMap::iterator iterDel = m_vPaths.begin(); iterDel != m_vPaths.end(); ++iterDel)
		{
			m_setRemovedItems.Add(iterDel->first);
		}
	}
	else
	{
		m_setRemovedItems.Clear();
		m_oidNextObjectID = 1;
	}

	m_vPaths.clear();
}

// ============================================================================
/// chcore::TModPathContainer::GetCount
/// @date 2009/11/30
///
/// @brief     Retrieves count of elements in the container.
/// @return    Count of elements.
// ============================================================================
size_t TModPathContainer::GetCount() const
{
	return m_vPaths.size();
}

// ============================================================================
/// chcore::TModPathContainer::GetCount
/// @date 2010/10/12
///
/// @brief     Retrieves info if this container is empty.
/// @return    True if empty, false otherwise.
// ============================================================================
bool TModPathContainer::IsEmpty() const
{
	return m_vPaths.empty();
}

const TSmartPath& TModPathContainer::GetAtOid(object_id_t oidObjectID) const
{
	return m_vPaths.at(oidObjectID);
}

TSmartPath& TModPathContainer::GetAtOid(object_id_t oidObjectID)
{
	return m_vPaths.at(oidObjectID).Modify();
}

void TModPathContainer::SetByOid(object_id_t oidObjectID, const TSmartPath& spPath)
{
	DataMap::iterator iterFnd = m_vPaths.find(oidObjectID);
	if(iterFnd != m_vPaths.end())
		iterFnd->second = spPath;
	else
		m_vPaths.insert(std::make_pair(oidObjectID, TModificationTracker<TSmartPath>(spPath, true)));
}

void TModPathContainer::DeleteOid(object_id_t oidObjectID)
{
	m_vPaths.erase(oidObjectID);
	m_setRemovedItems.Add(oidObjectID);
}

bool TModPathContainer::HasModifications() const
{
	if(!m_setRemovedItems.IsEmpty())
		return true;

	for(DataMap::const_iterator iterDel = m_vPaths.begin(); iterDel != m_vPaths.end(); ++iterDel)
	{
		if(iterDel->second.IsModified() || iterDel->second.IsAdded())
			return true;
	}

	return false;
}

void TModPathContainer::ClearModifications()
{
	m_setRemovedItems.Clear();

	for(DataMap::iterator iterDel = m_vPaths.begin(); iterDel != m_vPaths.end(); ++iterDel)
	{
		iterDel->second.ClearModifications();
	}
}

void TModPathContainer::Store(const ISerializerContainerPtr& spContainer) const
{
	InitColumns(spContainer);

	// delete items first
	spContainer->DeleteRows(m_setRemovedItems);
	m_setRemovedItems.Clear();

	// add/modify
	for(DataMap::const_iterator iterPath = m_vPaths.begin(); iterPath != m_vPaths.end(); ++iterPath)
	{
		const TModificationTracker<TSmartPath>& rItem = iterPath->second;

		if(rItem.IsModified())
		{
			ISerializerRowData& rRow = spContainer->GetRow(iterPath->first, rItem.IsAdded());
			rRow.SetValue(_T("path"), rItem);

			rItem.ClearModifications();
		}
		else
			continue;
	}
}

void TModPathContainer::Load(const ISerializerContainerPtr& spContainer)
{
	m_setRemovedItems.Clear();
	m_vPaths.clear();
	m_oidNextObjectID = 1;

	InitColumns(spContainer);

	ISerializerRowReaderPtr spRowReader = spContainer->GetRowReader();
	while(spRowReader->Next())
	{
		object_id_t oidObjectID = 0;
		TSmartPath path;

		spRowReader->GetValue(_T("id"), oidObjectID);
		spRowReader->GetValue(_T("path"), path);

		m_vPaths.insert(std::make_pair(oidObjectID, TModificationTracker<TSmartPath>(path, false)));
	}

	ClearModifications();
}

void TModPathContainer::InitColumns(const ISerializerContainerPtr& spContainer) const
{
	IColumnsDefinition& rColumns = spContainer->GetColumnsDefinition();
	if(rColumns.IsEmpty())
	{
		rColumns.AddColumn(_T("id"), ColumnType<object_id_t>::value);
		rColumns.AddColumn(_T("path"), IColumnsDefinition::eType_path);
	}
}

END_CHCORE_NAMESPACE
