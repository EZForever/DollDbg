#pragma once
#include <dolldbg/dolldbg.h>
#include <dolldbg/rpc/exc.h>
#include <dolldbg/protocol/base.h>
#include <dolldbg/protocol/number.h>

#include <flatbuffers/flatbuffers.h>

namespace DollDbg
{
    namespace Protocol
    {
        namespace PayloadGuid
        {
            // {C1806A47-437E-4C0A-8143-904A5F35AAD3}
            constexpr GUID VictimTransport = 
                { 0xc1806a47, 0x437e, 0x4c0a, { 0x81, 0x43, 0x90, 0x4a, 0x5f, 0x35, 0xaa, 0xd3 } };

            // {50118080-B6F7-4735-97D7-05316832C936}
            constexpr GUID VictimModulePath =
                { 0x50118080, 0xb6f7, 0x4735, { 0x97, 0xd7, 0x5, 0x31, 0x68, 0x32, 0xc9, 0x36 } };
        }

        inline flatbuffers::Offset<flatbuffers::String> CreateString(flatbuffers::FlatBufferBuilder& builder, const string_t& str = string_t())
            { return str.empty() ? flatbuffers::Offset<flatbuffers::String>() : builder.CreateString(string_to_u8s(str)); }

        inline string_t GetString(const flatbuffers::String* str, const string_t& def = string_t())
            { return str ? string_from(str->str(), nullptr) : def; }

        template<class Ty>
        inline buffer_t Serialize(const std::function<flatbuffers::Offset<Ty>(flatbuffers::FlatBufferBuilder&)>& ctor)
        {
            // FIXME: `thread_local` does not do initialization for pre-existing threads
            //thread_local static flatbuffers::FlatBufferBuilder builder;
            flatbuffers::FlatBufferBuilder builder;
            builder.Clear();
            builder.Finish(ctor(builder));
            return buffer_t(builder.GetBufferPointer(), builder.GetBufferPointer() + builder.GetSize());
        }

        template<class Ty>
        inline const Ty* Unserialize(const buffer_t& buffer)
        {
            auto ptr = flatbuffers::GetRoot<Ty>(buffer.data());
            auto verifier = flatbuffers::Verifier(buffer.data(), buffer.size());
            if(!ptr->Verify(verifier))
                throw Rpc::RpcTransportException("unserialize(): Invalid payload", Rpc::error_t::transport_code_t::invalid_payload);
            return ptr;
        }
    }
}