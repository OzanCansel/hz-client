#pragma once

#include "hz_client/connection.hpp"
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream_buffer.hpp>

namespace hz_client
{

inline connection::connection(io_context& ctx)
    :   m_write_in_progress {false}
    ,   m_sck {ctx}
    ,   m_active_buffer_idx {0}
    ,   m_correlation_id {1}
{}

inline boost::asio::ip::tcp::socket&
connection::sck()
{
    return m_sck;
}

template<typename T, typename CompletionToken>
auto connection::invoke(
    message::request<T> message,
    CompletionToken&& token
)
{
    enum state { starting, response_awaiting };

    return boost::asio::async_compose<
        CompletionToken, void(boost::system::error_code, std::vector<char>)
    >(
        [
            this,
            state   = starting,
            message = std::move(message)
        ]
        (
            auto& composable,
            const boost::system::error_code& err = {},
            std::vector<char> response = {}
        ) mutable
        {
            if (!err)
            {
                switch (state)
                {
                    case starting:
                    {
                        state = response_awaiting;

                        message.header.correlation_id = m_correlation_id++;
                        rbs::serialize_le(message, pending_buffer());

                        m_handlers.emplace(
                            std::make_pair(
                                message.header.correlation_id,
                                make_shallow_copyable(
                                    [
                                        composable = std::move(composable)
                                    ]
                                    (const boost::system::error_code& err, std::vector<char> response) mutable
                                    {
                                        composable(err, move(response));
                                    }
                                )
                            )
                        );

                        do_write();

                        return;
                    }
                }
            }

            return composable.complete(err, move(response));
        },
        token
    );
}

inline void connection::start_reader()
{
    boost::asio::async_read(
        m_sck,
        m_read_buffer,
        boost::asio::transfer_exactly(message::frame_header::HEADER_SIZE),
        bind(
            &connection::on_frame_header_read,
            this,
            std::placeholders::_1
        )
    );
}

inline void connection::do_write()
{
   if (m_write_in_progress)
        return;

    m_write_in_progress = true;

    toggle_write_buffer();
    boost::asio::async_write(
        m_sck,
        writing_buffer().data(),
        [this](const boost::system::error_code& ec, std::size_t n){
            m_write_in_progress = false;
            if (ec) {
                throw std::runtime_error {
                    "Write error."
                };
            }

            writing_buffer().consume(n);
        }
    );
}

inline void connection::on_frame_header_read(const error_code& err)
{
    message::frame_header header;
    rbs::stream ss {m_read_buffer, rbs::endian::little};

    ss >> header;
    ss << header;

    bool is_final { bool(header.flags & message::frame_header::IS_FINAL_FLAG) };

    if (header.length > message::frame_header::HEADER_SIZE)
    {
        int needs_to_be_read {header.length - message::frame_header::HEADER_SIZE};

        boost::asio::async_read(
            m_sck,
            m_read_buffer,
            boost::asio::transfer_exactly(needs_to_be_read),
            bind(
                &connection::on_frame_read,
                this,
                std::placeholders::_1,
                header.length,
                is_final
            )
        );
    }
    else
    {
        on_frame_read(error_code{}, message::frame_header::HEADER_SIZE, is_final);
    }    
}

inline void connection::on_frame_read(const error_code& err, int n_read, bool is_final)
{
    if (err) {
        throw std::runtime_error {
            "Error occurred."
        };
    }

    auto buffer = m_read_buffer.data();

    copy(
        buffers_begin(buffer),
        buffers_end(buffer),
        back_inserter(m_received_message)
    );
    m_read_buffer.consume(n_read);

    if (is_final)
    {
        boost::iostreams::stream_buffer<boost::iostreams::array_source> rd_buffer {
            data(m_received_message),
            size(m_received_message)
        };

        rbs::stream is {rd_buffer, rbs::endian::little};

        message::initial_frame iframe;

        is >> iframe;

        auto handler = m_handlers.find(iframe.correlation_id);

        if (handler != end(m_handlers))
        {
            handler->second(boost::system::error_code{}, move(m_received_message));
            m_handlers.erase(handler);
        }
    }

    boost::asio::async_read(
        m_sck,
        m_read_buffer,
        boost::asio::transfer_exactly(message::frame_header::HEADER_SIZE),
        bind(
            &connection::on_frame_header_read,
            this,
            std::placeholders::_1
        )
    );
}

inline void connection::toggle_write_buffer()
{
    m_active_buffer_idx = !m_active_buffer_idx;
}

inline boost::asio::streambuf& connection::writing_buffer()
{
    return m_write_buffers[m_active_buffer_idx];
}

inline boost::asio::streambuf& connection::pending_buffer()
{
    return m_write_buffers[!m_active_buffer_idx];
}

template<typename CompletionToken>
auto connection::async_connect(
    boost::asio::ip::tcp::endpoint ep,
    CompletionToken&& token
)
{
    enum {started, connecting, start_read, protocol_sending};

    return boost::asio::async_compose<
        CompletionToken, void(boost::system::error_code)
    >
    (
        [
            this,
            state = started,
            host = std::move(ep)
        ]
        (
            auto& composable,
            const boost::system::error_code& error = {}
        ) mutable
        {
            if (!error)
            {
                switch (state)
                {
                    case started:
                    {
                        state = connecting;
                        sck().async_connect(
                            host ,
                            std::move(composable)
                        );

                        return;
                    }
                    case connecting:
                    {
                        state = start_read;
                        start_reader();

                        composable();

                        return;
                    }
                    case start_read:
                    {
                        state = protocol_sending;
                        boost::asio::async_write(
                            m_sck,
                            boost::asio::buffer("CP2", 3),
                            [
                                composable = std::move(composable)
                            ]
                            (const boost::system::error_code& err, std::size_t) mutable
                            {
                                composable(err);
                            }
                        );

                        return;
                    }
                }
            }

            composable.complete(error);
        },
        token
    );
}

template<typename CompletionToken>
auto connection::async_authenticate(
    message::authentication req,
    CompletionToken&& token
)
{
    enum {starting, authenticating};

    return boost::asio::async_compose<
        CompletionToken, void(boost::system::error_code)
    >(
        [
            this,
            req   = std::move(req),
            state = starting
        ]
        (
            auto& composable,
            const boost::system::error_code& error = {}
        ) mutable
        {
            if (!error)
            {
                switch (state)
                {
                    case starting:
                    {
                        state = authenticating;

                        message::request<message::authentication> message;

                        message.entity = std::move(req);

                        invoke(
                            std::move(message),
                            make_shallow_copyable(
                                [composable = std::move(composable)](
                                    const boost::system::error_code& err,
                                    std::vector<char> response
                                ) mutable
                                {
                                    composable(err);
                                }
                            )
                        );

                        return;
                    }
                }
            }

            composable.complete(error);
        },
        token
    );
}

}