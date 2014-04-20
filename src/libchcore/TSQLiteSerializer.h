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
#ifndef __TSQLITESERIALIZER_H__
#define __TSQLITESERIALIZER_H__

#include "libchcore.h"
#include <map>
#include "ISerializer.h"
#include "TSQLiteDatabase.h"
#include "TString.h"
#include "ISerializerContainer.h"
#include "TPath.h"
#include "TSQLiteSerializerContainer.h"
#include "ISQLiteSerializerSchema.h"

BEGIN_CHCORE_NAMESPACE

class LIBCHCORE_API TSQLiteSerializer : public ISerializer
{
public:
	TSQLiteSerializer(const TSmartPath& pathDB, const ISerializerSchemaPtr& spSchema);

	virtual TSmartPath GetLocation() const;

	virtual ISerializerContainerPtr GetContainer(const TString& strContainerName);
	virtual void Flush();

private:
#pragma warning(push)
#pragma warning(disable: 4251)
	sqlite::TSQLiteDatabasePtr m_spDatabase;
	ISerializerSchemaPtr m_spSchema;

	typedef std::map<TString, TSQLiteSerializerContainerPtr> ContainerMap;
	ContainerMap m_mapContainers;
#pragma warning(pop)
};

typedef boost::shared_ptr<TSQLiteSerializer> TSQLiteSerializerPtr;

END_CHCORE_NAMESPACE

#endif