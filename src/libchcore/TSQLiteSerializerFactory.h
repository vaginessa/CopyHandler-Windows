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
#ifndef __TSQLITESERIALIZERFACTORY_H__
#define __TSQLITESERIALIZERFACTORY_H__

#include "libchcore.h"
#include "TPath.h"
#include "ISerializerFactory.h"

namespace chcore
{
	class LIBCHCORE_API TSQLiteSerializerFactory : public ISerializerFactory
	{
	public:
		explicit TSQLiteSerializerFactory(const TSmartPath& pathSerializeDir);
		virtual ~TSQLiteSerializerFactory();

		virtual ISerializerPtr CreateTaskManagerSerializer(bool bForceRecreate = false) override;
		virtual ISerializerPtr CreateTaskSerializer(const TString& strNameHint = _T(""), bool bForceRecreate = false) override;

	private:
		TSmartPath m_pathSerializeDir;
	};

	typedef std::shared_ptr<TSQLiteSerializerFactory> TSQLiteSerializerFactoryPtr;
}

#endif
