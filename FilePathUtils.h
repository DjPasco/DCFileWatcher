#pragma once

#include "filesystem"

namespace file_path_utils
{
    std::filesystem::path MakePath(const wchar_t *sPath, const wchar_t *sFileName)
    {
        std::filesystem::path path(sPath);
        path /= sFileName;
        return path;
    }

    std::filesystem::path MakeBcpPath(const wchar_t *sBcpPath, const wchar_t *sFileName)
    {
        std::filesystem::path path(sBcpPath);
        path /= sFileName;
        path += ".bak";
        return path;
    }
}