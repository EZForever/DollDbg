#pragma once
#include <dolldbg/dolldbg.h>

#include <functional>

namespace DollDbg
{
    namespace Util
    {
        using finally_t = std::function<void()>;

        class finally
        {
        protected:
            finally(finally&) = delete;
            finally(finally&&) = delete;

            finally_t _fini;

        public:
            finally(nullptr_t, finally_t fini)
                : _fini(fini) {}
            finally(finally_t init, finally_t fini)
                : _fini(fini) { init(); }
            ~finally()
                { _fini(); }
        };
    }

}