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
/*************************************************************************
	CLanguageDialog template

	Files: LanguageDialog.h, LanguageDialog.cpp
	Author: Ixen Gerthannes
	Usage:
		Derive your class from CLanguageDialog instead of CDialog, change all
		calls from CDialog to CLanguageDialog, change call to base constructor
		so it can take language as parameter.
	Creating dialog class:
		- derive class from CLanguageDialog
		- change all occurences of CDialog to CLanguageDialog
		- change parameters list of your default constructor so it can take
			language as parameter (WORD wLang)
		- modify call to base class constructor by putting into it declared
			wLang from your constructor
	Displaying dialog box:
		- declare object as your dialog class
		- eventually set public member m_bAutoDelete to true if you're
			creating dialog with operator new, and it should be
			automatically deleted when closed
		- call DoModal/Create/CreateModeless member function for
			modal/modeless/mixed mode dialog
	Members:
		Constructors - as described in CDialog constructors - language
						specifies resource language to load
			CLanguageDialog();
			CLanguageDialog(PCTSTR lpszTemplateName, CWnd* pParent = nullptr);
			CLanguageDialog(UINT uiIDTemplate, CWnd* pParent = nullptr);
		Functions:
			int DoModal(); - like in CDialog
			BOOL Create(); - creates modeless dialog box; this class
				automatically handles DestroyWindow, and like
			void Cleanup(); - function cleans unused data - use only when 
				window object wasn't created yet (in Create() and earlier)
			WORD GetCurrentLanguage() const; - retrieves current language
				setting for this dialog
		Attributes:
			bool m_bAutoDelete; - specifies whether this dialog should be
				deleted (by 'delete this') when closed.
*************************************************************************/
#pragma once

#include "libictranslate.h"
#include "ResourceManager.h"

namespace ictranslate
{
#pragma pack(push, 1)
	struct DLGTEMPLATEEX
	{
		WORD dlgVer;
		WORD signature;
		DWORD helpID;
		DWORD exStyle;
		DWORD style;
		WORD cDlgItems;
		short x;
		short y;
		short cx;
		short cy;
	};

	struct DLGITEMTEMPLATEEX
	{
		DWORD helpID;
		DWORD exStyle;
		DWORD style;
		short x;
		short y;
		short cx;
		short cy;
		WORD id;
		WORD __DUMMY__;
	};
#pragma pack(pop)

	class CDlgTemplate
	{
	public:
		CDlgTemplate();
		explicit CDlgTemplate(const DLGTEMPLATE* pDlgTemplate);
		explicit CDlgTemplate(const DLGTEMPLATEEX* pDlgTemplate);
		~CDlgTemplate();

		bool Open(const DLGTEMPLATE* pDlgTemplate);

	protected:
		void ConvertItemToEx(const DLGITEMTEMPLATE* pSrc, DLGITEMTEMPLATEEX* pDst);		// converts DLGITEMTEMPLATE to DLGITEMTEMPLATEEX
		void ConvertDlgToEx(const DLGTEMPLATE* pSrc, DLGTEMPLATEEX* pDst);

		const BYTE* ReadCompoundData(const BYTE* pBuffer, WORD* pwData, PTSTR* ppszStr);

	public:
		struct _ITEM
		{
			DLGITEMTEMPLATEEX m_itemTemplate;

			WORD m_wClass = 0;
			TCHAR *m_pszClass = nullptr;

			WORD m_wTitle = 0;
			TCHAR *m_pszTitle = nullptr;

			WORD m_wCreationDataSize = 0;
			BYTE *m_pbyCreationData = nullptr;
		};

		std::vector<_ITEM> m_vItems;

		DLGTEMPLATEEX m_dlgTemplate;

		WORD m_wMenu = (WORD)-1;
		TCHAR *m_pszMenu = nullptr;

		WORD m_wClass = (WORD)-1;
		TCHAR *m_pszClass = nullptr;

		WORD m_wTitle = (WORD)-1;		// always -1
		TCHAR *m_pszTitle = nullptr;

