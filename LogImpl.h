#ifndef __LOG_IMPL_H__
#define __LOG_IMPL_H__
#pragma once

#pragma warning (disable : 4996) // 'function' : This function or variable may be unsafe.

#include "mutex"
#include "regex"
#include "fstream"

namespace internal
{
    // http://www.cplusplus.com/reference/ctime/asctime/ 
    // do not return the \n at the end (diff. from asctime)
    static inline wchar_t* asctime_impl(const struct tm *timeptr)
    {
        static const wchar_t wday_name[][4] = {
        L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat"
        };
        static const wchar_t mon_name[][4] = {
        L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
        L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
        };
        static wchar_t result[26];
        swprintf(result, L"%.3s %.3s%3d %.2d:%.2d:%.2d %d",
        wday_name[timeptr->tm_wday],
        mon_name[timeptr->tm_mon],
        timeptr->tm_mday, timeptr->tm_hour,
        timeptr->tm_min, timeptr->tm_sec,
        1900 + timeptr->tm_year);
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
        const wchar_t *sTime = internal::asctime_impl(timeinfo);

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