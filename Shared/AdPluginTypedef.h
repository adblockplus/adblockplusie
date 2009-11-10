#ifndef _ADPLUGIN_TYPEDEF_H_
#define _ADPLUGIN_TYPEDEF_H_


#ifdef SUPPORT_FILE_DOWNLOAD

struct SDownloadFileProperties
{
    CStringA extension;
    CStringA type;
    CStringA content;
    CStringA description;
    
    SDownloadFileProperties()
    {
    }

    SDownloadFileProperties(const SDownloadFileProperties& org)
    {
        extension = org.extension;
        type = org.type;
        content = org.content;
        description = org.description;
    }
};

struct SDownloadFile
{
    CStringW downloadFile;
    CStringA downloadUrl;
    int fileType;
    SDownloadFileProperties properties;

    SDownloadFile() : fileType(0)
    {
    }

    SDownloadFile(const SDownloadFile& org)
    {
        downloadFile = org.downloadFile;
        downloadUrl = org.downloadUrl;
        fileType = org.fileType;
        properties = org.properties;
    }
};

#endif // SUPPORT_FILE_DOWNLOAD


#ifdef SUPPORT_FILTER

// Filter URL list (url -> version)
typedef std::map<CStringA, int> TFilterUrlList;

// Filter file list (filename -> download path)
typedef std::set<std::pair<CStringA,CStringA> > TFilterFileList;

#endif // SUPPORT_FILTER


#ifdef SUPPORT_WHITELIST

// Domain list (domain -> reason)
typedef std::map<CStringA, int> TDomainList;

// Domain history (domain -> reason)
typedef std::vector<std::pair<CStringA, int> > TDomainHistory;

#endif // SUPPORT_WHITELIST


#ifdef SUPPORT_FILE_DOWNLOAD

// Download files (url -> struct)
typedef std::map<CStringA,SDownloadFile> TDownloadFiles;

// Download files (menu ID -> struct)
typedef std::map<UINT,SDownloadFile> TMenuDownloadFiles;

// Download file properties (content type -> struct)
typedef std::map<CStringA,SDownloadFileProperties> TDownloadFileProperties;

#endif


#endif // _ADPLUGIN_TYPEDEF_H_
