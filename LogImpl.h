#ifndef __LOG_IMPL_H__
#define __LOG_IMPL_H__
#pragma once

#pragma warning (disable : 4996) // 'function' : This function or variable may be unsafe.

#include "mutex"
#include "regex"
#include "fstream"

namespace internal
{
    static inline wchar_t* GetFormatedTime(const struct tm *timeptr)
    {
        static wchar_t result[20];
        wcsftime(result, 20, L"%Y-%m-%d %H:%M:%S\0", timeptr);
        return result;
    }

    const static std::wstring g_sRegExPrefix  = L"^.*?\\b(";
    const static std::wstring g_sRegExPSuffix = L")\\b.*$";
};

static std::mutex g_log_mutex;
static std::mutex g_console_mutex;

class CLogImpl
{
// Static operations
public:
    static void Log(const wchar_t *sLogPath, const wchar_t *sMsg)
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);

        time_t rawtime;
        struct tm *timeinfo;
            
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        const wchar_t *sTime = internal::GetFormatedTime(timeinfo);

        std::wstring sMsgImpl;
        sMsgImpl += L"[";
        sMsgImpl += sTime;
        sMsgImpl += L"] ";
        sMsgImpl += sMsg;
        sMsgImpl += L"\n";

        FILE *pFile = _wfopen(sLogPath, L"a");    
        if (!pFile) {
            return;
        }
        fwprintf(pFile, L"%s", sMsgImpl.c_str());
        fclose(pFile);
    }

    static void GetLinesByRegexFromLogFile(const wchar_t *sLogPath, std::vector<std::wstring> &sLinesFromLog, const wchar_t *sRegex)
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        const std::wstring pattern = internal::g_sRegExPrefix + sRegex + internal::g_sRegExPSuffix;
        sLinesFromLog.clear();
        std::wifstream infile(sLogPath);
        for (std::wstring line; std::getline(infile, line);) {
            if(std::regex_match(line, std::wregex(pattern.c_str()))) {
                sLinesFromLog.push_back(line);
            }
        }
    }
};

class CDCOut
{
public:
    static void OutInfoNoConsole(const wchar_t *sLogPath, const wchar_t *sMsg)
    {
        CLogImpl::Log(sLogPath, sMsg);
    }

    static void OutInfo(const wchar_t *sLogPath, const wchar_t *sMsg)
    {
        CLogImpl::Log(sLogPath, sMsg);
        
        {
            std::lock_guard<std::mutex> lock(g_console_mutex);
            std::wcout << sMsg << std::endl;
        }
    }

    static void OutInfo(const wchar_t *sLogPath, const wchar_t *sMsg, const wchar_t *sParam1)
    {
        std::wstring sText(sMsg);
        sText += sParam1;
        OutInfo(sLogPath, sText.c_str());
    }
};

#endif