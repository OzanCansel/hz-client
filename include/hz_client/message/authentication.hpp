#pragma once

#include <string>
#include <vector>
#include <optional>

#include <boost/uuid/uuid.hpp>
#include <rbs/stream.hpp>

#include "hz_client/message/initial_frame.hpp"
#include "hz_client/message/uuid_serialization.hpp"
#include "hz_client/message/string_serialization.hpp"
#include "hz_client/message/optional_serialization.hpp"
#include "hz_client/message/range_serialization.hpp"

namespace hz_client::message
{

struct authentication
{
    std::string cluster_name;
    boost::uuids::uuid uid;
    std::string instance_name;
    std::vector<std::string> labels;
    std::optional<std::string> username;
    std::optional<std::string> password;
};

template<auto... Args>
inline rbs::stream<Args...>&
operator<<(
    rbs::stream<Args...>& ss ,
    const authentication& auth
)
{
    using namespace std::string_literals;

    return ss << auth.uid
              << std::uint8_t(1)
              << auth.cluster_name
              << auth.username
              << auth.password
              << "CPP"s
              << "5.1.0"s
              << auth.instance_name
              << range { begin(auth.labels), end(auth.labels) };
}

template<auto... Args>
inline rbs::stream<Args...>&
operator>>(
    rbs::stream<Args...>& ss ,
    authentication& auth
)
{
    return ss;
}

}