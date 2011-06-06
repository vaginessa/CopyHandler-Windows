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
/// @file  TString.cpp
/// @date  2011/06/05
/// @brief Contains implementation of TString class.
// ============================================================================
#include "stdafx.h" 
#include "TString.h"
#include <string.h>
#pragma warning(push)
#pragma warning(disable: 4996)	// boost::split uses unsafe std::copy
	#include <boost/algorithm/string.hpp>
#pragma warning(pop)
#include "TStringArray.h"

/// Rounding up value to the nearest chunk multiplicity
#define ROUNDUP(val,chunk) ((val + chunk - 1) & ~(chunk-1))

///< Increment value for internal TString buffer
#define CHUNK_INCSIZE		64

// if (buffer_size-string_size > CHUNK_DECSIZE) then the buffer will be shrinked
///< Difference value above which the TString will be shrinked
#define CHUNK_DECSIZE		256

BEGIN_CHCORE_NAMESPACE

namespace details
{
	TInternalStringData* TInternalStringData::Allocate(size_t stBufferSize)
	{
		if(stBufferSize == 0)
			stBufferSize = 1;

		// calculate the real buffer size to be allocated
		size_t stRealBufferSize = stBufferSize * sizeof(wchar_t) + sizeof(TInternalStringData);
		stRealBufferSize = ROUNDUP(stRealBufferSize, CHUNK_INCSIZE);

		// initialize the newly created TInternalStringData
		TInternalStringData* pNewData = (TInternalStringData*)new BYTE[stRealBufferSize];
		pNewData->m_lRefCount = 0;
		pNewData->m_stBufferSize = (stRealBufferSize - sizeof(TInternalStringData)) / sizeof(wchar_t);
		pNewData->m_stStringLength = 0;

		// nullify the string, so it appear empty
		pNewData->GetData()[0] = _T('\0');

		return pNewData;
	}

	void TInternalStringData::Free(TInternalStringData* pStringData)
	{
		if(pStringData)
			delete [] (BYTE*)pStringData;
	}

	TInternalStringData* TInternalStringData::Clone()
	{
		TInternalStringData* pStringData = TInternalStringData::Allocate(m_stBufferSize);
		BOOST_ASSERT(m_stBufferSize == pStringData->m_stBufferSize);

		wcsncpy_s(pStringData->GetData(), pStringData->m_stBufferSize, GetData(), m_stStringLength + 1);
		pStringData->m_stStringLength = m_stStringLength;

		return pStringData;
	}

	TInternalStringData* TInternalStringData::CloneWithResize(size_t stNewSize)
	{
		TInternalStringData* pStringData = TInternalStringData::Allocate(stNewSize);

		size_t stDataToCopy = min(pStringData->m_stBufferSize - 1, m_stStringLength);

		// copy as much data from the current buffer as possible
		wcsncpy_s(pStringData->GetData(), pStringData->m_stBufferSize, GetData(), stDataToCopy);
		// and ensure we always have proper string termination
		pStringData->GetData()[stDataToCopy] = _T('\0');

		pStringData->m_stStringLength = stDataToCopy;

		return pStringData;
	}

	TInternalStringData* TInternalStringData::CloneIfShared(bool& bCloned)
	{
		if(m_lRefCount > 1)
		{
			bCloned = true;
			return Clone();
		}
		else
		{
			bCloned = false;
			return this;
		}
	}
}

using namespace details;

size_t TString::npos = (size_t)-1;

/** Standard constructor - allocates the underlying data object
 */
TString::TString() :
	m_pszStringData(NULL)
{
}

/** Constructor allocates the underlying data object and initializes it with
 *  a given unicode TString.
 * \param[in] pszStr - source unicode TString
 */
TString::TString(const wchar_t* pszStr) :
	m_pszStringData(NULL)
{
	SetString(pszStr);
}

