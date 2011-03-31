/***************************************************************************
*   Copyright (C) 2001-2008 by J�zef Starosczyk                           *
*   ixen@copyhandler.com                                                  *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU Library General Public License          *
*   (version 2) as published by the Free Software Foundation;             *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU Library General Public     *
*   License along with this program; if not, write to the                 *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/
#include "stdafx.h"
#include "TShellExtensionClient.h"
#include "objbase.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

TShellExtensionClient::TShellExtensionClient() :
	m_piShellExtControl(NULL)
{
}

TShellExtensionClient::~TShellExtensionClient()
{
	FreeControlInterface();
}

HRESULT TShellExtensionClient::RegisterShellExtDll(const CString& strPath, long lClientVersion, long& rlExtensionVersion, CString& rstrExtensionStringVersion)
{
	if(strPath.IsEmpty())
		return E_INVALIDARG;

	HRESULT hResult = S_OK;

	if(SUCCEEDED(hResult))
		hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// get rid of the interface, so we can at least try to re-register
	if(SUCCEEDED(hResult))
		FreeControlInterface();

	// first try - load dll and register it manually.
	// if failed - try by loading extension manually (would fail on vista when running as user)
	if(SUCCEEDED(hResult))
	{
		HRESULT (STDAPICALLTYPE *pfn)(void);
		HINSTANCE hMod = LoadLibrary(strPath);	// load the dll
		if(hMod == NULL)
			hResult = HRESULT_FROM_WIN32(GetLastError());
		if(SUCCEEDED(hResult) && !hMod)
			hResult = E_FAIL;
		if(SUCCEEDED(hResult))
		{
			(FARPROC&)pfn = GetProcAddress(hMod, "DllRegisterServer");
			if(pfn == NULL)
				hResult = E_FAIL;
			if(SUCCEEDED(hResult))
				hResult = (*pfn)();

			CoFreeLibrary(hMod);
		}
		CoUninitialize();
	}

	// if previous operation failed (ie. vista system) - try running regsvr32 with elevated privileges
	if(SCODE_CODE(hResult) == ERROR_ACCESS_DENIED)
	{
		// try with regsvr32
		SHELLEXECUTEINFO sei;
		memset(&sei, 0, sizeof(sei));
		sei.cbSize = sizeof(sei);
		sei.fMask = SEE_MASK_UNICODE;
		sei.lpVerb = _T("runas");
		sei.lpFile = _T("regsvr32.exe");
		CString strParams = CString(_T(" \"")) + strPath + CString(_T("\""));
		sei.lpParameters = strParams;
		sei.nShow = SW_SHOW;

		if(!ShellExecuteEx(&sei))
			hResult = E_FAIL;
		else
			hResult = S_OK;
	}

	if(SUCCEEDED(hResult))
		hResult = EnableExtensionIfCompatible(lClientVersion, rlExtensionVersion, rstrExtensionStringVersion);

	return hResult;
}

HRESULT TShellExtensionClient::UnRegisterShellExtDll(const CString& strPath)
{
	if(strPath.IsEmpty())
		return E_INVALIDARG;

	HRESULT hResult = S_OK;

	if(SUCCEEDED(hResult))
		hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);

	// get rid of the interface if unregistering
	if(SUCCEEDED(hResult))
		FreeControlInterface();

	// first try - load dll and register it manually.
	// if failed - try by loading extension manually (would fail on vista when running as user)
	if(SUCCEEDED(hResult))
	{
		HRESULT (STDAPICALLTYPE *pfn)(void);
		HINSTANCE hMod = LoadLibrary(strPath);	// load the dll
		if(hMod == NULL)
			hResult = HRESULT_FROM_WIN32(GetLastError());
		if(SUCCEEDED(hResult) && !hMod)
			hResult = E_FAIL;
		if(SUCCEEDED(hResult))
		{
			(FARPROC&)pfn = GetProcAddress(hMod, "DllUnregisterServer");
			if(pfn == NULL)
				hResult = E_FAIL;
			if(SUCCEEDED(hResult))
				hResult = (*pfn)();

			CoFreeLibrary(hMod);
		}
		CoUninitialize();
	}

	// if previous operation failed (ie. vista system) - try running regsvr32 with elevated privileges
	if(SCODE_CODE(hResult) == ERROR_ACCESS_DENIED)
	{
		// try with regsvr32
		SHELLEXECUTEINFO sei;
		memset(&sei, 0, sizeof(sei));
		sei.cbSize = sizeof(sei);
		sei.fMask = SEE_MASK_UNICODE;
		sei.lpVerb = _T("runas");
		sei.lpFile = _T("regsvr32.exe");
		CString strParams = CString(_T("/u \"")) + strPath + CString(_T("\""));
		sei.lpParameters = strParams;
		sei.nShow = SW_SHOW;

		if(!ShellExecuteEx(&sei))
			hResult = E_FAIL;
		else
			hResult = S_OK;
	}

	return hResult;
}

HRESULT TShellExtensionClient::EnableExtensionIfCompatible(long lClientVersion, long& rlExtensionVersion, CString& rstrExtensionStringVersion)
{
	rlExtensionVersion = 0;
	rstrExtensionStringVersion.Empty();

	BSTR bstrVersion = NULL;

	HRESULT hResult = RetrieveControlInterface();
	if(SUCCEEDED(hResult) && !m_piShellExtControl)
		hResult = E_FAIL;
	if(SUCCEEDED(hResult))
		hResult = m_piShellExtControl->GetVersion(&rlExtensionVersion, &bstrVersion);
	if(SUCCEEDED(hResult))
	{
		// enable or disable extension - currently we only support extension from strictly the same version as CH
		hResult = m_piShellExtControl->SetFlags((lClientVersion == rlExtensionVersion) ? eShellExt_Enabled : 0, eShellExt_Enabled);
		if(SUCCEEDED(hResult))
			hResult = S_OK;
	}
	else if(SUCCEEDED(hResult))
		hResult = S_FALSE;

	// do not overwrite S_OK/S_FALSE status after this line - it needs to be propagated upwards
	if(bstrVersion)
	{
		rstrExtensionStringVersion = bstrVersion;
		::SysFreeString(bstrVersion);
	}

	return hResult;
}

void TShellExtensionClient::Close()
{
	FreeControlInterface();
}

HRESULT TShellExtensionClient::RetrieveControlInterface()
{
	HRESULT hResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if(SUCCEEDED(hResult))
		hResult = CoCreateInstance(CLSID_CShellExtControl, NULL, CLSCTX_ALL, IID_IShellExtControl, (void**)&m_piShellExtControl);
	if(SUCCEEDED(hResult) && !m_piShellExtControl)
		hResult = E_FAIL;

	return hResult;
}

void TShellExtensionClient::FreeControlInterface()
{
	if(m_piShellExtControl)
	{
		m_piShellExtControl->Release();
		m_piShellExtControl = NULL;
	}
}