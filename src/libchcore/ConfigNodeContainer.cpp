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
#include "ConfigNodeContainer.h"
#include <boost/numeric/conversion/cast.hpp>
#include <boost/lexical_cast.hpp>
#include "TCoreException.h"
#include "ErrorCodes.h"
#include <boost/property_tree/ptree.hpp>

BEGIN_CHCORE_NAMESPACE

namespace details
{
	///////////////////////////////////////////////////////////////////////
	ChangeValue::ChangeValue(const TString& strNewValue) : m_strNewValue(strNewValue)
	{
	}

	void ChangeValue::operator()(ConfigNode& rNode)
	{
		m_bWasModified = false;
		if(rNode.m_strValue.Get() != m_strNewValue)
		{
			rNode.m_strValue.Modify() = m_strNewValue;
			m_bWasModified = true;
		}
	}

	bool ChangeValue::WasModified() const
	{
		return m_bWasModified;
	}

	///////////////////////////////////////////////////////////////////////
	ChangeOrderAndValue::ChangeOrderAndValue(const TString& tNewValue, int iOrder) :
	m_strNewValue(tNewValue),
		m_iOrder(iOrder)
	{
	}

	void ChangeOrderAndValue::operator()(ConfigNode& rNode)
	{
		m_bWasModified = false;
		if(rNode.m_strValue.Get() != m_strNewValue || rNode.m_iOrder != m_iOrder)
		{
			rNode.m_strValue = m_strNewValue;
			rNode.m_iOrder = m_iOrder;

			m_bWasModified = true;
		}
	}

	bool ChangeOrderAndValue::WasModified() const
	{
		return m_bWasModified;
	}

	///////////////////////////////////////////////////////////////////////
	ConfigNodeContainer::ConfigNodeContainer() :
		m_bDelayedEnabled(false),
		m_stLastObjectID(0)
	{
	}

	ConfigNodeContainer::ConfigNodeContainer(const ConfigNodeContainer& rSrc)
	{
		boost::shared_lock<boost::shared_mutex> lock(rSrc.m_lock);
		m_mic = rSrc.m_mic;
		m_strFilePath = rSrc.m_strFilePath;
		m_setDelayedNotifications.Clear();
		m_bDelayedEnabled = false;
		m_stLastObjectID = rSrc.m_stLastObjectID;
	}

	ConfigNodeContainer& ConfigNodeContainer::operator=(const ConfigNodeContainer& rSrc)
	{
		if(this != &rSrc)
		{
			boost::unique_lock<boost::shared_mutex> lock2(m_lock);
			boost::shared_lock<boost::shared_mutex> lock(rSrc.m_lock);

			m_mic = rSrc.m_mic;
			m_strFilePath = rSrc.m_strFilePath;
			m_bDelayedEnabled = false;
			m_stLastObjectID = rSrc.m_stLastObjectID;

			m_setDelayedNotifications.Clear();
		}

		return *this;
	}

	TStringArray ConfigNodeContainer::GetArrayValue(PCTSTR pszPropName, const TStringArray& rDefaultValue) const
	{
		TStringArray tArray;

		boost::shared_lock<boost::shared_mutex> lock(m_lock);

		std::pair<ConfigNodeContainer::NodeContainer::const_iterator, ConfigNodeContainer::NodeContainer::const_iterator> pairFnd
			= m_mic.equal_range(boost::make_tuple(pszPropName));

		if(pairFnd.first == m_mic.end())
			return rDefaultValue;

		while(pairFnd.first != m_mic.end() && pairFnd.first != pairFnd.second)
		{
			tArray.Add((*pairFnd.first).m_strValue);
			++pairFnd.first;
		}

		return tArray;
	}

