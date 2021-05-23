#pragma once

#include "string"
#include "sstream"
#include "iomanip"

namespace time_utils
{
    static bool GetTimeFromFilePath(const wchar_t *sFileName, std::tm &timeToDelete)
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
        std::wistringstream streamTime(sTime.c_str());
        streamTime >> std::get_time(&timeToDelete, L"%Y-%m-%d %H-%M-%S");
        
        if (streamTime.fail()) {
            return false;
        }

#if (_MSC_VER < 1920)
        timeToDelete.tm_isdst = -1;
#endif

        return true;
    }
}