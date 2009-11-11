#include "AdPluginStdAfx.h"

#include "AdPluginUpdateDialog.h"
#include "AdPluginDictionary.h"
#include "AdPluginClient.h"


void CUpdateDialog::SetVersions(CString newVersion, CString curVersion)
{
	m_curVersion = curVersion;
	m_newVersion = newVersion;
} 


LRESULT CUpdateDialog::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CAxDialogImpl<CUpdateDialog>::OnInitDialog(uMsg, wParam, lParam, bHandled);

	bHandled = TRUE;

	this->CenterWindow();

	try
	{
	    CPluginDictionary* dictionary = CPluginDictionary::GetInstance();

		CString text;

		text = dictionary->Lookup("UPDATE_TITLE");
		SetWindowText(text);
		
		text = dictionary->Lookup("UPDATE_NEW_VERSION_EXISTS");
		SetDlgItemText(IDC_UPDATETEXT, text);

		text = dictionary->Lookup("UPDATE_DO_YOU_WISH_TO_DOWNLOAD");
		SetDlgItemText(IDC_DOYOU, text);

		text = dictionary->Lookup("GENERAL_YES");
		SetDlgItemText(IDOK, text);

		text = dictionary->Lookup("GENERAL_NO");
		SetDlgItemText(IDNO, text);
	}
	catch (std::runtime_error&)
	{
	}

	return 1;  // Let the system set the focus
}