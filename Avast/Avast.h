#ifdef _DEBUG
 #define PLUGIN_UPDATE_URL "update.txt"
#else
 #define PLUGIN_UPDATE_URL "avastupdate.txt"
#endif

// Name of user dir in %APPDATA%\..\LocalLow
#define USER_DIR "Avast Ad Blocker\\"

#define MULTIPLE_VERSIONS_CHECK() \
  HKEY hkey;\
  LONG res = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Browser Helper Objects\\{FFCB3198-32F3-4E8B-9539-4324694ED664}", NULL, KEY_QUERY_VALUE, &hkey);\
  if (hkey != NULL)\
  {\
    RegCloseKey(hkey);\
    res = RegOpenKeyEx(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Ext\\Settings\\{FFCB3198-32F3-4E8B-9539-4324694ED663}", NULL, KEY_SET_VALUE, &hkey);\
    if (hkey != NULL)\
    {\
      DWORD val = 1;\
      res = RegSetValueEx(hkey, L"Flags", NULL, REG_DWORD, (BYTE*)&val, sizeof(val)); \
    }\
    return S_OK;\
  }