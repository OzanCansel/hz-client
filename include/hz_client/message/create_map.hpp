#pragma once

#include <rbs/stream.hpp>

#include "hz_client/message/string_serialization.hpp"

namespace hz_client::message
{

struct create_map
{
    std::string name;
};

template<auto... Args>
inline rbs::stream<Args...>&
operator<<(
    rbs::stream<Args...>& ss ,
    const create_map& x
)
{
    using namespace std::string_literals;

    return ss << x.name << std::string {"hz:impl:mapService"};
}

template<auto... Args>
inline rbs::stream<Args...>&
operator>>(
    rbs::stream<Args...>& ss ,
    create_map& x
)
{
    return ss;
}

}