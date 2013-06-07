#ifndef DICTIONARY_H
#define DICTIONARY_H

#include <map>
#include <string>
#include <utility>

class Dictionary
{
  friend class DictionaryTest;

public:
  static void Create(const std::wstring& locale);
  static Dictionary* GetInstance();
  std::wstring Lookup(const std::string& section, const std::string& key) const;

private:
  static Dictionary* instance;

  typedef std::pair<std::string,std::string> KeyType;
  typedef std::map<KeyType,std::wstring> DataType;
  DataType data;

  Dictionary(const std::wstring& locale);
  bool ReadDictionary(const std::wstring& basePath, const std::wstring& locale);
};

#endif
