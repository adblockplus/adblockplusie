#ifndef UPDATER_H
#define UPDATER_H

#include <string>
#include <Windows.h>
#include <AdblockPlus/JsEngine.h>

class Updater
{
public:
  Updater(AdblockPlus::JsEnginePtr jsEngine, const std::string& url);
  void Update();
private:
  AdblockPlus::JsEnginePtr jsEngine;
  std::string url;
  std::wstring tempFile;
  HWND dialog;

  void StartDownload(HWND dialog);
  void RunDownload();
};

#endif // UPDATER_H
