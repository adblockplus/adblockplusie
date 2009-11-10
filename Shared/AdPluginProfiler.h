#ifndef _ADPLUGIN_PROFILER_H_
#define _ADPLUGIN_PROFILER_H_


class CAdPluginProfiler
{

private:

    DWORD m_dwStartTime;
    DWORD m_dwEndTime;
    DWORD m_dwAddTime;
    
public:

    CAdPluginProfiler();
    ~CAdPluginProfiler();

    void StartTimer();
    void StopTimer();

    CStringA GetElapsedTimeString(DWORD addTime=0) const;
    DWORD GetElapsedTime() const;
};


#endif // _ADPLUGIN_PROFILER_H_
