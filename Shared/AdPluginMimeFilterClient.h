#ifndef _MIME_FILTER_CLIENT_H
#define _MIME_FILTER_CLIENT_H

class MimeFilterClient
{

public:

	MimeFilterClient();
	~MimeFilterClient();

	CComPtr<IClassFactory> m_classFactory;
	CComPtr<IClassFactory> m_spCFHTTP;
};

#endif
