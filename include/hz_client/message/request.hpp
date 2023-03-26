#pragma once

#include "hz_client/message/initial_frame.hpp"
#include "hz_client/message/range_serialization.hpp"

namespace hz_client::message
{

template<typename... Ts>
struct request : Ts...
{
    mutable initial_frame header;
};

template<auto... Args>
rbs::stream<Args...>&
operator<<(
    rbs::stream<Args...>& ss ,
    const request<authentication>& req
)
{
    using namespace std::string_literals;

    req.header.length = 40;

    req.header.flags = uint16_t(frame_header::BEGIN_FRAGMENT_FLAG | frame_header::END_FRAGMENT_FLAG);

    req.header.type           = 256;
    req.header.partition_id   = -1;

    return ss << req.header
              << static_cast<const authentication&>(req)
              << final_frame();
}

}