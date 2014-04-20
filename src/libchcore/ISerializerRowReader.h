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
#ifndef __ISERIALIZERROWREADER_H__
#define __ISERIALIZERROWREADER_H__

#include "libchcore.h"
#include "TString.h"
#include "TPath.h"
#include "IColumnsDefinition.h"

BEGIN_CHCORE_NAMESPACE

class LIBCHCORE_API ISerializerRowReader
{
public:
	virtual ~ISerializerRowReader();

	virtual IColumnsDefinitionPtr GetColumnsDefinitions() const = 0;

	virtual bool Next() = 0;

	virtual void GetValue(const TString& strColName, bool& bValue) = 0;
	virtual void GetValue(const TString& strColName, short& iValue) = 0;
	virtual void GetValue(const TString& strColName, unsigned short& uiValue) = 0;
	virtual void GetValue(const TString& strColName, int& iValue) = 0;
	virtual void GetValue(const TString& strColName, unsigned int& uiValue) = 0;
	virtual void GetValue(const TString& strColName, long long& llValue) = 0;
	virtual void GetValue(const TString& strColName, unsigned long long& llValue) = 0;
	virtual void GetValue(const TString& strColName, double& dValue) = 0;
	virtual void GetValue(const TString& strColName, TString& strValue) = 0;
	virtual void GetValue(const TString& strColName, TSmartPath& pathValue) = 0;
};

typedef boost::shared_ptr<ISerializerRowReader> ISerializerRowReaderPtr;

END_CHCORE_NAMESPACE

#endif