TString::TString(const wchar_t* pszStart, const wchar_t* pszEnd) :
	m_pszStringData(NULL)
{
	SetString(pszStart, pszEnd - pszStart);
}

TString::TString(const wchar_t* pszStart, size_t stCount) :
	m_pszStringData(NULL)
{
	SetString(pszStart, stCount);
}

/** Constructor increases the reference count in the parameter's data object
 *  and copies only the data object address.
 * \param[in] str - source TString object
 */
TString::TString(const TString& str) :
	m_pszStringData(str.m_pszStringData)
{
	AddRef();
}

/** Destructor releases the underlying data object.
 */
TString::~TString()
{
	try
	{
		Release();
	}
	catch(...)
	{
	}
}

/** Operator releases the current data object, stores a pointer to
 *  the data object from the given TString object and increases a reference
 *  count.
 * \param[in] src - source TString object
 * \return A reference to the current TString.
 */
const TString& TString::operator=(const TString& rSrc)
{
	if(this != &rSrc)
	{
		Release();

		m_pszStringData = rSrc.m_pszStringData;
		AddRef();
	}

	return *this;
}

/** Operator makes an own copy of underlying data object (if needed) and copy
 *  there the given unicode TString.
 * \param[in] pszSrc - source unicode TString
 * \return A reference to the current TString object.
 */
const TString& TString::operator=(const wchar_t* pszSrc)
{
	if(m_pszStringData != pszSrc)
		SetString(pszSrc);

	return *this;
}

/** Operator concatenates a given TString object with the current content of
 *  this TString and returns a new TString object.
 * \param[in] src - TString object that will be appended
 * \return A new TString object with concatenated strings.
 */
const TString TString::operator+(const TString& src) const
{
	TString str(*this);
	str.Append(src);
	
	return str;
}

/** Operator concatenates a given unicode TString with the current content of
 *  this TString and returns a new TString object.
 * \param[in] pszSrc - unicode TString that will be appended
 * \return A new TString object with concatenated strings.
 */
const TString TString::operator+(const wchar_t* pszSrc) const
{
	TString str(*this);
	str.Append(pszSrc);
	
	return str;
}

/** Operator appends a given TString object to own internal buffer.
 * \param[in] src - TString object that will be appended
 * \return A reference to this.
 */
const TString& TString::operator+=(const TString& src)
{
	Append(src);
	return *this;
}

/** Operator appends a given unicode TString to own internal buffer.
 * \param[in] pszSrc - unicode TString that will be appended
 * \return A reference to this.
 */
const TString& TString::operator+=(const wchar_t* pszSrc)
{
	Append(pszSrc);
	return *this;
}

/** Function counts the GetLength of a TString in characters (doesn't matter if the TString
 *  is unicode or ansi).
 *  \note All GetLength checks should be done through this function, because of possible future
 *  update that will store the TString GetLength in the internal member.
 *  \return The TString GetLength in characters, not including the terminating '\\0'
 */
size_t TString::GetLength() const
{
	if(m_pszStringData)
		return GetInternalStringData()->GetStringLength();
	else
		return 0;
}

/** Function makes own data object writable and clears it. Does not delete the
 *  internal buffer - only sets the content to '\\0'.
 */
void TString::Clear()
{
	// make sure we have the modifiable object without allocated TString buffer
	Release();
}

/** Function checks if the TString is empty.
 *  \return True if this TString is empty, false otherwise.
 */
bool TString::IsEmpty() const
{
	return !m_pszStringData || m_pszStringData[0] == _T('\0');
}

/** Function merges the given unicode TString with the current content of an internal buffer.
 * \param[in] pszSrc - unicode TString to append
 */
void TString::Append(const wchar_t* pszSrc)
{
	if(!pszSrc)
		return;

	size_t stCurrentLen = GetLength();
	size_t stAddLen = wcslen(pszSrc);
	EnsureWritable(stCurrentLen + stAddLen + 1);

	wcsncpy_s(m_pszStringData + stCurrentLen, GetCurrentBufferSize() - stCurrentLen, pszSrc, stAddLen + 1);
	GetInternalStringData()->SetStringLength(stCurrentLen + stAddLen);
}

