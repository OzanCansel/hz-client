#pragma once

#include <rbs/stream.hpp>

#include "hz_client/message/frame_header.hpp"

namespace hz_client::message
{

template<typename Iter>
struct range
{
    Iter beg;
    Iter end;
};

template<typename Iter>
range(Iter, Iter) -> range<Iter>;

template<auto... Args, typename Iter>
inline rbs::stream<Args...>&
operator<<(rbs::stream<Args...>& ss, const range<Iter>& x)
{
    ss << begin_frame();

    for (auto it = x.beg; it != x.end; ++it)
        ss << *it;

    ss << end_frame();

    return ss;
}

}