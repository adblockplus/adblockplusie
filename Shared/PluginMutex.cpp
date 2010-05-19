#include "PluginStdAfx.h"

#include "PluginMutex.h"
#include "PluginClient.h"
#include "sddl.h"


CPluginMutex::CPluginMutex(const CString& name, int errorSubidBase) : m_isLocked(false), m_errorSubidBase(errorSubidBase), m_name(name)
{
    if (m_errorSubidBase != PLUGIN_ERROR_MUTEX_DEBUG_FILE)
    {
        DEBUG_MUTEX("Mutex::Create name:" + name)
    }

    m_hMutex = ::CreateMutex(NULL, FALSE, "Global\\SimpleAdblock" + name);

    if (m_hMutex == NULL)
    {
		DWORD error = GetLastError();
		m_hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, "Global\\SimpleAdblock" + name);
		if (m_hMutex == NULL)
		{
		    m_hMutex = ::CreateMutex(NULL, FALSE, "Local\\SimpleAdblock" + name);
			if (m_hMutex == NULL)
			{
				m_hMutex = OpenMutex(NULL, FALSE, "Local\\SimpleAdblock" + name);
				if (m_hMutex == NULL)
				{
					DWORD error = GetLastError();
					DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_CREATE + m_errorSubidBase, "Mutex::CreateMutex");
				}
			}
			else
			{
				switch (::WaitForSingleObject(m_hMutex, 3000))
				{
					// The thread got ownership of the mutex
					case WAIT_OBJECT_0: 
						m_isLocked = true;
						break;

					case WAIT_TIMEOUT:
						DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_WAIT_TIMEOUT + m_errorSubidBase, "Mutex::CreateMutex - Timeout");
						m_hMutex = NULL;
						break;

					case WAIT_FAILED:
						DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_WAIT + m_errorSubidBase, "Mutex::CreateMutex - Wait error");
						break;
				}
			}

		}
    }
    else
    {
        switch (::WaitForSingleObject(m_hMutex, 3000))
        {
            // The thread got ownership of the mutex
            case WAIT_OBJECT_0: 
                m_isLocked = true;
                break;

            case WAIT_TIMEOUT:
                DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_WAIT_TIMEOUT + m_errorSubidBase, "Mutex::CreateMutex - Timeout");
                m_hMutex = NULL;
                break;

            case WAIT_FAILED:
                DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_WAIT + m_errorSubidBase, "Mutex::CreateMutex - Wait error");
                break;
        }
    }
}

CPluginMutex::~CPluginMutex()
{
    if (m_errorSubidBase != PLUGIN_ERROR_MUTEX_DEBUG_FILE)
    {
		DEBUG_MUTEX("Mutex::Release name:" + m_name)
    }

    if (m_isLocked)
    {
        m_isLocked = false;
    }

    if (m_hMutex)
    {
        if (!::ReleaseMutex(m_hMutex))
        {
            DEBUG_ERROR_LOG(::GetLastError(), PLUGIN_ERROR_MUTEX, PLUGIN_ERROR_MUTEX_RELEASE + m_errorSubidBase, "Mutex::ReleaseMutex");
        }
    }

    m_hMutex = NULL;
}

bool CPluginMutex::IsLocked() const
{
    return m_isLocked;
}
