#ifndef _PLUGIN_TYPEDEF_H_
#define _PLUGIN_TYPEDEF_H_


#ifdef SUPPORT_FILE_DOWNLOAD

struct SDownloadFileCategory
{
	CString category;
    CString type;
    CString extension;
	CString ffmpegArgs;
    CString description;

    SDownloadFileCategory()
    {
    }

    SDownloadFileCategory(const SDownloadFileCategory& org)
    {
		category = org.category;
		type = org.type;
		extension = org.extension;
		ffmpegArgs = org.ffmpegArgs;
		description = org.description;
	}
};

struct SDownloadFileProperties
{
    CString content;
    CString conversions;
    CString category;
	SDownloadFileCategory properties;
    
    SDownloadFileProperties()
    {
    }

    SDownloadFileProperties(const SDownloadFileProperties& org)
    {
		content = org.content;
        category = org.category;
		properties = org.properties;
		conversions = org.conversions;
    }
};

struct SDownloadFile
{
    CString downloadFile;
    CString downloadUrl;
    int fileType;
	int fileSize;
    SDownloadFileProperties properties;

    SDownloadFile() : fileType(0), fileSize(0)
    {
    }

    SDownloadFile(const SDownloadFile& org)
    {
        downloadFile = org.downloadFile;
        downloadUrl = org.downloadUrl;
        fileType = org.fileType;
        fileSize = org.fileSize;
        properties = org.properties;
    }
};

#endif // SUPPORT_FILE_DOWNLOAD


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


#ifdef SUPPORT_FILE_DOWNLOAD

// Download files (url -> struct)
typedef std::map<CString,SDownloadFile> TDownloadFiles;

// Download files (menu ID -> struct)
typedef std::map<UINT,SDownloadFile> TMenuDownloadFiles;

// Download file properties (content type -> struct)
typedef std::map<CString,SDownloadFileProperties> TDownloadFileProperties;

// Download file categories (category -> struct)
typedef std::map<CString,SDownloadFileCategory> TDownloadFileCategories;

#endif


#endif // _PLUGIN_TYPEDEF_H_
