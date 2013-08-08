#ifndef UPDATER_H
#define UPDATER_H

#include <string>
#include <Windows.h>
#include <AdblockPlus/JsEngine.h>

class Updater
{
public:
  Updater(AdblockPlus::JsEnginePtr jsEngine);
  void SetUrl(const std::string& url);
  void Update();
private:
  AdblockPlus::JsEnginePtr jsEngine;
  std::string url;
  std::wstring tempFile;
  HWND dialog;

  void Download();
  void OnDownloadSuccess();
};

#endif // UPDATER_H
