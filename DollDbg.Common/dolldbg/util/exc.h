#pragma once
#include <dolldbg/dolldbg.h>
#include <dolldbg/rpc/exc.h>
#include <dolldbg/util/htonx.h>

#include <iomanip>
#include <system_error>

namespace DollDbg
{
    namespace Util
    {
        inline string_t string_from_exc(std::exception_ptr exc = std::current_exception())
        {
            sstream ss;
            try
            {
                std::rethrow_exception(exc);
            }
            catch (const Rpc::RpcException& exc)
            {
                // HACK: 110% ugly, should do better
                auto category = (Rpc::error_t::category_base_t)exc.error().category();
                category = Util::htonx(category);

                ss << string_from(exc.what())
                    << TEXT(", code = ") << string_from(std::string((char*)&category, sizeof(category)))
                    << TEXT(":") << exc.error().code();
            }
            catch (const std::system_error& exc)
            {
                ss << string_from(exc.what())
                    << TEXT(", code = ") << exc.code().value();
            }
            catch (const std::exception& exc)
            {
                ss << string_from(exc.what());
            }
            catch (...)
            {
                ss << TEXT("???");
            }
            return ss.str();
        }
    }
}