/** Function merges the given TString object with the current content of an internal buffer.
 * \param[in] src - TString object to append
 */
void TString::Append(const TString& rSrc)
{
	if(rSrc.IsEmpty())
		return;

	size_t stCurrentLen = GetLength();
	size_t stAddLen = rSrc.GetLength();
	EnsureWritable(stCurrentLen + stAddLen + 1);

	wcsncpy_s(m_pszStringData + stCurrentLen, GetCurrentBufferSize() - stCurrentLen, rSrc.m_pszStringData, stAddLen + 1);
	GetInternalStringData()->SetStringLength(stCurrentLen + stAddLen);
}

/** Returns a new TString object with the Left part of this TString object.
 * \param[in] tLen - count of characters to copy to the new TString object
 * \return The TString with the Left part of the current TString.
 */
TString TString::Left(size_t tLen) const
{
	if(!m_pszStringData || tLen == 0)
		return TString();

	size_t stCurrentLength = GetLength();

	if(tLen >= stCurrentLength)
		return *this;
	else
		return TString(m_pszStringData, tLen);
}

/** Returns a new TString object with the Right part of this TString object.
 * \param[in] tLen - count of characters to copy to the new TString object
 * \return The TString with the Right part of the current TString.
 */
TString TString::Right(size_t tLen) const
{
	if(!m_pszStringData || tLen == 0)
		return TString();

	size_t stCurrentLen = GetLength(); 
	if(tLen >= stCurrentLen)
		return *this;
	else
		return TString(m_pszStringData + stCurrentLen - tLen, tLen);
}

/** Returns a new TString object with the middle part of this TString object.
 * \param[in] tStart - position of the first character to copy
 * \param[in] tLen - count of chars to copy
 * \return The TString with the middle part of the current TString.
 */
TString TString::Mid(size_t tStart, size_t tLen) const
{
	if(!m_pszStringData || tLen == 0)
		return TString();

	size_t stCurrentLength = GetLength();
	if(tStart >= stCurrentLength)
		return TString();

	size_t stRealLength = min(tLen, stCurrentLength - tStart);

	TString strNew(m_pszStringData + tStart, stRealLength);

	return strNew;
}

TString TString::MidByPos(size_t tStart, size_t stAfterEndPos) const
{
	return Mid(tStart, stAfterEndPos - tStart);
}

/** Makes this TString it's Left part. Much faster than using standard
 *  Left() function.
 * \param[in] tLen - count of characters at the beginning of the TString to be Left in a TString.
 * \param[in] bReallocBuffer - if the internal TString buffer is to be reallocated if exceeds
 *                             the allowable range size (CHUNK_INCSIZE, CHUNK_DECSIZE).
 * \see Left()
 */
void TString::LeftSelf(size_t tLen)
{
	// nothing to do if nothing inside
	if(!m_pszStringData)
		return;

	size_t stCurrentLength = GetLength();
	if(tLen < stCurrentLength)		// otherwise there is nothing to do
	{
		EnsureWritable(tLen + 1);
		m_pszStringData[tLen] = _T('\0');
		GetInternalStringData()->SetStringLength(tLen);
	}
}

/** Makes this TString it's Right part. Much faster than using standard
 *  Right() function.
 * \param[in] tLen - count of characters at the end of the TString to be Left in a TString.
 * \param[in] bReallocBuffer - if the internal TString buffer is to be reallocated if exceeds
 *                             the allowable range size (CHUNK_INCSIZE, CHUNK_DECSIZE).
 * \see Right()
 */
