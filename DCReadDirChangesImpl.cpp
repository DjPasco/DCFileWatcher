#include "DCReadDirChangesImpl.h"

#include "iostream"
#include "chrono"

#include "fileapi.h"

#include "LogImpl.h"
#include "TimeUtils.h"
#include "FilePathUtils.h"
#include "IniFileUtils.h"

#ifdef _DEBUG
    #define new DEBUG_NEW
#endif

const static wchar_t * g_sDelete = L"delete_";

CDCReadDirChangesImpl::CDCReadDirChangesImpl(const wchar_t *sHotPath, const wchar_t *sBcpPath, const wchar_t *sLogFile, const std::vector<std::pair<std::wstring, std::wstring>> &arrToDelete) 
    : m_sHotPath(sHotPath), m_sBcpPath(sBcpPath), m_sLogFile(sLogFile), m_arrToDelete(arrToDelete)
{
    for (const auto &pair : m_arrToDelete) {
        if (std::filesystem::exists(pair.first)) {
            std::tm timeToDelete = { 0 };
            const bool bFindTime = time_utils::GetTimeFromFilePath(pair.first.c_str(), timeToDelete);
            if (bFindTime) {
                StartFileDeleteThread(pair.first, pair.second, m_sLogFile, timeToDelete, false);
            }
        }
    }
}

CDCReadDirChangesImpl::~CDCReadDirChangesImpl()
{
    threadWatchDir.join();
    threadInput.join();

    if (INVALID_HANDLE_VALUE != m_hDir) {
        CloseHandle(m_hDir);
    }

    SaveNotDeletedFiles();
}

void CDCReadDirChangesImpl::StartFileDeleteThread(std::wstring sFilePath, std::wstring sFileBcpPath, std::wstring sLogFile, std::tm timeToDelete, bool bAddToArr)
{
    std::thread threadDeleteFile(&CDCReadDirChangesImpl::FileDeleteThread, this, sFilePath, sFileBcpPath, sLogFile, timeToDelete);
    threadDeleteFile.detach();
    if (bAddToArr) {
        m_arrToDelete.push_back(std::make_pair(sFilePath, sFileBcpPath));
    }
}

void CDCReadDirChangesImpl::FileDeleteThread(std::wstring sFilePath, std::wstring sFileBcpPath, std::wstring sLogFile, std::tm timeToDelete)
{
    const std::time_t tTime = std::mktime(&timeToDelete);
    const std::chrono::system_clock::time_point timePoint = std::chrono::system_clock::from_time_t(tTime);
    std::this_thread::sleep_until(timePoint);
    const bool bRetFile = std::filesystem::remove(sFilePath);
    const bool bRetBcp  = std::filesystem::remove(sFileBcpPath);
    std::wstring sInfo;
    if (bRetFile && bRetBcp) {
        sInfo = std::wstring(L"The file was deleted because of \"delete_ISODATETIME\" prefix: ") + sFilePath + L" Backup file also was deleted: " + sFileBcpPath;
    }
    else {
        sInfo = std::wstring(L"The delete operation on file ") + sFilePath + L" failed.";
    }

    CDCOut::OutInfoNoConsole(sLogFile.c_str(), sInfo.c_str());
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

void CDCReadDirChangesImpl::Quit() 
{
    m_bRunning = false;
}

void CDCReadDirChangesImpl::SaveNotDeletedFiles() 
{
    for (const auto &pair : m_arrToDelete) {
        if (std::filesystem::exists(pair.first)) {
            ini_file_utils::AppendFileToDelete(pair.first.c_str(), pair.second.c_str(), m_sLogFile.c_str());
        }
    }
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
                std::wstring sOperationText;
                std::wstring sFileName;
                bool bDeleted(false);
                bool bRenamedOld(false);

                GetEventInformation(pEvent, sOperationText, bDeleted, bRenamedOld, sOldFileName, sFileName);

                if (!bRenamedOld) {
                    if (sFileName != sLogFile) {
                        const std::filesystem::path pathFile = file_path_utils::MakePath(sHotPath, sFileName.c_str());
                        const std::filesystem::path pathBcp  = file_path_utils::MakeBcpPath(sBcpPath, sFileName.c_str());
                    
                        std::wstring sInfo;
                        if (!sOldFileName.empty()) {
                            sInfo = sOperationText + L": " + sOldFileName + L" -> "+ sFileName;
                            
                            if (sFileName.rfind(g_sDelete, 0) == 0) {
                                bDeleted = true;
                                DeleteFileOnPrefix(pathFile, sBcpPath, sLogFile, sOldFileName.c_str(), sInfo, sFileName.c_str());
                            }

                            sOldFileName.clear();
                        }
                        else {
                            sInfo = sOperationText + L": " + sFileName;
                        }

                        CDCOut::OutInfoNoConsole(sLogFile, sInfo.c_str());

                        if (!bDeleted) {
                            std::filesystem::copy(pathFile, pathBcp, std::filesystem::copy_options::update_existing);
                            const std::wstring sBcpInfo = std::wstring(L"The file backup was created: ") + pathBcp.c_str();
                            CDCOut::OutInfoNoConsole(sLogFile, sBcpInfo.c_str());
                        }                        
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

void CDCReadDirChangesImpl::GetEventInformation(const FILE_NOTIFY_INFORMATION *pEvent, std::wstring &sOperationText, bool &bDeleted, bool &bRenamedOld, std::wstring &sOldFileName, std::wstring &sFileName)
{
    DWORD name_len = pEvent->FileNameLength / sizeof(wchar_t);
    sFileName.assign(pEvent->FileName, name_len);
    
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
                sOldFileName = sFileName; //To have correct backup file path when delete will be required
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
}

void CDCReadDirChangesImpl::DeleteFileOnPrefix(const std::filesystem::path &pathFile, const wchar_t *sBcpPath, const wchar_t *sLogFile, const wchar_t *sOldFileName, std::wstring &sInfo, const wchar_t *sFileName)
{
    std::tm timeToDelete = {0};
    const bool bFindTime = time_utils::GetTimeFromFilePath(pathFile.c_str(), timeToDelete);
    const std::filesystem::path pathToDeleteBcp = file_path_utils::MakeBcpPath(sBcpPath, sOldFileName);

    if (!bFindTime) {
        std::filesystem::remove(pathFile);
        std::filesystem::remove(pathToDeleteBcp);
        sInfo = std::wstring(L"The file was deleted because of \"delete_\" prefix: ") + sFileName + L" Backup file also was deleted: " + pathToDeleteBcp.c_str();
    }
    else {
        StartFileDeleteThread(pathFile, pathToDeleteBcp, sLogFile, timeToDelete, true);
        sInfo = std::wstring(L"The file will be deleted later because of \"delete_ISODATETIME\" prefix: ") + sFileName + L" Backup file also will be deleted: " + pathToDeleteBcp.c_str();
    }
}