#pragma once

#include <optional>

#include <rbs/stream.hpp>

#include "hz_client/message/frame_header.hpp"

namespace std
{

template<auto... Args, typename T>
inline rbs::stream<Args...>&
operator<<(rbs::stream<Args...>& ss, const std::optional<T>& x)
{
    if (x.has_value())
    {
        return ss << x.value();
    }
    else
    {
        using namespace hz_client::message;

        frame_header header {};

        header.flags = uint16_t(frame_header::IS_NULL_FLAG);

        return ss << header;
    }
}

}