	bool ConfigNodeContainer::GetArrayValueNoDefault(PCTSTR pszPropName, TStringArray& rValue) const
	{
		boost::shared_lock<boost::shared_mutex> lock(m_lock);

		rValue.Clear();

		std::pair<ConfigNodeContainer::NodeContainer::const_iterator, ConfigNodeContainer::NodeContainer::const_iterator> pairFnd
			= m_mic.equal_range(boost::make_tuple(pszPropName));

		if(pairFnd.first == m_mic.end())
			return false;

		while(pairFnd.first != m_mic.end() && pairFnd.first != pairFnd.second)
		{
			rValue.Add((*pairFnd.first).m_strValue);
			++pairFnd.first;
		}

		return true;
	}

	bool ConfigNodeContainer::SetArrayValue(PCTSTR pszPropName, const TStringArray& rValue)
	{
		bool bResult = false;

		boost::unique_lock<boost::shared_mutex> lock(m_lock);

		std::pair<ConfigNodeContainer::NodeContainer::const_iterator, ConfigNodeContainer::NodeContainer::const_iterator> pairFnd
			= m_mic.equal_range(boost::make_tuple(pszPropName));

		if(pairFnd.first == m_mic.end())
		{
			// insert new items
			for(size_t stIndex = 0; stIndex < rValue.GetCount(); ++stIndex)
			{
				m_mic.insert(ConfigNode(++m_stLastObjectID, pszPropName, boost::numeric_cast<int>(stIndex), rValue.GetAt(stIndex)));
			}

			return false;
		}
		else
		{
			// insert/modify/delete items (diff mode)
			size_t stIndex = 0;
			while(pairFnd.first != m_mic.end() && pairFnd.first != pairFnd.second)
			{
				if(stIndex < rValue.GetCount())
				{
					// update existing item
					ChangeOrderAndValue tChange(rValue.GetAt(stIndex), boost::numeric_cast<int>(stIndex));
					m_mic.modify(pairFnd.first, tChange);
					bResult |= tChange.WasModified();

					++pairFnd.first;
				}
				else
				{
					// delete this item
					m_setRemovedObjects.Add(pairFnd.first->m_stObjectID);
					pairFnd.first = m_mic.erase(pairFnd.first);
				}

				++stIndex;
			}

			while(stIndex < rValue.GetCount())
			{
				// add items not added before (with new oids)
				m_mic.insert(ConfigNode(++m_stLastObjectID, pszPropName, boost::numeric_cast<int>(stIndex), rValue.GetAt(stIndex)));
				++stIndex;
			}

			return true;
		}
	}

	void ConfigNodeContainer::DeleteNode(PCTSTR pszPropName)
	{
		boost::unique_lock<boost::shared_mutex> lock(m_lock);

		std::pair<ConfigNodeContainer::NodeContainer::const_iterator, ConfigNodeContainer::NodeContainer::const_iterator> pairFnd
			= m_mic.equal_range(boost::make_tuple(pszPropName));

		if(pairFnd.first == m_mic.end())
			return;

		ConfigNodeContainer::NodeContainer::const_iterator iter = pairFnd.first;
		while(iter != m_mic.end() && iter != pairFnd.second)
		{
			m_setRemovedObjects.Add(iter->m_stObjectID);
		}

		m_mic.erase(pairFnd.first, pairFnd.second);
	}

	bool ConfigNodeContainer::ExtractNodes(PCTSTR pszNode, ConfigNodeContainer& tNewContainer) const
	{
		bool bFound = false;
		TString strReplace(pszNode);
		strReplace += _T(".");

		boost::unique_lock<boost::shared_mutex> dst_lock(tNewContainer.m_lock);
		tNewContainer.m_mic.clear();

		boost::shared_lock<boost::shared_mutex> lock(m_lock);

		for(NodeContainer::const_iterator iter = m_mic.begin(); iter != m_mic.end(); ++iter)
		{
			if(iter->m_strNodeName.Get().StartsWith(strReplace))
			{
				bFound = true;

				TString strName = iter->m_strNodeName.Get();
				strName.MidSelf(strReplace.GetLength());

				tNewContainer.m_mic.insert(ConfigNode(++tNewContainer.m_stLastObjectID, strName, iter->GetOrder(), iter->m_strValue));
			}
		}

		return bFound;
	}