void TString::RightSelf(size_t tLen)
{
	// nothing to do if nothing inside
	if(!m_pszStringData)
		return;

	size_t stCurrentLength = GetLength();
	if(tLen < stCurrentLength)		// otherwise there is nothing to do
	{
		EnsureWritable(stCurrentLength + 1);

		wmemmove(m_pszStringData, m_pszStringData + stCurrentLength - tLen, tLen + 1);
		GetInternalStringData()->SetStringLength(tLen);
	}
}

/** Makes this TString it's middle part. Much faster than using standard
 *  Mid() function.
 * \param[in] tStart - starting position of a text to be Left in a TString
 * \param[in] tLen - count of characters at the middle of the TString to be Left in a TString.
 * \param[in] bReallocBuffer - if the internal TString buffer is to be reallocated if exceeds
 *                             the allowable range size (CHUNK_INCSIZE, CHUNK_DECSIZE).
 * \see Mid()
 */
void TString::MidSelf(size_t tStart, size_t tLen)
{
	if(!m_pszStringData)
		return;

	size_t stCurrentLength = GetLength();
	if(tStart >= stCurrentLength)
	{
		EnsureWritable(1);
		m_pszStringData[0] = _T('\0');
		GetInternalStringData()->SetStringLength(0);
	}
	else
	{
		size_t stRealLength = min(tLen, stCurrentLength - tStart);

		EnsureWritable(stRealLength + 1);
		wmemmove(m_pszStringData, m_pszStringData + tStart, stRealLength);
		m_pszStringData[stRealLength] = _T('\0');

		GetInternalStringData()->SetStringLength(stRealLength);
	}
}

void TString::DeleteChar(size_t stIndex)
{
	size_t stCurrentLength = GetLength();
	if(stIndex >= stCurrentLength)
		return;

	EnsureWritable(1);
	wmemmove(m_pszStringData + stIndex, m_pszStringData + stIndex + 1, stCurrentLength - stIndex);
	GetInternalStringData()->SetStringLength(stCurrentLength - 1);
}

void TString::Delete(size_t stIndex, size_t stCount)
{
	size_t stCurrentLength = GetLength();
	if(stIndex >= stCurrentLength)
		return;

	EnsureWritable(stCurrentLength + 1);

	size_t stCountToDelete = min(stCurrentLength - stIndex, stCount);

	wmemmove(m_pszStringData + stIndex, m_pszStringData + stIndex + stCountToDelete, stCountToDelete);
	GetInternalStringData()->SetStringLength(stCurrentLength - stCountToDelete);
}

void TString::Split(const wchar_t* pszSeparators, TStringArray& rStrings) const
{
	rStrings.Clear();
	if(!m_pszStringData || !pszSeparators)
		return;

	// ugly version - many reallocations due to the usage of stl wstrings
	std::vector<std::wstring> vStrings;
	boost::split(vStrings, m_pszStringData, boost::is_any_of(pszSeparators));

	BOOST_FOREACH(const std::wstring& strPart, vStrings)
	{
		rStrings.Add(strPart.c_str());
	}

}

/** Compares a TString with the given unicode TString. Comparison is case sensitive.
 * \param[in] psz - unicode TString to which the TString object will be compared
 * \return <0 if this TString object is "less" than psz, 0 if they are equal and >0 otherwise.
 */
int_t TString::Compare(const wchar_t* psz) const
{
	if(psz == m_pszStringData)
		return 0;
	else
	{
		if(psz == NULL || m_pszStringData == NULL)
			return m_pszStringData == NULL ? -1 : 1;
		else
			return wcscmp(m_pszStringData, psz);
	}
}

/** Compares a TString with the given TString object. Comparison is case sensitive.
 * \param[in] str - TString object to which internal TString object will be compared
 * \return <0 if this TString object is "less" than psz, 0 if they are equal and >0 otherwise.
 */
