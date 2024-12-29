#include "privilege.h"

extern "C"
{
    BOOL xAdjustPrivilege(HANDLE hProcess, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
    {
        HANDLE hToken;
        if (!OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hToken))
            return FALSE;

        LUID luid;
        if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid))
        {
            CloseHandle(hToken);
            return FALSE;
        }

        TOKEN_PRIVILEGES tp { 1, { { luid, bEnablePrivilege ? (DWORD)SE_PRIVILEGE_ENABLED : 0 } } };
        if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL))
        {
            CloseHandle(hToken);
            return FALSE;
        }

        CloseHandle(hToken);
        return TRUE;
    }
}