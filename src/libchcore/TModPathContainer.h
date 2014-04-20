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
#ifndef __TMODPATHCONTAINER_H__
#define __TMODPATHCONTAINER_H__

#include "libchcore.h"
#include <boost/container/flat_map.hpp>
#include "TModificationTracker.h"
#include "TPath.h"
#include "ISerializerContainer.h"
#include "TRemovedObjects.h"

BEGIN_CHCORE_NAMESPACE

class LIBCHCORE_API TModPathContainer
{
public:
	TModPathContainer();
	TModPathContainer(const TModPathContainer& rSrcContainer);
	~TModPathContainer();

	TModPathContainer& operator=(const TModPathContainer& rSrcContainer);
	TModPathContainer& operator=(const TPathContainer& rSrcContainer);

#pragma region Index-based interface
	void Add(const TSmartPath& spPath);

	const TSmartPath& GetAt(size_t stIndex) const;
	TSmartPath& GetAt(size_t stIndex);
	size_t GetOidAt(size_t stIndex) const;

	void SetAt(size_t stIndex, const TSmartPath& spPath);

	void DeleteAt(size_t stIndex);
#pragma endregion

#pragma region Object id-based interface
	const TSmartPath& GetAtOid(size_t stObjectID) const;
	TSmartPath& GetAtOid(size_t stObjectID);

	void SetByOid(size_t stObjectID, const TSmartPath& spPath);
	void DeleteOid(size_t stObjectID);
#pragma endregion

#pragma region Generic interface
	void Clear(bool bClearModificationsData);

	size_t GetCount() const;
	bool IsEmpty() const;
#pragma endregion

#pragma region Modifications management
	bool HasModifications() const;
	void ClearModifications();
#pragma endregion

#pragma region Serialization
	void Store(const ISerializerContainerPtr& spContainer) const;
	void Load(const ISerializerContainerPtr& spContainer);
#pragma endregion

private:
#pragma warning(push)
#pragma warning(disable: 4251)
	mutable TRemovedObjects m_setRemovedItems;
	typedef boost::container::flat_map<size_t, TModificationTracker<TSmartPath> > DataMap;
	DataMap m_vPaths;
#pragma warning(pop)
	size_t m_stNextObjectID;
};

END_CHCORE_NAMESPACE

#endif