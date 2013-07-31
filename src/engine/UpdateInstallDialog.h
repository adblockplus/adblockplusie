#ifndef UPDATE_INSTALL_DIALOG_H
#define UPDATE_INSTALL_DIALOG_H

class UpdateInstallDialog
{
public:
  UpdateInstallDialog();
  ~UpdateInstallDialog();
  bool Show();

private:
  HWND window;

  UpdateInstallDialog(const UpdateInstallDialog&);
  UpdateInstallDialog& operator=(const UpdateInstallDialog&);
};

#endif
