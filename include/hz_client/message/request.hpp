#pragma once

#include "hz_client/message/initial_frame.hpp"
#include "hz_client/message/range_serialization.hpp"
#include "hz_client/message/authentication.hpp"
#include "hz_client/message/create_map.hpp"
#include "hz_client/message/map_put.hpp"
#include "hz_client/message/ping.hpp"

namespace hz_client::message
{

template<typename T>
struct request
{
    mutable initial_frame header;
    T entity;
};

template<auto... Args>
rbs::stream<Args...>&
operator<<(
    rbs::stream<Args...>& ss ,
    const request<authentication>& req
)
{
    req.header.length = 40;

    req.header.flags = uint16_t(
        frame_header::BEGIN_FRAGMENT_FLAG |
        frame_header::END_FRAGMENT_FLAG
    );

    req.header.type           = 256;
    req.header.partition_id   = -1;

    return ss << req.header
              << req.entity
              << final_frame();
}

template<auto... Args>
rbs::stream<Args...>&
operator<<(
    rbs::stream<Args...>& ss ,
    const request<ping>& req
)
{
    req.header.length = 22;

    req.header.flags = uint16_t(
        frame_header::BEGIN_FRAGMENT_FLAG |
        frame_header::END_FRAGMENT_FLAG |
        frame_header::IS_FINAL_FLAG
    );

    req.header.type           = 2816;
    req.header.partition_id   = -1;

    return ss << req.header;
}

template<auto... Args>
rbs::stream<Args...>&
operator<<(
    rbs::stream<Args...>& ss ,
    const request<create_map>& req
)
{
    req.header.length = 22;

    req.header.flags = uint16_t(
        frame_header::BEGIN_FRAGMENT_FLAG |
        frame_header::END_FRAGMENT_FLAG
    );

    req.header.type           = 1024;
    req.header.partition_id   = -1;

    return ss << req.header << req.entity << final_frame();
}

template<auto... Args>
rbs::stream<Args...>&
operator<<(
    rbs::stream<Args...>& ss ,
    const request<map_put>& req
)
{
    req.header.length = 38;

    req.header.flags = uint16_t(
        frame_header::BEGIN_FRAGMENT_FLAG |
        frame_header::END_FRAGMENT_FLAG
    );

    req.header.type           = 65792;
    req.header.partition_id   = 50;

    return ss << req.header
              << std::int64_t(5000)
              << std::int64_t(-1)
              << req.entity
              << final_frame();
}

}