#pragma once

#include <cstdint>
#include <bitset>

#include <rbs/rbs.hpp>

namespace hz_client::message
{

struct frame_header
{
    enum frame_flags
    {
        DEFAULT_FLAGS = 0,
        BEGIN_FRAGMENT_FLAG = 1 << 15,
        END_FRAGMENT_FLAG = 1 << 14,
        UNFRAGMENTED_MESSAGE = BEGIN_FRAGMENT_FLAG | END_FRAGMENT_FLAG,
        IS_FINAL_FLAG = 1 << 13,
        BEGIN_DATA_STRUCTURE_FLAG = 1 << 12,
        END_DATA_STRUCTURE_FLAG = 1 << 11,
        IS_NULL_FLAG = 1 << 10,
        IS_EVENT_FLAG = 1 << 9,
        BACKUP_AWARE_FLAG = 1 << 8,
        BACKUP_EVENT_FLAG = 1 << 7
    };

    static inline constexpr int HEADER_SIZE = 6;

    int           length {HEADER_SIZE};
    std::uint16_t flags;
};

inline frame_header begin_frame()
{
    frame_header x {};

    x.flags = uint16_t(frame_header::BEGIN_DATA_STRUCTURE_FLAG);

    return x;
}

inline frame_header end_frame()
{
    frame_header x {};

    x.flags = uint16_t(frame_header::END_DATA_STRUCTURE_FLAG);

    return x;
}

inline frame_header null_frame()
{
    frame_header x {};

    x.flags = uint16_t(frame_header::IS_NULL_FLAG);

    return x;
}

inline frame_header final_frame()
{
    frame_header x = end_frame();

    x.flags |= uint16_t(frame_header::IS_FINAL_FLAG);

    return x;
}

template<auto... Args>
rbs::stream<Args...>& operator<<(
    rbs::stream<Args...>& ss ,
    const frame_header& x
)
{
    auto prev = ss.byte_order();
    ss.byte_order(rbs::endian::little);
    ss << x.length << x.flags;
    ss.byte_order(prev);

    return ss;
}

template<auto... Args>
rbs::stream<Args...>& operator>>(
    rbs::stream<Args...>& ss ,
    frame_header& x
)
{
    auto prev = ss.byte_order();
    ss.byte_order(rbs::endian::little);
    ss >> x.length >> x.flags;
    ss.byte_order(prev);

    return ss;
}

}