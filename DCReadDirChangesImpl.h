#pragma once

#include <windows.h>
#include <thread>
#include <mutex>
#include <atomic>

class CDCReadDirChangesImpl
{
public:
    CDCReadDirChangesImpl(const wchar_t *sHotPath, const wchar_t *sBcpPath, const wchar_t *sLogFile);
    ~CDCReadDirChangesImpl();

public:
    void Start();

private:
    void Quit();
    void ApplyLogFileFilter(const wchar_t *sInput);
    void WaitForInputThread();
    void WatchDirThread(HANDLE hDir, const wchar_t *sHotPath, const wchar_t *sBcpPath, const wchar_t *sLogFile);

private:
    std::atomic<bool> m_bRunning {true};
    HANDLE m_hDir                {nullptr};
    std::thread threadWatchDir;
    std::thread threadInput;

    std::wstring m_sHotPath;
    std::wstring m_sBcpPath;
    std::wstring m_sLogFile;
};