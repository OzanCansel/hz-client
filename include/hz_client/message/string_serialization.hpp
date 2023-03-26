#pragma once

#include <string>

#include <rbs/stream.hpp>

#include "hz_client/message/frame_header.hpp"

namespace std
{

template<auto... Args>
inline rbs::stream<Args...>&
operator<<(rbs::stream<Args...>& ss, const std::string& x)
{
    hz_client::message::frame_header header {};

    header.length = 6 + x.length();

    ss << header;
    ss.write(x.data(), x.length());

    return ss;
}

}