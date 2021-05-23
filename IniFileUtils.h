#pragma once

#include "string"

#include "LogImpl.h"

namespace ini_file_utils
{
    const static std::wstring sIniFile = L"DCFileWatcher.ini";

    static inline void LoadData(std::wstring &sHotPath, std::wstring &sBcpPath, std::vector<std::pair<std::wstring, std::wstring>> &arrToDelete, const wchar_t *sLogFile)
    {
        std::wifstream fileIni(sIniFile);
        if (!fileIni.is_open()) {
            CDCOut::OutInfo(sLogFile, L"Failed to open ini file.");
            return;
        }

        std::getline(fileIni, sHotPath);
        std::getline(fileIni, sBcpPath);

        while (true) {
            std::wstring sPath;
            std::wstring sBcpPath;

            if (!std::getline(fileIni, sPath)) {
                break;
            }

            if (!std::getline(fileIni, sBcpPath)) {
                break;
            }

            arrToDelete.push_back(std::make_pair(sPath, sBcpPath));
        }
    }

    static inline void SaveData(const wchar_t *sHotPath, const wchar_t *sBcpPath, const wchar_t *sLogFile)
    {
        std::wofstream fileIni(sIniFile);
        if (!fileIni.is_open()) {
            CDCOut::OutInfo(sLogFile, L"Failed to open ini file.");
            return;
        }

        fileIni << sHotPath << std::endl;
        fileIni << sBcpPath << std::endl;
    }

    static inline void AppendFileToDelete(const wchar_t *sPath, const wchar_t *sBcpPath, const wchar_t *sLogFile)
    {
        std::wofstream fileIni(sIniFile, std::ios_base::out|std::ios_base::app);
        if (!fileIni.is_open()) {
            CDCOut::OutInfo(sLogFile, L"Failed to open ini file.");
            return;
        }

        fileIni << sPath << std::endl;
        fileIni << sBcpPath;
    }
}