#include "DCReadDirChangesImpl.h"

#include <iostream>
#include <filesystem>

#include "fileapi.h"

#include "LogImpl.h"

#ifdef _DEBUG
    #define new DEBUG_NEW
#endif

CDCReadDirChangesImpl::CDCReadDirChangesImpl(const wchar_t *sHotPath, const wchar_t *sBcpPath, const wchar_t *sLogFile) 
    : m_sHotPath(sHotPath), m_sBcpPath(sBcpPath), m_sLogFile(sLogFile)
{
    //
}

CDCReadDirChangesImpl::~CDCReadDirChangesImpl()
{
    threadWatchDir.join();
    threadInput.join();

    if (INVALID_HANDLE_VALUE != m_hDir) {
        CloseHandle(m_hDir);
    }
}

void CDCReadDirChangesImpl::ApplyLogFileFilter(const wchar_t *sInput)
{
    std::vector<std::wstring> arrLinesFromLog;
    CLogImpl::GetLinesByRegexFromLogFile(m_sLogFile.c_str(), arrLinesFromLog, sInput);
    for (const std::wstring &sLine : arrLinesFromLog) {
        std::wcout << sLine << std::endl;
    }

    std::wcout << L"Please enter regex, to filter log file." << std::endl;
}

void CDCReadDirChangesImpl::Start()
{
    m_hDir = CreateFile(m_sHotPath.c_str(), // pointer to the file name
                            FILE_LIST_DIRECTORY,                // access (read/write) mode
                            FILE_SHARE_READ|FILE_SHARE_DELETE|FILE_SHARE_WRITE,  // share mode
                            NULL,                               // security descriptor
                            OPEN_EXISTING,                      // how to create
                            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,         // file attributes
                            NULL                                // file with attributes to copy
                            );

    if (INVALID_HANDLE_VALUE == m_hDir) {
        CDCOut::OutInfo(m_sLogFile.c_str(), L"Invalid watch folder handle");
        return;
    }

    threadWatchDir = std::thread(&CDCReadDirChangesImpl::WatchDirThread, this, m_hDir, m_sHotPath.c_str(), m_sBcpPath.c_str(), m_sLogFile.c_str());
    threadInput    = std::thread(&CDCReadDirChangesImpl::WaitForInputThread, this);
}

void CDCReadDirChangesImpl::Quit() {
    m_bRunning = false;
}

void CDCReadDirChangesImpl::WaitForInputThread()
{
    while (m_bRunning) {
        std::wstring sInput;
        std::getline(std::wcin, sInput);
        if (sInput == L":q") {
            Quit();
        }
        else {
            ApplyLogFileFilter(sInput.c_str());
        }
    }
}

void CDCReadDirChangesImpl::WatchDirThread(HANDLE hDir, const wchar_t *sHotPath, const wchar_t *sBcpPath, const wchar_t *sLogFile)
{
    if (INVALID_HANDLE_VALUE == m_hDir) {
        return;
    }

    OVERLAPPED overlapped;
    overlapped.hEvent = CreateEvent(NULL, FALSE, 0, NULL);

    uint8_t change_buf[1024];
    BOOL bSuccess = ReadDirectoryChangesW(hDir, change_buf, 1024, TRUE,
                                          FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE,
                                          NULL, &overlapped, NULL);

    if (!bSuccess) {
        return;
    }

    while (true) {
        if (!m_bRunning) {
            return;
        }
            
        DWORD result = WaitForSingleObject(overlapped.hEvent, 0);
        std::wstring sOldFileName;

        if (result == WAIT_OBJECT_0) {
            DWORD bytes_transferred;
            GetOverlappedResult(hDir, &overlapped, &bytes_transferred, FALSE);

            FILE_NOTIFY_INFORMATION *pEvent = (FILE_NOTIFY_INFORMATION*)change_buf;

            for (;;) {
                DWORD name_len = pEvent->FileNameLength / sizeof(wchar_t);
                std::wstring sFileName;
                sFileName.assign(pEvent->FileName, name_len);

                std::wstring sOperationText;
                bool bDeleted(false);
                bool bRenamedOld(false);
                                    
                switch(pEvent->Action)
                {
                    case FILE_ACTION_ADDED: 
                    {
                        sOperationText = L"The file was added to the directory"; 
                    }
                    break; 
                    case FILE_ACTION_REMOVED: 
                    {
                        sOperationText = L"The file was removed from the directory";
                        bDeleted = true;
                    }
                    break; 
                    case FILE_ACTION_MODIFIED: 
                    {
                        sOperationText = L"The file was modified";
                    }
                    break; 
                    case FILE_ACTION_RENAMED_OLD_NAME: 
                    {
                        bRenamedOld = true;
                        sOldFileName = sFileName;
                    }
                    break; 
                    case FILE_ACTION_RENAMED_NEW_NAME: 
                    {
                        sOperationText = L"The file was renamed";
                    }
                    break;
                    default:
                    {
                        sOperationText = L"Unknown action";
                    }
                    break;
                }

                if (!bRenamedOld) {
                    if (sFileName != sLogFile) {
                        std::filesystem::path pathSrc(sHotPath);
                        pathSrc /= sFileName;

                        std::filesystem::path pathTrg(sBcpPath);
                        pathTrg /= sFileName;
                        pathTrg += ".bak";
                    
                        std::wstring sInfo;
                        if (!sOldFileName.empty()) {
                            sInfo = sOperationText + L": " + sOldFileName + L" -> "+ sFileName;
                            sOldFileName.clear();
                        }
                        else {
                            sInfo = sOperationText + L": " + sFileName;
                        }

                        if (!bDeleted) {
                            std::filesystem::copy(pathSrc, pathTrg, std::filesystem::copy_options::update_existing);
                        }
                        CDCOut::OutInfoNoConsole(sLogFile, sInfo.c_str());
                    }
                }

                // Are there more events to handle?
                if (pEvent->NextEntryOffset) {
                    *((uint8_t**)&pEvent) += pEvent->NextEntryOffset;
                }
                else {
                    break;
                }
            }
      
            // Queue the next event
            bSuccess = ReadDirectoryChangesW(hDir, change_buf, 1024, TRUE,
                                             FILE_NOTIFY_CHANGE_FILE_NAME|FILE_NOTIFY_CHANGE_DIR_NAME|FILE_NOTIFY_CHANGE_LAST_WRITE,
                                             NULL, &overlapped, NULL);

            if (!bSuccess) {
                return;
            }
        }
    }
}