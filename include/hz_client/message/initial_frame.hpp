#pragma once

#include <cstdint>

#include "frame_header.hpp"

namespace hz_client::message
{

struct initial_frame : frame_header
{
    std::int32_t  type;
    std::uint64_t correlation_id;
    std::int32_t  partition_id;
};

template<auto... Args>
inline rbs::stream<Args...>&
operator<<(
    rbs::stream<Args...>& ss ,
    const initial_frame& frame
)
{
    return ss << static_cast<const frame_header&>(frame)
              << frame.type
              << frame.correlation_id
              << frame.partition_id;
}

template<auto... Args>
inline rbs::stream<Args...>&
operator>>(
    rbs::stream<Args...>& ss ,
    initial_frame& frame
)
{
    return ss >> static_cast<frame_header&>(frame)
              >> frame.type
              >> frame.correlation_id
              >> frame.partition_id;
}

}