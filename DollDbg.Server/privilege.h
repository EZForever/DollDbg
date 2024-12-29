#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern "C"
{
    BOOL xAdjustPrivilege(HANDLE hProcess, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege);
}