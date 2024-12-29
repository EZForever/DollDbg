#include "server.h"

#include <dolldbg/util/exc.h>
#include <dolldbg/util/tiostream.h>

#include <mutex>
#include <condition_variable>

using namespace DollDbg;

static bool running = true;
static std::mutex stopMtx;
static std::condition_variable stopCv;

BOOL WINAPI onConsoleCtrl(DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
    case CTRL_C_EVENT:
    {
        std::tcout
            << TEXT("\x1b[97m")
            << TEXT("--- Server reset ---") << std::endl
            << TEXT("\x1b[39m")
            << std::endl;

        stopCv.notify_one();
        return TRUE;
    }
    case CTRL_BREAK_EVENT:
    {
        std::tcerr << TEXT("punt!") << std::endl;
        Server::Server::get().onCtrlBreak();
        return TRUE;
    }
    case CTRL_CLOSE_EVENT:
    {
        running = false;
        stopCv.notify_one();
        return TRUE;
    }
    // Not available for interactive applications
    //case CTRL_LOGOFF_EVENT:
    //case CTRL_SHUTDOWN_EVENT:
    default:
        return FALSE;
    }
}

int _tmain(int argc, TCHAR** argv)
{
    std::tcout
        << TEXT("DollDbg.Server v0.0.0 Indev (" __DATE__ ")") << std::endl
        << TEXT("Copyright (c) 2022 Eric Zhang. All rights reserved.") << std::endl
        << std::endl;

    string_t connStr;
    if (argc == 2)
        connStr = argv[1];
    if (connStr.empty())
    {
        std::tcout
            << TEXT("usage: DollDbg.Server <connection_string>") << std::endl
            << std::endl
            << TEXT("where connection_string:") << std::endl
            << TEXT('\t') << TEXT("tcp:<host>[:<port>]") << std::endl
            << TEXT('\t') << TEXT("npipe:\\\\.\\pipe\\<pipe_name>") << std::endl
            << TEXT('\t') << TEXT("com<X>[:baud=<baud_rate>]") << std::endl
            << std::endl;

        std::tcout << TEXT("Connection string? ") << std::flush;
        std::getline(std::tcin, connStr);
        if (connStr.empty())
            return 1;
    }

    std::unique_lock<std::mutex> lck(stopMtx);
    while (running)
    {
        try
        {
            Server::Server::init(std::tcout, connStr);
        }
        catch (...)
        {
            std::tcerr << Util::string_from_exc() << std::endl;
            return 1;
        }

        if (!SetConsoleCtrlHandler(onConsoleCtrl, TRUE))
            std::tcerr << "main(): SetConsoleCtrlHandler(TRUE) failed" << std::endl;

        std::tcout
            << TEXT("Server running, press Ctrl-Break to show status or Ctrl-C to reset") << std::endl
            << std::endl;

        stopCv.wait(lck);

        if (!SetConsoleCtrlHandler(onConsoleCtrl, FALSE))
            std::tcerr << "main(): SetConsoleCtrlHandler(FALSE) failed" << std::endl;

        try
        {
            Server::Server::fini();
        }
        catch (...)
        {
            std::tcerr << Util::string_from_exc() << std::endl;
            return 1;
        }
    }

    return 0;
}

