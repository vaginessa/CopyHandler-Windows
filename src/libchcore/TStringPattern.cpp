#include "stdafx.h"
#include "TStringPattern.h"
#include "TCoreException.h"
#include "ErrorCodes.h"
#include <tchar.h>

namespace chcore
{
	namespace
	{
		bool _tcicmp(TCHAR c1, TCHAR c2)
		{
			TCHAR ch1[2] = { c1, 0 }, ch2[2] = { c2, 0 };
			return (_tcsicmp(ch1, ch2) == 0);
		}
	}

	TStringPattern::TStringPattern(EPatternType ePatternType) :
		m_ePatternType(ePatternType)
	{
	}

	TStringPattern::TStringPattern(const TString& strPattern, EPatternType ePatternType) :
		m_ePatternType(ePatternType),
		m_strPattern(strPattern)
	{
	}

	TString TStringPattern::ToSerializedString() const
	{
		TString strPrefix;
		switch (m_ePatternType)
		{
		case EPatternType::eType_Wildcard:
			strPrefix = L"WC;";
			break;

		default:
			THROW_CORE_EXCEPTION(eErr_UnhandledCase);
		}

		return TString(strPrefix + m_strPattern);
	}

	void TStringPattern::FromSerializedString(const TString& strSerializedPattern)
	{
		if (strSerializedPattern.StartsWith(L"WC;"))
		{
			m_ePatternType = EPatternType::eType_Wildcard;
			m_strPattern = strSerializedPattern.Mid(3);
		}
		else
			THROW_CORE_EXCEPTION(eErr_UnhandledCase);
	}

	TStringPattern TStringPattern::CreateFromSerializedString(const TString& strSerializedPattern)
	{
		TStringPattern pattern;
		pattern.FromSerializedString(strSerializedPattern);
		return pattern;
	}

	bool TStringPattern::MatchMask(LPCTSTR lpszMask, LPCTSTR lpszString) const
	{
		bool bMatch = 1;

		//iterate and delete '?' and '*' one by one
		while (*lpszMask != _T('\0') && bMatch && *lpszString != _T('\0'))
		{
			if (*lpszMask == _T('?')) lpszString++;
			else if (*lpszMask == _T('*'))
			{
				bMatch = Scan(lpszMask, lpszString);
				lpszMask--;
			}
			else
			{
				bMatch = _tcicmp(*lpszMask, *lpszString);
				lpszString++;
			}
			lpszMask++;
		}
		while (*lpszMask == _T('*') && bMatch) lpszMask++;

		return bMatch && *lpszString == _T('\0') && *lpszMask == _T('\0');
	}

	// scan '?' and '*'
	bool TStringPattern::Scan(LPCTSTR& lpszMask, LPCTSTR& lpszString) const
	{
		// remove the '?' and '*'
		for (lpszMask++; *lpszString != _T('\0') && (*lpszMask == _T('?') || *lpszMask == _T('*')); lpszMask++)
			if (*lpszMask == _T('?')) lpszString++;
		while (*lpszMask == _T('*')) lpszMask++;

		// if lpszString is empty and lpszMask has more characters or,
		// lpszMask is empty, return 
		if (*lpszString == _T('\0') && *lpszMask != _T('\0')) return false;
		if (*lpszString == _T('\0') && *lpszMask == _T('\0')) return true;
		// else search substring
		else
		{
			LPCTSTR wdsCopy = lpszMask;
			LPCTSTR lpszStringCopy = lpszString;
			bool bMatch = true;
			do
			{
				if (!MatchMask(lpszMask, lpszString)) lpszStringCopy++;
				lpszMask = wdsCopy;
				lpszString = lpszStringCopy;
				while (!(_tcicmp(*lpszMask, *lpszString)) && (*lpszString != '\0')) lpszString++;
				wdsCopy = lpszMask;
				lpszStringCopy = lpszString;
			} while ((*lpszString != _T('\0')) ? !MatchMask(lpszMask, lpszString) : (bMatch = false) != false);

			if (*lpszString == _T('\0') && *lpszMask == _T('\0')) return true;

			return bMatch;
		}
	}

	bool TStringPattern::Matches(const TString& strTextToMatch) const
	{
		return MatchMask(m_strPattern.c_str(), strTextToMatch.c_str());
	}

	void TStringPattern::SetPattern(const TString& strPattern, EPatternType ePatternType)
	{
		m_ePatternType = ePatternType;
		m_strPattern = strPattern;
	}
}