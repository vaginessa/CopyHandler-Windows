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
#ifndef __TSHAREDMODIFICATIONTRACKER_H__
#define __TSHAREDMODIFICATIONTRACKER_H__

#include "libchcore.h"

BEGIN_CHCORE_NAMESPACE

template<class T, class Bitset, size_t ChangeBit>
class TSharedModificationTracker
{
public:
	TSharedModificationTracker(Bitset& rBitset) :
		m_tValue(),
		m_rBitset(rBitset)
	{
	}

	TSharedModificationTracker(const TSharedModificationTracker<T, Bitset, ChangeBit>& rSrc) :
		m_tValue(rSrc.m_tValue),
		m_rBitset(rSrc.m_rBitset)
	{
	}

	template<class V>
	TSharedModificationTracker(Bitset& rBitset, const V& rValue) :
		m_tValue(rValue),
		m_rBitset(rBitset)
	{
	}

	TSharedModificationTracker& operator=(const TSharedModificationTracker<T, Bitset, ChangeBit>& rValue)
	{
		if(this != &rValue)
		{
			m_tValue = rValue.m_tValue;
			m_rBitset = rValue.m_rBitset;
		}

		return *this;
	}

	template<class V>
	TSharedModificationTracker& operator=(const V& rValue)
	{
		if(m_tValue != rValue)
		{
			m_tValue = rValue;
			m_rBitset[ChangeBit] = true;
		}

		return *this;
	}

	operator const T&() const
	{
		return m_tValue;
	}

	const T& Get() const
	{
		return m_tValue;
	}

	T& Modify()
	{
		m_rBitset[ChangeBit] = true;
		return m_tValue;
	}

	bool IsModified() const
	{
		return m_rBitset[ChangeBit];
	}

private:
	T m_tValue;
	Bitset& m_rBitset;
};

END_CHCORE_NAMESPACE

#endif