int_t TString::Compare(const TString& str) const
{
	if(str.m_pszStringData == m_pszStringData)
		return 0;
	else
	{
		if(str.m_pszStringData == NULL || m_pszStringData == NULL)
			return m_pszStringData == NULL ? -1 : 1;
		else
			return wcscmp(m_pszStringData, str.m_pszStringData);
	}
}

/** Compares a TString with the given unicode TString. Comparison is case insensitive.
 * \param[in] psz - unicode TString to which internal TString object will be compared
 * \return <0 if this TString object is "less" than psz, 0 if they are equal and >0 otherwise.
 */
int_t TString::CompareNoCase(const wchar_t* psz) const
{
	if(psz == m_pszStringData)
		return 0;
	else
	{
		if(psz == NULL || m_pszStringData == NULL)
			return m_pszStringData == NULL ? -1 : 1;
		else
			return _wcsicmp(m_pszStringData, psz);
	}
}

/** Compares a TString with the given TString object. Comparison is case insensitive.
 * \param[in] str - TString object to which internal TString object will be compared
 * \return <0 if this TString object is "less" than str, 0 if they are equal and >0 otherwise.
 */
int_t TString::CompareNoCase(const TString& str) const
{
	if(str.m_pszStringData == m_pszStringData)
		return 0;
	else
	{
		if(str.m_pszStringData == NULL || m_pszStringData == NULL)
			return m_pszStringData == NULL ? -1 : 1;
		else
			return _wcsicmp(m_pszStringData, str.m_pszStringData);
	}
}

bool TString::StartsWith(const wchar_t* pszText) const
{
	if(!m_pszStringData || !pszText)
		return false;

	return boost::starts_with(m_pszStringData, pszText);
}

bool TString::StartsWithNoCase(const wchar_t* pszText) const
{
	if(!m_pszStringData || !pszText)
		return false;

	return boost::istarts_with(m_pszStringData, pszText);
}

bool TString::EndsWith(const wchar_t* pszText) const
{
	if(!m_pszStringData || !pszText)
		return false;

	return boost::ends_with(m_pszStringData, pszText);
}

bool TString::EndsWithNoCase(const wchar_t* pszText) const
{
	if(!m_pszStringData || !pszText)
		return false;

	return boost::iends_with(m_pszStringData, pszText);
}

size_t TString::FindFirstOf(const wchar_t* pszChars, size_t stStartFromPos) const
{
	if(!m_pszStringData || !pszChars)
		return npos;

	size_t stCurrentLength = GetLength();
	for(size_t stIndex = stStartFromPos; stIndex < stCurrentLength; ++stIndex)
	{
		if(wcschr(pszChars, m_pszStringData[stIndex]))
			return stIndex;
	}

	return npos;
}

size_t TString::FindLastOf(const wchar_t* pszChars) const
{
	if(!m_pszStringData || !pszChars)
		return npos;

	for(size_t stIndex = GetLength(); stIndex != 0; --stIndex)
	{
		if(wcschr(pszChars, m_pszStringData[stIndex - 1]))
			return stIndex - 1;
	}

	return npos;
}

/** Returns a character at a given position. Function is very slow (needs to recalc the size of the TString
 *  and make a few comparisons), but quite safe - if the index is out of range then -1 is returned.
 *  Make sure to interpret the returned character according to unicode flag. If the TString is unicode, then the
 *  character returned is also unicode (and vice versa).
 * \param[in] tPos - index of the character to return.
 * \return Character code of character on a specified position, or -1 if out of range.
 */
bool TString::GetAt(size_t tPos, wchar_t& wch) const
{
	size_t tSize = GetLength();
	if(tPos < tSize)
	{
		wch = m_pszStringData[tPos];
		return true;
	}
	else
		return false;
}

wchar_t TString::GetAt(size_t tPos) const
{
	size_t tSize = GetLength();
	if(tPos < tSize)
		return m_pszStringData[tPos];
	else
	{
		BOOST_ASSERT(tPos >= tSize);
		// would be nice to throw an exception here
		return L'\0';
	}
}

