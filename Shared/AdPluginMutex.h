#ifndef _PLUGIN_MUTEX_H_
#define _PLUGIN_MUTEX_H_


class CPluginMutex
{

private:

    HANDLE m_hMutex;
    bool m_isLocked;
    int m_errorSubidBase;

public:

	CPluginMutex(const CString& name, int errorSubidBase);
	~CPluginMutex();

    bool IsLocked() const;
};


#endif // _PLUGIN_MUTEX_H_
