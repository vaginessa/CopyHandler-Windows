/************************************************************************
	Copy Handler 1.x - program for copying data in Microsoft Windows
						 systems.
	Copyright (C) 2001-2004 Ixen Gerthannes (copyhandler@o2.pl)

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*************************************************************************/
#define VWIN32_DIOC_DOS_IOCTL 1 
 
typedef struct _DEVIOCTL_REGISTERS 
{ 
    DWORD reg_EBX; 
    DWORD reg_EDX; 
    DWORD reg_ECX; 
    DWORD reg_EAX; 
    DWORD reg_EDI; 
    DWORD reg_ESI; 
    DWORD reg_Flags; 
} DEVIOCTL_REGISTERS, *PDEVIOCTL_REGISTERS;

#pragma pack(1)
typedef struct _DRIVE_MAP_INFO
{
	BYTE dmiAllocationLength;
	BYTE dmiInfoLength;
	BYTE dmiFlags;
	BYTE dmiInt13Unit;
	DWORD dmiAssociatedDriveMap;
	ULONGLONG dmiPartitionStartRBA;
} DRIVE_MAP_INFO, *PDRIVE_MAP_INFO;
#pragma pack()

// only 9x
BOOL GetDriveMapInfo(UINT nDrive, PDRIVE_MAP_INFO pdmi)
{
	DEVIOCTL_REGISTERS reg, *preg;
	reg.reg_EAX = 0x440D;       // IOCTL for block devices 
	reg.reg_EBX = nDrive;       // zero-based drive ID
	reg.reg_ECX = 0x086f;       // Get Media ID command 
	reg.reg_EDX = (DWORD)pdmi; // receives media ID info 
	preg=&reg;
	
	preg->reg_Flags = 0x8000; // assume error (carry flag set) 
	
	HANDLE hDevice = CreateFile("\\\\.\\vwin32", 
		GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 
        (LPSECURITY_ATTRIBUTES) NULL, OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL, (HANDLE) NULL); 
	
    if (hDevice == (HANDLE)INVALID_HANDLE_VALUE) 
	{
		return FALSE;
	}
    else
    {
	    DWORD cb; 
        BOOL fResult = DeviceIoControl(hDevice, VWIN32_DIOC_DOS_IOCTL, 
            preg, sizeof(*preg), preg, sizeof(*preg), &cb, 0); 
		
        if (!fResult)
		{
			CloseHandle(hDevice);
			return FALSE; 
		}
    } 
	
    CloseHandle(hDevice); 
	
    return TRUE;
}

// only NT
bool GetSignature(LPCTSTR lpszDrive, LPTSTR lpszBuffer, int iSize)
{
	TCHAR szMapping[1024], szQuery[16384], szSymbolic[1024];
	
	// read mappings
	if (QueryDosDevice(lpszDrive, szMapping, 1024) == 0)
		return false;

	// search for all, to find out in which string is the signature
	int iCount, iCount2;
	if ((iCount=QueryDosDevice(NULL, szQuery, 16384)) == 0)
	{
		TRACE("Encountered error #%lu @QueryDosDevice\n", GetLastError());
		return false;
	}

	int iOffset=0, iOffset2=0;
	TCHAR *pszSignature, *pszOffset;
	while (iOffset < iCount)
	{
		if ((iCount2=QueryDosDevice(szQuery+iOffset, szSymbolic, 1024)) == 0)
			return false;

		// read values from szSymbolic and compare with szMapping
		iOffset2=0;
		while (iOffset2 < iCount2)
		{
			// compare szSymbolic+iOffset2 with szMapping
			if (_tcscmp(szMapping, szSymbolic+iOffset2) == 0
				&& _tcsncmp(szQuery+iOffset, _T("STORAGE#Volume#"), _tcslen("STORAGE#Volume#")) == 0)
			{
				// now search for 'Signature' and extract (from szQuery+iOffset)
				pszSignature=_tcsstr(szQuery+iOffset, _T("Signature"));
				if (pszSignature == NULL)
					continue;
				pszOffset=_tcsstr(pszSignature, _T("Offset"));
				if (pszOffset == NULL)
					continue;

				// for better string copying
				pszOffset[0]=_T('\0');

				// found Signature & Offset - copy
				int iCnt=reinterpret_cast<int>(pszOffset)-reinterpret_cast<int>(pszSignature)+1;
				_tcsncpy(lpszBuffer, pszSignature, (iCnt > iSize) ? iSize : iCnt);
				return true;
			}

			iOffset2+=_tcslen(szSymbolic)+1;
		}

		iOffset+=_tcslen(szQuery+iOffset)+1;
	}

	return false;
}

// at 9x function checks int13h devices and at NT within symbolic links
bool IsSamePhysicalDisk(int iDrvNum1, int iDrvNum2)
{
	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize=sizeof(OSVERSIONINFO);

	GetVersionEx(&osvi);
	if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
	{
		DRIVE_MAP_INFO dmi1, dmi2;
		dmi1.dmiAllocationLength=sizeof(DRIVE_MAP_INFO);
		dmi1.dmiInt13Unit=0xff;
		dmi2.dmiAllocationLength=sizeof(DRIVE_MAP_INFO);
		dmi2.dmiInt13Unit=0xff;
		
		// iDrvNum is 0-based, and we need 1-based
		if (!GetDriveMapInfo(iDrvNum1+1, &dmi1) || !GetDriveMapInfo(iDrvNum2+1, &dmi2) || dmi1.dmiInt13Unit != dmi2.dmiInt13Unit || dmi1.dmiInt13Unit == 0xff)
			return false;
		else
			return true;
	}
	else if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
	{
		TCHAR drv1[3], drv2[3];
		
		drv1[0]=static_cast<TCHAR>(iDrvNum1+_T('A'));
		drv1[1]=_T(':');
		drv1[2]=_T('\0');
		drv2[0]=static_cast<TCHAR>(iDrvNum2+_T('A'));
		drv2[1]=_T(':');
		drv2[2]=_T('\0');
		
		TCHAR szSign1[512], szSign2[512];
		return (GetSignature(drv1, szSign1, 512) && GetSignature(drv2, szSign2, 512) && _tcscmp(szSign1, szSign2) == 0);
	}

	return false;
}