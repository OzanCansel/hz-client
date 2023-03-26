#pragma once

#include <type_traits>
#include <memory>

namespace hz_client
{

template <typename F>
struct make_shallow_copyable
{
    make_shallow_copyable(F&& f)
        :   m_functor{
                std::make_shared<F>(
                    std::move(f)
                )
            }
    {}

    template<typename... Tn>
    auto operator()(Tn&&... args)
    {
        return (*m_functor)(std::forward<Tn>(args)...);
    }

private:

    std::shared_ptr<F> m_functor;
};

}