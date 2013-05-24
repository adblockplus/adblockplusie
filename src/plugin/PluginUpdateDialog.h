// CUpdateDialog

class CUpdateDialog : public CAxDialogImpl<CUpdateDialog>
{

public:

  CUpdateDialog()
  {
  }

  ~CUpdateDialog()
  {
  }

  enum { IDD = IDD_UPDATEDIALOG };

  BEGIN_MSG_MAP(CUpdateDialog)
    MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
    COMMAND_HANDLER(IDOK, BN_CLICKED, OnClickedOK)
    COMMAND_HANDLER(IDCANCEL, BN_CLICKED, OnClickedCancel)
    CHAIN_MSG_MAP(CAxDialogImpl<CUpdateDialog>)
  END_MSG_MAP()

  // Handler prototypes:
  LRESULT OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
  void SetVersions(CString new_version_, CString cur_version);
  LRESULT OnClickedOK(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
  {
    EndDialog(wID);
    return 0;
  }

  LRESULT OnClickedCancel(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
  {
    EndDialog(wID);
    return 1;
  }

private:

  CString m_curVersion;
  CString m_newVersion;
};