	bool ConfigNodeContainer::ExtractMultipleNodes(PCTSTR pszNode, std::vector<ConfigNodeContainer>& tNewContainers) const
	{
		bool bFound = false;
		TString strReplace(pszNode);
		strReplace += _T("[");

		boost::shared_lock<boost::shared_mutex> lock(m_lock);

		size_t stLastIndex = std::numeric_limits<size_t>::max();
		ConfigNodeContainer* pCurrentContainer = NULL;
		
		for(NodeContainer::const_iterator iter = m_mic.begin(); iter != m_mic.end(); ++iter)
		{
			if(iter->m_strNodeName.Get().StartsWith(strReplace))
			{
				bFound = true;

				TString strName = iter->m_strNodeName.Get();
				strName.MidSelf(strReplace.GetLength());
				size_t stPos = strName.Find(_T("]"));
				if(stPos == std::numeric_limits<size_t>::max())
					THROW_CORE_EXCEPTION(eErr_InvalidData);

				size_t stNodeIndex = boost::lexical_cast<size_t>(strName.Left(stPos));
				if(stNodeIndex != stLastIndex)
				{
					tNewContainers.push_back(ConfigNodeContainer());
					pCurrentContainer = &tNewContainers.back();

					stLastIndex = stNodeIndex;
				}

				pCurrentContainer->m_mic.insert(ConfigNode(++pCurrentContainer->m_stLastObjectID, strName.Mid(stPos + 1), iter->GetOrder(), iter->m_strValue));
			}
		}

		return bFound;
	}

	void ConfigNodeContainer::ImportNodes(PCTSTR pszNode, const ConfigNodeContainer& tContainer)
	{
		boost::shared_lock<boost::shared_mutex> src_lock(tContainer.m_lock);
		boost::unique_lock<boost::shared_mutex> lock(m_lock);

		// search for nodes in this container that starts with pszNode
		TString strSearch(pszNode);
		strSearch += _T(".");

		typedef std::pair<TString, int> PairInfo;
		std::set<PairInfo> setExistingNames;
		
		for(NodeContainer::const_iterator iter = m_mic.begin(); iter != m_mic.end(); ++iter)
		{
			if(iter->m_strNodeName.Get().StartsWith(strSearch))
			{
				setExistingNames.insert(std::make_pair(iter->m_strNodeName.Get(), iter->m_iOrder));
			}
		}

		NodeContainer::const_iterator iter = tContainer.m_mic.begin();
		for(; iter != tContainer.m_mic.end(); ++iter)
		{
			TString strNodeName = pszNode;
			strNodeName += _T(".") + iter->m_strNodeName;

			std::set<PairInfo>::iterator iterExisting = setExistingNames.find(std::make_pair(strNodeName, iter->m_iOrder));
			if(iterExisting != setExistingNames.end())
			{
				// node already exists - modify instead of delete+add
				m_mic.modify(iter, ChangeValue(iter->m_strValue));

				setExistingNames.erase(iterExisting);
			}
			else
			{
				// node does not exist - need to add new one
				m_mic.insert(ConfigNode(++m_stLastObjectID, strNodeName, iter->GetOrder(), iter->m_strValue));
			}

			// remove all nodes with names from setExisting
			BOOST_FOREACH(const PairInfo& pairNode, setExistingNames)
			{
				NodeContainer::iterator iterToRemove = m_mic.find(boost::make_tuple(pairNode.first, pairNode.second));
				if(iterToRemove != m_mic.end())
					m_setRemovedObjects.Add(iterToRemove->m_stObjectID);

				m_mic.erase(iterToRemove);
			}
		}
	}

