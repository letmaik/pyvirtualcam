#pragma once

#include <Windows.h>
#include <stdio.h>

inline void PrintLastError()
{
    DWORD errorMessageID = GetLastError();
    if (errorMessageID == 0)
        return;

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    fprintf(stderr, messageBuffer);
    LocalFree(messageBuffer);
}
