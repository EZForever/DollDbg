#include "server.h"
#include "privilege.h"

namespace DollDbg
{
    Server::Server* Server::Server::_instance = nullptr;

    namespace Server
    {
        Server::Server(Util::tostream& out, const string_t& connStr)
            : _out(out), _connStr(connStr), _rpc(nullptr), _running(false)
        {
            // Debug privilege
            if (!xAdjustPrivilege(GetCurrentProcess(), SE_DEBUG_NAME, TRUE))
                throw Util::make_syserr("Server::(): xAdjustPrivilege(SE_DEBUG_NAME) failed");

            // Search for victim module
            // FIXME: SearchPathW() is not designed for searching a DLL for LoadLibrary()
            // https://docs.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryw#security-remarks
            std::vector<string_t::value_type> path;
            DWORD pathSize = SearchPath(NULL, _victimModuleName, NULL, 0, path.data(), NULL);
            if (pathSize == 0)
                throw Util::make_syserr("Server::(): SearchPath() failed");

            path.resize((size_t)pathSize + 1);
            pathSize = SearchPath(NULL, _victimModuleName, NULL, (DWORD)path.capacity(), path.data(), NULL);
            if (pathSize == 0)
                throw Util::make_syserr("Server::(): SearchPath() failed");

            _victimModulePath = string_t(path.begin(), path.begin() + pathSize);
            _out << TEXT("Victim module found at ") << _victimModulePath << std::endl;

            // Parse connection string & establish connection
            _initRpc();
        }

        Server::~Server()
        {
            _finiRpc();

            std::vector<uint32_t> victimPids;
            {
                std::lock_guard<std::mutex> lck(_victimsMtx);
                victimPids.resize(_victims.size());
                std::transform(_victims.cbegin(), _victims.cend(), victimPids.begin(), [](auto& x) { return x.first; });
            }

            for (auto pid : victimPids)
                _detachVictim(pid);

            {
                std::unique_lock<std::mutex> lck(_victimsMtx);
                _victimsCv.wait(lck, [this]() { return _victims.size() == 0; });
            }
        }

        void Server::onCtrlBreak()
        {
            std::lock_guard<std::mutex> lck(_victimsMtx);

            _out
                << TEXT("\x1b[97m")
                << TEXT("--- Server status ---") << std::endl
                << TEXT("Connection string: ") << _connStr << std::endl
                << TEXT("Victim module path: ") << _victimModulePath << std::endl
                << TEXT("Victim count: ") << _victims.size() << std::endl
                << TEXT("\x1b[39m")
                << std::endl;
        }
    }
}