		// font
		WORD m_wFontSize = 0;
		WORD m_wWeight = 0;
		BYTE m_byItalic = 0;
		BYTE m_byCharset = 0;
		TCHAR *m_pszFace = nullptr;
	};

	// class stores information about control initial position and offset and scaling factors
	class CControlResizeInfo
	{
	public:
		CControlResizeInfo(int iCtrlID, double dXPosFactor, double dYPosFactor, double dXScaleFactor, double dYScaleFactor);

		void SetInitialPosition(const CRect& rcPos);
		void GetNewControlPlacement(const CRect& rcDlgInitial, const CRect& rcDlgCurrent, CRect& rcNewPlacement);

		void ResetInitState();
		bool IsInitialized() const;

		int GetControlID() const
		{
			return m_iControlID;
		}

	protected:
		int m_iControlID;
		CRect m_rcInitialPosition;
		double m_dXOffsetFactor;
		double m_dYOffsetFactor;
		double m_dXScaleFactor;
		double m_dYScaleFactor;
	};

	/////////////////////////////////////////////////////////////////////////////
	// CLanguageDialog dialog
#define LDF_NODIALOGSIZE 0x01
#define LDF_NODIALOGFONT 0x02

	class LIBICTRANSLATE_API CLanguageDialog : public CDialog
	{
	public:
		// Construction/destruction
		explicit CLanguageDialog(bool* pLock = nullptr);
		explicit CLanguageDialog(PCTSTR lpszTemplateName, CWnd* pParent = nullptr, bool* pLock = nullptr);   // standard constructor
		explicit CLanguageDialog(UINT uiIDTemplate, CWnd* pParent = nullptr, bool* pLock = nullptr);   // standard constructor
		CLanguageDialog(const CLanguageDialog&) = delete;

		CLanguageDialog& operator=(const CLanguageDialog&) = delete;

		~CLanguageDialog();

		// static members - initialize global pointer to a resource manager
		static void SetResManager(CResourceManager* prm)
		{
			m_prm = prm;
		}

		// creation
		INT_PTR DoModal() override;
		virtual BOOL Create();

		void MapRect(RECT* pRect);
		CFont* GetFont()
		{
			return m_pFont ? m_pFont : ((CDialog*)this)->GetFont();
		}

		afx_msg BOOL OnHelpInfo(HELPINFO* pHelpInfo);
		afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
		void OnHelpButton();

		// Controls resize support
		void InitializeResizableControls();
		void ClearResizableControls();
		void AddResizableControl(int iCtrlID, double dXPosFactor, double dYPosFactor, double dXScaleFactor, double dYScaleFactor);
		void RepositionResizableControls();

	protected:
		void UpdateLanguage();
		virtual UINT GetLanguageUpdateOptions()
		{
			return 0;
		}
		virtual void OnLanguageChanged()
		{
		}
		void Cleanup();

		virtual BOOL OnTooltipText(UINT /*uiID*/, TOOLTIPTEXT* /*pTip*/)
		{
			return FALSE;
		}
		BOOL OnInitDialog() override;
		void OnCancel() override;
		void OnOK() override;
		void PostNcDestroy() override;
		LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;

		DECLARE_MESSAGE_MAP()
	private:
		void CalcBaseUnits(PCTSTR pszFacename, WORD wPointSize);

		// Attributes
	public:
		bool m_bAutoDelete;			// deletes this dialog when exiting
		bool m_bLockInstance;		// allows only one instance of this dialog if set

	protected:
		static CResourceManager* m_prm;		// points to the resource manager instance

		bool *m_pbLock;				// dialog box instance lock system
		bool m_bLockChanged;		// if this dialog changed the lock
		PCTSTR m_pszResName;		// resource (string) name of the dialog template
		UINT m_uiResID;				// resource ID if any of the dialog template
		CWnd* m_pParent;			// parent window ptr
		char m_cType;				// type of this dialog box
		CFont* m_pFont;				// currently used font
		int m_iBaseX, m_iBaseY;

		// controls resizing capabilities
		CRect m_rcDialogInitialPosition;
		std::map<int, CControlResizeInfo> m_mapResizeInfo;
	};
}
