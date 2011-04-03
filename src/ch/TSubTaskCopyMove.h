// ============================================================================
//  Copyright (C) 2001-2009 by Jozef Starosczyk
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
/// @file  TSubTaskCopyMove.h
/// @date  2010/09/18
/// @brief Contains declarations of classes responsible for copy and move sub-operation.
// ============================================================================
#ifndef __TSUBTASKCOPYMOVE_H__
#define __TSUBTASKCOPYMOVE_H__

#include "TSubTaskBase.h"

struct CUSTOM_COPY_PARAMS;
class TAutoFileHandle;
class CDataBuffer;

class TSubTaskCopyMove : public TSubTaskBase
{
public:
	TSubTaskCopyMove(TSubTaskContext& tSubTaskContext);

	ESubOperationResult Exec();

private:
	bool GetMove(const CFileInfoPtr& spFileInfo);
	int GetBufferIndex(const CFileInfoPtr& spFileInfo);

	ESubOperationResult CustomCopyFileFB(CUSTOM_COPY_PARAMS* pData);

	ESubOperationResult OpenSourceFileFB(TAutoFileHandle& hFile, const CFileInfoPtr& spSrcFileInfo, bool bNoBuffering);
	ESubOperationResult OpenDestinationFileFB(TAutoFileHandle& hFile, const chcore::TSmartPath& pathDstFile, bool bNoBuffering, const CFileInfoPtr& spSrcFileInfo, unsigned long long& ullSeekTo, bool& bFreshlyCreated);
	ESubOperationResult OpenExistingDestinationFileFB(TAutoFileHandle& hFile, const chcore::TSmartPath& pathDstFilePath, bool bNoBuffering);

	ESubOperationResult SetFilePointerFB(HANDLE hFile, long long llDistance, const chcore::TSmartPath& pathFile, bool& bSkip);
	ESubOperationResult SetEndOfFileFB(HANDLE hFile, const chcore::TSmartPath& pathFile, bool& bSkip);

	ESubOperationResult ReadFileFB(HANDLE hFile, CDataBuffer& rBuffer, DWORD dwToRead, DWORD& rdwBytesRead, const chcore::TSmartPath& pathFile, bool& bSkip);
	ESubOperationResult WriteFileFB(HANDLE hFile, CDataBuffer& rBuffer, DWORD dwToWrite, DWORD& rdwBytesWritten, const chcore::TSmartPath& pathFile, bool& bSkip);
	ESubOperationResult CreateDirectoryFB(const chcore::TSmartPath& pathDirectory);
};

#endif
