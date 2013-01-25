#ifndef _PLUGIN_TYPEDEF_H_
#define _PLUGIN_TYPEDEF_H_

#ifdef SUPPORT_FILTER

// Filter URL list (url -> version)
typedef std::map<CString, int> TFilterUrlList;

// Filter file list (filename -> download path)
typedef std::set<std::pair<CString,CString> > TFilterFileList;

#endif // SUPPORT_FILTER


#ifdef SUPPORT_WHITELIST

// Domain list (domain -> reason)
typedef std::map<CString, int> TDomainList;

// Domain history (domain -> reason)
typedef std::vector<std::pair<CString, int> > TDomainHistory;

#endif // SUPPORT_WHITELIST


#endif // _PLUGIN_TYPEDEF_H_