/** Returns a pointer to the unicode internal buffer. If the buffer is in ansi format
 *  then NULL value is returned. Internal buffer is resized to the specified value
 *  if currently smaller than requested (if -1 is specified as tMinSize then the buffer
 *  is not resized, and the return value could be NULL).
 * \param[in] tMinSize - requested minimal size of the internal buffer (-1 if the size of the TString should not be changed)
 * \return Pointer to the internal unicode buffer.
 */
wchar_t* TString::GetBuffer(size_t tMinSize)
{
	EnsureWritable(tMinSize);
	return m_pszStringData;
}

/** Releases buffer got by user by calling get_bufferx functions. The current
 *  job of this function is to make sure the TString will terminate with null
 *  character at the end of the buffer.
 */
void TString::ReleaseBuffer()
{
	EnsureWritable(1);

	m_pszStringData[GetCurrentBufferSize() - 1] = L'\0';
	GetInternalStringData()->SetStringLength(wcslen(m_pszStringData));
}

void TString::ReleaseBufferSetLength(size_t tSize)
{
	EnsureWritable(tSize + 1);

	m_pszStringData[GetCurrentBufferSize() - 1] = L'\0';
	GetInternalStringData()->SetStringLength(tSize);
}

/** Cast operator - tries to return a pointer to wchar_t* using the current internal
 *  buffer. If the internal buffer is in ansi format, then the debug version asserts
 *  and release return NULL.
 * \return Pointer to an unicode TString (could be null).
 */
TString::operator const wchar_t*() const
{
	return m_pszStringData ? m_pszStringData : L"";
}

/** Function makes the internal data object writable, copies the given unicode TString into
 *  the internal TString buffer and sets the unicode flag if needed.
 * \param[in] pszStr - source unicode TString
 */
void TString::SetString(const wchar_t* pszStr)
{
	if(!pszStr)
		SetString(_T(""));
	else
	{
		size_t stStringLen = wcslen(pszStr);
		EnsureWritable(stStringLen + 1);

		wcsncpy_s(m_pszStringData, GetCurrentBufferSize(), pszStr, stStringLen + 1);
		GetInternalStringData()->SetStringLength(stStringLen);
	}
}

void TString::SetString(const wchar_t* pszStart, size_t stCount)
{
	if(!pszStart || stCount == 0)
		SetString(_T(""));
	else
	{
		EnsureWritable(stCount + 1);

		wcsncpy_s(m_pszStringData, GetCurrentBufferSize() - 1, pszStart, stCount);
		m_pszStringData[stCount] = _T('\0');
		GetInternalStringData()->SetStringLength(stCount);
	}
}

void TString::EnsureWritable(size_t stRequestedSize)
{
	// NOTE: when stRequestedSize is 0, we want to preserve current buffer size
	TInternalStringData* pInternalStringData = GetInternalStringData();
	if(!pInternalStringData)
	{
		pInternalStringData = TInternalStringData::Allocate(stRequestedSize);
		m_pszStringData = pInternalStringData->GetData();
		AddRef();
	}
	else if(stRequestedSize <= GetCurrentBufferSize())
	{
		// no need to resize - just ensuring that we have the exclusive ownership is enough
		bool bCloned = false;
		TInternalStringData* pNewData = pInternalStringData->CloneIfShared(bCloned);
		if(bCloned)
		{
			Release();

			m_pszStringData = pNewData->GetData();
			AddRef();
		}
	}
	else
	{
		// need to resize
		TInternalStringData* pNewData = pInternalStringData->CloneWithResize(stRequestedSize);
		Release();

		m_pszStringData = pNewData->GetData();
		AddRef();
	}
}

END_CHCORE_NAMESPACE

chcore::TString operator+(const wchar_t* pszString, chcore::TString& str)
{
	chcore::TString strNew(pszString);
	strNew += str;
	return strNew;
}