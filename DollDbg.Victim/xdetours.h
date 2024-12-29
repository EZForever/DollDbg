#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <detours.h>

extern "C"
{
    PVOID WINAPI xDetourCodeFromPointer(PVOID pPointer, PVOID* ppGlobals);
}