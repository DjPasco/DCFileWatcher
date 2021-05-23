#include "iostream"
#include "memory"
#include "string"

#include "LogImpl.h"
#include "IniFileUtils.h"
#include "DCReadDirChangesImpl.h"

#ifdef _DEBUG
    #define new DEBUG_NEW
#endif

const static std::wstring sLogFile = L"DCFileWatcher.log";

int wmain(int argc, wchar_t* argv[])
{
    std::wstring sHotPath;
    std::wstring sBcpPath;
    std::vector<std::pair<std::wstring, std::wstring>> arrToDelete;
    ini_file_utils::LoadData(sHotPath, sBcpPath, arrToDelete, sLogFile.c_str());
    if (1 == argc) {
        CDCOut::OutInfo(sLogFile.c_str(), L"No parameters were passed to the application. Application will start with previuos parameters.");
    }
    else if (3 == argc) {
        sHotPath = argv[1];
        sBcpPath = argv[2];
    }
    else {
        CDCOut::OutInfo(sLogFile.c_str(), L"Application supports two parameters: 1) Path to watch; 2) Backup path");
        return 0;
    }

    if (sHotPath.empty() || sBcpPath.empty()) {
        CDCOut::OutInfo(sLogFile.c_str(), L"Application supports two parameters: 1) Path to watch; 2) Backup path");
        return 0;
    }

    ini_file_utils::SaveData(sHotPath.c_str(), sBcpPath.c_str(), sLogFile.c_str());

    CDCOut::OutInfo(sLogFile.c_str(), L"Application started to watch: ", sHotPath.c_str());
    CDCOut::OutInfo(sLogFile.c_str(), L"Application backup path: ", sBcpPath.c_str());
    CDCOut::OutInfo(sLogFile.c_str(), L"Application log file: ", sLogFile.c_str());
    CDCOut::OutInfo(sLogFile.c_str(), L"Please enter regex, to filter log file.");
    
    std::unique_ptr<CDCReadDirChangesImpl> pWatcher = std::make_unique<CDCReadDirChangesImpl>(sHotPath.c_str(), sBcpPath.c_str(), sLogFile.c_str(), arrToDelete);
    pWatcher->Start();

    return 0;
}