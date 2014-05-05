// ============================================================================
//  Copyright (C) 2001-2011 by Jozef Starosczyk
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
/// @file  TTaskDefinition.cpp
/// @date  2010/09/18
/// @brief Contains implementation of class representing task input data.
// ============================================================================
#include "stdafx.h"
#include "..\common\version.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/lexical_cast.hpp>
#include "TTaskDefinition.h"
#include "TCoreException.h"
#include "ErrorCodes.h"

#define CURRENT_TASK_VERSION (((unsigned long long)PRODUCT_VERSION1) << 48 | ((unsigned long long)PRODUCT_VERSION2) << 32 | ((unsigned long long)PRODUCT_VERSION3) << 16 | ((unsigned long long)PRODUCT_VERSION4))

BEGIN_CHCORE_NAMESPACE

TTaskDefinition::TTaskDefinition() :
	m_bModified(false),
	m_strTaskName(),
	m_ullTaskVersion(CURRENT_TASK_VERSION)
{
	boost::uuids::random_generator gen;
	boost::uuids::uuid u = gen();
	m_strTaskName = boost::lexical_cast<std::wstring>(u).c_str();
}

TTaskDefinition::TTaskDefinition(const TTaskDefinition& rSrc) :
	m_strTaskName(rSrc.m_strTaskName),
	m_vSourcePaths(rSrc.m_vSourcePaths),
	m_afFilters(rSrc.m_afFilters),
	m_pathDestinationPath(rSrc.m_pathDestinationPath),
	m_tOperationPlan(rSrc.m_tOperationPlan),
	m_ullTaskVersion(rSrc.m_ullTaskVersion),
	m_tConfiguration(rSrc.m_tConfiguration),
	m_bModified(rSrc.m_bModified)
{
}

TTaskDefinition::~TTaskDefinition()
{
}

TTaskDefinition& TTaskDefinition::operator=(const TTaskDefinition& rSrc)
{
	if(this != &rSrc)
	{
		m_strTaskName = rSrc.m_strTaskName;
		m_vSourcePaths = rSrc.m_vSourcePaths;
		m_afFilters = rSrc.m_afFilters;
		m_pathDestinationPath = rSrc.m_pathDestinationPath;
		m_tOperationPlan = rSrc.m_tOperationPlan;
		m_ullTaskVersion = rSrc.m_ullTaskVersion;
		m_tConfiguration = rSrc.m_tConfiguration;
		m_bModified = rSrc.m_bModified;
	}

	return *this;
}

// Task unique id
TString TTaskDefinition::GetTaskName() const
{
	return m_strTaskName;
}

// Source paths
// initialize object with data (get/set, from cfg file?, from string(cmd line options))
void TTaskDefinition::AddSourcePath(const TSmartPath& tPath)
{
	m_vSourcePaths.Add(tPath);
	m_bModified = true;
}

TSmartPath TTaskDefinition::GetSourcePathAt(size_t stIndex) const
{
	return m_vSourcePaths.GetAt(stIndex);
}

size_t TTaskDefinition::GetSourcePathCount() const
{
	return m_vSourcePaths.GetCount();
}

void TTaskDefinition::ClearSourcePaths()
{
	m_vSourcePaths.Clear();
	m_bModified = true;
}

void TTaskDefinition::SetSourcePaths(const TPathContainer& rvPaths)
{
	m_vSourcePaths = rvPaths;
}

const TPathContainer& TTaskDefinition::GetSourcePaths() const
{
	return m_vSourcePaths;
}

// Destination path
void TTaskDefinition::SetDestinationPath(const TSmartPath& pathDestination)
{
	m_pathDestinationPath = pathDestination;
	m_pathDestinationPath.AppendSeparatorIfDoesNotExist();
	m_bModified = true;
}

TSmartPath TTaskDefinition::GetDestinationPath() const
{
	return m_pathDestinationPath;
}

// Operation type
void TTaskDefinition::SetOperationType(EOperationType eOperation)
{
	m_tOperationPlan.SetOperationType(eOperation);
	m_bModified = true;
}

EOperationType TTaskDefinition::GetOperationType() const
{
	return m_tOperationPlan.GetOperationType();
}

const TOperationPlan& TTaskDefinition::GetOperationPlan() const
{
	return m_tOperationPlan;
}

// Task configuration
void TTaskDefinition::SetConfiguration(const TConfig& rConfig)
{
	m_tConfiguration = rConfig;
	m_bModified = true;
}

TConfig& TTaskDefinition::GetConfiguration()
{
	return m_tConfiguration;
}

const TConfig& TTaskDefinition::GetConfiguration() const
{
	return m_tConfiguration;
}

