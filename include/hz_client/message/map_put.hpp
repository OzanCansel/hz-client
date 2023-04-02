#pragma once

#include <rbs/stream.hpp>

#include "hz_client/message/string_serialization.hpp"
#include "hz_client/message/frame_header.hpp"

namespace hz_client::message
{

struct map_put
{
    std::string map_name;
    int key;
    int value;
};

template<auto... Args>
inline rbs::stream<Args...>&
operator<<(
    rbs::stream<Args...>& ss ,
    const map_put& x
)
{
    using namespace std::string_literals;

    frame_header key_header;

    key_header.flags  = 0;
    key_header.length = 18;

    auto prev = ss.byte_order();
    ss.byte_order(rbs::endian::big);

    ss << x.map_name
       << key_header
       << 0
       << -7
       << x.key
       << key_header
       << 0
       << -7
       << x.value;

    ss.byte_order(prev);

    return ss;
}

template<auto... Args>
inline rbs::stream<Args...>&
operator>>(
    rbs::stream<Args...>& ss ,
    map_put& x
)
{
    return ss;
}

}