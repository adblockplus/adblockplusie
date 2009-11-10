#ifndef _ADPLUGIN_MUTEX_H_
#define _ADPLUGIN_MUTEX_H_


class CAdPluginMutex
{

private:

    HANDLE m_hMutex;
    bool m_isLocked;
    int m_errorSubidBase;

public:

	CAdPluginMutex(const CString& name, int errorSubidBase);
	~CAdPluginMutex();

    bool IsLocked() const;
};


#endif // _ADPLUGIN_MUTEX_H_