// Serialization
void TTaskDefinition::Load(const TSmartPath& strPath)
{
	// read everything
	TConfig tTaskInfo;
	tTaskInfo.Read(strPath.ToString());

	// clear everything
	m_strTaskName.Clear();
	m_vSourcePaths.Clear();
	m_pathDestinationPath.Clear();
	m_afFilters.Clear();

	m_tConfiguration.Clear();

	m_bModified = false;

	// get information from config file
	// task unique id - use if provided, generate otherwise
	if(!GetConfigValue(tTaskInfo, _T("TaskDefinition.UniqueID"), m_strTaskName) || m_strTaskName.IsEmpty())
	{
		boost::uuids::random_generator gen;
		boost::uuids::uuid u = gen();
		m_strTaskName = boost::lexical_cast<std::wstring>(u).c_str();

		m_bModified = true;
	}

	// basic information
	// source paths to be processed
	if(!GetConfigValue(tTaskInfo, _T("TaskDefinition.SourcePaths.Path"), m_vSourcePaths) || m_vSourcePaths.IsEmpty())
		THROW_CORE_EXCEPTION(eErr_MissingXmlData);

	GetConfigValue(tTaskInfo, _T("TaskDefinition.Filters"), m_afFilters);

	// destination path
	if(!GetConfigValue(tTaskInfo, _T("TaskDefinition.DestinationPath"), m_pathDestinationPath) || m_pathDestinationPath.IsEmpty())
		THROW_CORE_EXCEPTION(eErr_MissingXmlData);

	m_pathDestinationPath.AppendSeparatorIfDoesNotExist();

	// type of the operation
	int iOperation = eOperation_None;
	if(!tTaskInfo.GetValue(_T("TaskDefinition.OperationType"), iOperation))
		THROW_CORE_EXCEPTION(eErr_MissingXmlData);

	m_tOperationPlan.SetOperationType((EOperationType)iOperation);

	// and version of the task
	if(!GetConfigValue(tTaskInfo, _T("TaskDefinition.Version"), m_ullTaskVersion))
		THROW_CORE_EXCEPTION(eErr_MissingXmlData);

	if(m_ullTaskVersion < CURRENT_TASK_VERSION)
	{
		// migrate the task to the newer version
		// (nothing to migrate at this point, since 1.40 is the first release with xml-based tasks).

		// then mark it as a newest version task
		m_ullTaskVersion = CURRENT_TASK_VERSION;
		m_bModified = true;
	}
	else if(m_ullTaskVersion > CURRENT_TASK_VERSION)
		THROW_CORE_EXCEPTION(eErr_UnsupportedVersion);

	tTaskInfo.ExtractSubConfig(_T("TaskDefinition.TaskSettings"), m_tConfiguration);
}

void TTaskDefinition::StoreInString(TString& strOutput)
{
	// read everything
	TConfig tTaskInfo;

	// get information from config file
	// task unique id - use if provided, generate otherwise
	SetConfigValue(tTaskInfo, _T("TaskDefinition.UniqueID"), m_strTaskName);

	// basic information
	SetConfigValue(tTaskInfo, _T("TaskDefinition.SourcePaths.Path"), m_vSourcePaths);
	SetConfigValue(tTaskInfo, _T("TaskDefinition.Filters"), m_afFilters);
	SetConfigValue(tTaskInfo, _T("TaskDefinition.DestinationPath"), m_pathDestinationPath);

	int iOperation = m_tOperationPlan.GetOperationType();
	SetConfigValue(tTaskInfo, _T("TaskDefinition.OperationType"), iOperation);

	SetConfigValue(tTaskInfo, _T("TaskDefinition.Version"), m_ullTaskVersion);

	tTaskInfo.PutSubConfig(_T("TaskDefinition.TaskSettings"), m_tConfiguration);

	tTaskInfo.WriteToString(strOutput);
}

void TTaskDefinition::LoadFromString(const TString& strInput)
{
	// read everything
	TConfig tTaskInfo;
	tTaskInfo.ReadFromString(strInput);

	// clear everything
	m_strTaskName.Clear();
	m_vSourcePaths.Clear();
	m_pathDestinationPath.Clear();

	m_tConfiguration.Clear();

	m_bModified = false;

	// get information from config file
	// task unique id - use if provided, generate otherwise
	if(!GetConfigValue(tTaskInfo, _T("TaskDefinition.UniqueID"), m_strTaskName) || m_strTaskName.IsEmpty())
	{
		boost::uuids::random_generator gen;
		boost::uuids::uuid u = gen();
		m_strTaskName = boost::lexical_cast<std::wstring>(u).c_str();

		m_bModified = true;
	}

	// basic information
	// source paths to be processed
	if(!GetConfigValue(tTaskInfo, _T("TaskDefinition.SourcePaths.Path"), m_vSourcePaths) || m_vSourcePaths.IsEmpty())
		THROW_CORE_EXCEPTION(eErr_MissingXmlData);

	GetConfigValue(tTaskInfo, _T("TaskDefinition.Filters"), m_afFilters);

	// destination path
	if(!GetConfigValue(tTaskInfo, _T("TaskDefinition.DestinationPath"), m_pathDestinationPath) || m_pathDestinationPath.IsEmpty())
		THROW_CORE_EXCEPTION(eErr_MissingXmlData);

	m_pathDestinationPath.AppendSeparatorIfDoesNotExist();

	// type of the operation
	int iOperation = eOperation_None;
	if(!tTaskInfo.GetValue(_T("TaskDefinition.OperationType"), iOperation))
		THROW_CORE_EXCEPTION(eErr_MissingXmlData);

	m_tOperationPlan.SetOperationType((EOperationType)iOperation);

	// and version of the task
	if(!GetConfigValue(tTaskInfo, _T("TaskDefinition.Version"), m_ullTaskVersion))
		THROW_CORE_EXCEPTION(eErr_MissingXmlData);

	if(m_ullTaskVersion < CURRENT_TASK_VERSION)
	{
		// migrate the task to the newer version
		// (nothing to migrate at this point, since 1.40 is the first release with xml-based tasks).

		// then mark it as a newest version task
		m_ullTaskVersion = CURRENT_TASK_VERSION;
		m_bModified = true;
	}
	else if(m_ullTaskVersion > CURRENT_TASK_VERSION)
		THROW_CORE_EXCEPTION(eErr_UnsupportedVersion);

	tTaskInfo.ExtractSubConfig(_T("TaskDefinition.TaskSettings"), m_tConfiguration);
}

const TFileFiltersArray& TTaskDefinition::GetFilters() const
{
	return m_afFilters;
}

TFileFiltersArray& TTaskDefinition::GetFilters()
{
	return m_afFilters;
}

void TTaskDefinition::SetFilters(const TFileFiltersArray& rFilters)
{
	m_afFilters = rFilters;
}

END_CHCORE_NAMESPACE
