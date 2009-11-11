#ifndef _PLUGIN_PROFILER_H_
#define _PLUGIN_PROFILER_H_


class CPluginProfiler
{

private:

    DWORD m_dwStartTime;
    DWORD m_dwEndTime;
    DWORD m_dwAddTime;
    
public:

    CPluginProfiler();
    ~CPluginProfiler();

    void StartTimer();
    void StopTimer();

    CStringA GetElapsedTimeString(DWORD addTime=0) const;
    DWORD GetElapsedTime() const;
};


#endif // _PLUGIN_PROFILER_H_