	void ConfigNodeContainer::AddNodes(PCTSTR pszNode, const ConfigNodeContainer& tContainer)
	{
		boost::shared_lock<boost::shared_mutex> src_lock(tContainer.m_lock);
		boost::unique_lock<boost::shared_mutex> lock(m_lock);

		// determine the current (max) number associated with the node name
		TString strSearch(pszNode);
		strSearch += _T("[");

		size_t stMaxNodeNumber = 0;

		for(NodeContainer::const_iterator iter = m_mic.begin(); iter != m_mic.end(); ++iter)
		{
			TString strCurrentNode = iter->m_strNodeName.Get();
			if(strCurrentNode.StartsWith(strSearch))
			{
				strCurrentNode.MidSelf(strSearch.GetLength());
				size_t stPos = strCurrentNode.Find(_T("]"));
				if(stPos == std::numeric_limits<size_t>::max())
					THROW_CORE_EXCEPTION(eErr_InvalidData);

				size_t stNumber = boost::lexical_cast<size_t>(strCurrentNode.Left(stPos));
				stMaxNodeNumber = std::max(stMaxNodeNumber, stNumber + 1);
			}
		}

		TString strNodePrefix = pszNode;
		strNodePrefix += TString(_T("[")) + boost::lexical_cast<std::wstring>(stMaxNodeNumber).c_str() + _T("].");

		NodeContainer::const_iterator iter = tContainer.m_mic.begin();
		for(; iter != tContainer.m_mic.end(); ++iter)
		{
			TString strNodeName = strNodePrefix + iter->m_strNodeName;

			m_mic.insert(ConfigNode(++m_stLastObjectID, strNodeName, iter->GetOrder(), iter->m_strValue));
		}
	}

	void ConfigNodeContainer::ImportNode(TString strCurrentPath, const boost::property_tree::wiptree& rTree)
	{
		if(rTree.empty())
			return;

		// append separator for non-empty nodes
		if(!strCurrentPath.IsEmpty())
			strCurrentPath += _T(".");

		TString strNewPath;

		// analyze subnodes (has only leaf-node(s), multiple non-leaf subnodes with same name, multiple subnodes with different names)
		std::set<TString> setNodeNames;
		bool bAllLeafNodes = true;
		size_t stChildCount = 0;
		BOOST_FOREACH(const boost::property_tree::wiptree::value_type& rNode, rTree)
		{
			setNodeNames.insert(rNode.first.c_str());

			if(!rNode.second.empty())
				bAllLeafNodes = false;

			++stChildCount;
		}

		// sanity check (either unique names or empty or an array with single name
		size_t stNodeNamesCount = setNodeNames.size();
		if(stChildCount != stNodeNamesCount && stNodeNamesCount != 1)
			THROW_CORE_EXCEPTION(eErr_InvalidData);

		enum EMode { eMode_LeafStringArrayEntries, eMode_LeafOrContainer, eMode_ContainerSplit, eMode_ContainerPassThrough };
		EMode eMode = eMode_LeafStringArrayEntries;

		if(stNodeNamesCount == 1 && stChildCount > 1)
		{
			if(bAllLeafNodes)
				eMode = eMode_LeafStringArrayEntries;
			else
				eMode = eMode_ContainerSplit;
		}
		else
			eMode = eMode_LeafOrContainer;

		int iIndex = 0;
		BOOST_FOREACH(const boost::property_tree::wiptree::value_type& rNode, rTree)
		{
			switch(eMode)
			{
			case eMode_LeafStringArrayEntries:
				{
					strNewPath = strCurrentPath + rNode.first.c_str();
					m_mic.insert(ConfigNode(++m_stLastObjectID, strNewPath, iIndex++, rNode.second.get_value<std::wstring>().c_str()));
					break;
				}
			case eMode_LeafOrContainer:
				{
					strNewPath = strCurrentPath + rNode.first.c_str();
					if(rNode.second.empty())
					{
						// get leaf info
						m_mic.insert(ConfigNode(++m_stLastObjectID, strNewPath, 0, rNode.second.get_value<std::wstring>().c_str()));
					}
					else
					{
						// traverse through the container
						ImportNode(strNewPath, rNode.second);
					}

					break;
				}
			case eMode_ContainerSplit:
				{
					strNewPath = strCurrentPath + rNode.first.c_str() + _T("[") + boost::lexical_cast<std::wstring>(iIndex++).c_str() + _T("]");
					ImportNode(strNewPath, rNode.second);
					break;
				}
			case eMode_ContainerPassThrough:
				{
					break;
				}
			}
		}
	}

