#pragma once

#include <algorithm>

#include <boost/uuid/uuid.hpp>

#include <rbs/stream.hpp>

namespace boost::uuids
{

template<auto... Args>
inline rbs::stream<Args...>&
operator<<(
    rbs::stream<Args...>& ss ,
    const uuid& uid
)
{
    ss << uid.is_nil();

    if (uid.is_nil()) {
        return ss;
    }

    auto copy_uid = uid;

    std::reverse(
        std::begin(copy_uid.data),
        std::begin(copy_uid.data) + 8
    );
    std::reverse(
        std::begin(copy_uid.data) + 8,
        std::end(copy_uid.data)
    );

    ss << copy_uid.data;

    return ss;
}

}