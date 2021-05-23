#pragma once

#include "windows.h"
#include "thread"
#include "mutex"
#include "atomic"
#include "filesystem"

class CDCReadDirChangesImpl final
{
public:
    CDCReadDirChangesImpl(const wchar_t *sHotPath, const wchar_t *sBcpPath, const wchar_t *sLogFile, const std::vector<std::pair<std::wstring, std::wstring>> &arrToDelete);
    ~CDCReadDirChangesImpl();

public:
    void Start();

private:
    void Quit();
    void SaveNotDeletedFiles();
    void ApplyLogFileFilter(const wchar_t *sInput);
    void WaitForInputThread();
    void WatchDirThread(HANDLE hDir, const wchar_t *sHotPath, const wchar_t *sBcpPath, const wchar_t *sLogFile);
    void FileDeleteThreadByTime(std::wstring sFilePath, std::wstring sFileBcpPath, std::wstring sLogFile, std::tm timeToDelete);
    void FileDeleteThread(std::wstring sFilePath, std::wstring sFileBcpPath, std::wstring sLogFile);
    void StartFileDeleteThreadByTime(std::wstring sFilePath, std::wstring sFileBcpPath, std::wstring sLogFile, std::tm timeToDelete, bool bAddToArr);
    void StartFileDeleteThread(std::wstring sFilePath, std::wstring sFileBcpPath, std::wstring sLogFile);

private:
    void GetEventInformation(const FILE_NOTIFY_INFORMATION *pEvent, std::wstring &sOperationText, bool &bDeleted, bool &bRenamedOld, std::wstring &sOldFileName, std::wstring &sFileName);
    void DeleteFileOnPrefix(const std::filesystem::path &pathFile, const wchar_t *sBcpPath, const wchar_t *sLogFile, const wchar_t *sOldFileName, std::wstring &sInfo, const wchar_t *sFileName);

private:
    std::atomic<bool> m_bRunning {true};
    
    HANDLE m_hDir                {nullptr};

    std::thread threadWatchDir;
    std::thread threadInput;

    std::wstring m_sHotPath;
    std::wstring m_sBcpPath;
    std::wstring m_sLogFile;

    std::vector<std::pair<std::wstring, std::wstring>> m_arrToDelete;
};