	void ConfigNodeContainer::ImportFromPropertyTree(const boost::property_tree::wiptree& rTree, boost::unique_lock<boost::shared_mutex>&)
	{
//		boost::unique_lock<boost::shared_mutex> lock(m_lock);	// do not lock - locking is done outside
		m_mic.clear();

		// iterate through property tree
		ImportNode(_T(""), rTree);

		Dump();
	}

	void ConfigNodeContainer::ExportToPropertyTree(boost::property_tree::wiptree& rTree) const
	{
		rTree.clear();

		size_t stLastBracketID = std::numeric_limits<size_t>::max();
		TString strGroupNode;

		TCHAR szData[1024];
szData;
		boost::property_tree::wiptree treeSubnodes;

		int iNode = 0;
iNode;
		boost::shared_lock<boost::shared_mutex> lock(m_lock);
		for(NodeContainer::const_iterator iter = m_mic.begin(); iter != m_mic.end(); ++iter)
		{
			const TString& rNodeName = iter->m_strNodeName.Get();

			// tmp
			//_sntprintf_s(szData, 1024, _TRUNCATE, _T("ExportToPropertyTree (%ld): %s\n"), iNode++, (PCTSTR)rNodeName);
//			OutputDebugString(szData);
			// /tmp

			TString strNodeName = rNodeName;

			size_t stBracketPos = strNodeName.Find(_T("["));
			if(stBracketPos != TString::npos)
			{
				// there is a bracket in the node name - this element is a part of a hierarchy of similar nodes
				size_t stSecondBracketPos = strNodeName.Find(_T("["), stBracketPos + 1);
				if(stSecondBracketPos == TString::npos)
					THROW_CORE_EXCEPTION(eErr_InvalidData);

				strGroupNode = strNodeName.Left(stBracketPos);
				TString strSubnodeName = strNodeName.Mid(stSecondBracketPos + 1);

				size_t stBracketID = boost::lexical_cast<size_t>(strNodeName.Mid(stBracketPos, stSecondBracketPos - stBracketPos - 1));
				if(stBracketID != stLastBracketID)
				{
					// new ID - add new property tree node
					if(!treeSubnodes.empty())
					{
						rTree.add_child((PCTSTR)strGroupNode, treeSubnodes);
						treeSubnodes.clear();
					}
				}

				// same ID - add new element to existing property tree node
				treeSubnodes.put((PCTSTR)strSubnodeName, iter->m_strValue);
			}
			else
			{
				// add the subnodes from previous bracket-based entries
				if(!treeSubnodes.empty())
				{
					rTree.add_child((PCTSTR)strGroupNode, treeSubnodes);
					treeSubnodes.clear();
				}

				// no bracket in the node name - this is just a standard entry
				rTree.add((PCTSTR)strNodeName, iter->m_strValue);
			}
		}

		// add the last subnode if not empty
		if(!treeSubnodes.empty())
			rTree.add_child((PCTSTR)strGroupNode, treeSubnodes);
	}

	void ConfigNodeContainer::Dump()
	{
		const size_t stBufferSize = 1024;
		TCHAR szBuffer[stBufferSize];

		for(NodeContainer::const_iterator iter = m_mic.begin(); iter != m_mic.end(); ++iter)
		{
			_sntprintf_s(szBuffer, stBufferSize, _TRUNCATE, _T("Node (oid %I64u): %s.%ld = %s\n"), iter->m_stObjectID, (PCTSTR)iter->m_strNodeName.Get(), iter->m_iOrder.Get(), (PCTSTR)iter->m_strValue.Get());
			OutputDebugString(szBuffer);
		}
	}

	void ConfigNodeContainer::AddEntry(PCTSTR pszPropName, int iIndex, const TString& strValue)
	{
		std::pair<NodeContainer::iterator, bool> pairInsert = m_mic.insert(ConfigNode(++m_stLastObjectID, pszPropName, iIndex, strValue));
		pairInsert.first->m_setModifications.reset();
	}
}

END_CHCORE_NAMESPACE