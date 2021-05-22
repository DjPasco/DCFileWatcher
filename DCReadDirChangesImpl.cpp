#include "DCReadDirChangesImpl.h"

#include <iostream>
#include <sstream>
#include <locale>
#include <iomanip>
#include <filesystem>
#include <chrono>

#include "fileapi.h"

#include "LogImpl.h"

#ifdef _DEBUG
    #define new DEBUG_NEW
#endif

const static wchar_t * g_sDelete = L"delete_";

namespace time_utils
{
    std::string currentISO8601TimeUTC() {
  auto now = std::chrono::system_clock::now();
  auto itt = std::chrono::system_clock::to_time_t(now);
  std::ostringstream ss;
  //ss << std::put_time(gmtime(&itt), "%FT%TZ");
  ss << std::put_time(gmtime(&itt), "%Y%m%d %H%M%S");
  return ss.str();
}
    bool GetTimeFromFile(const wchar_t *sFileName, std::tm &timeToDelete)
    {
        std::wstring sFileName_(sFileName);
        size_t nPos = sFileName_.find(L"_", 0);
        if (std::wstring::npos == nPos) {
            return false;
        }

        sFileName_.erase(0, nPos+1);

        nPos = sFileName_.find(L"_", nPos);
        if (std::wstring::npos == nPos) {
            return false;
        }

        std::wstring sTime = sFileName_.substr(0, nPos);
        {
            std::locale loc;
            std::wistringstream ss(sTime.c_str());
            ss >> std::get_time(&timeToDelete, L"%Y-%m-%d %H-%M-%S");
        
            if (ss.fail()) {
                return false;
            }
        }

        //time_t now;
        //time(&now);
        //char buf[sizeof "2011-10-08T07:07:09Z"];
        //strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
        //// this will work too, if your compiler doesn't support %F or %T:
        ////strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
        //std::cout << buf << "\n";

        return true;
    }
}

void FileDeleteThread(const wchar_t *sFilePath, const wchar_t *sFileBcpPath, const wchar_t *sLogFile, std::tm *timeToDelete)
{
    std::time_t tt = std::mktime (timeToDelete);
    std::chrono::system_clock::time_point timePoint = std::chrono::system_clock::from_time_t(tt);
    std::time_t t = std::chrono::system_clock::to_time_t(timePoint);
    std::cout << std::ctime(&t) << std::endl;
    std::this_thread::sleep_until(timePoint);
    std::filesystem::remove(sFilePath);
    std::filesystem::remove(sFileBcpPath);
    std::wstring sInfo = std::wstring(L"The file was deleted because of \"delete_ISODATETIME\" prefix: ") + sFilePath + L" Backup file also was deleted: " + sFileBcpPath;
    CDCOut::OutInfoNoConsole(sLogFile, sInfo.c_str());
}

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
                            
                            if (sFileName.rfind(g_sDelete, 0) == 0) {
                                std::filesystem::path pathToDelete(sHotPath);
                                pathToDelete /= sFileName;
                                std::tm timeToDelete = { 0 };
                                const bool bFindTime = time_utils::GetTimeFromFile(pathToDelete.c_str(), timeToDelete);
                                
                                std::filesystem::path pathToDeleteBcp(sBcpPath);
                                pathToDeleteBcp /= sOldFileName;
                                pathToDeleteBcp += ".bak";

                                bDeleted = true;

                                if (!bFindTime) {
                                    std::filesystem::remove(pathToDelete);
                                    std::filesystem::remove(pathToDeleteBcp);
                                    sInfo = std::wstring(L"The file was deleted because of \"delete_\" prefix: ") + sFileName + L" Backup file also was deleted: " + pathToDeleteBcp.c_str();
                                }
                                else {
                                    const wchar_t *sPath = pathToDelete.c_str();
                                    const wchar_t *sPathFile = pathToDeleteBcp.c_str();
                                    std::thread threadDeleteFile(&FileDeleteThread, sPath, sPathFile, sLogFile, &timeToDelete);
                                    threadDeleteFile.detach();
                                    sInfo = std::wstring(L"The file will be deleted later because of \"delete_ISODATETIME\" prefix: ") + sFileName + L" Backup file also was deleted: " + pathToDeleteBcp.c_str();
                                }
                            }
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