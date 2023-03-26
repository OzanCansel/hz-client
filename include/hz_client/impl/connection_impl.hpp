#pragma once

#include "hz_client/connection.hpp"

namespace hz_client
{

inline connection::connection(io_context& ctx)
    :   m_write_in_progress {false}
    ,   m_sck {ctx}
    ,   m_strand {ctx}
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
            state   = starting,
            self    = shared_from_this(),
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

                        self->m_strand.post(
                            make_shallow_copyable(
                                [
                                    self       = move(self),
                                    message    = std::move(message),
                                    composable = std::move(composable)
                                ]() mutable {
                                    message.header.correlation_id = self->m_correlation_id++;
                                    rbs::serialize_le(message, self->pending_buffer());

                                    self->m_handlers.emplace(
                                        std::make_pair(
                                            message.header.correlation_id,
                                            make_shallow_copyable(
                                                [
                                                    composable = std::move(composable),
                                                    self       = self
                                                ]
                                                (const boost::system::error_code& err, std::vector<char> response) mutable
                                                {
                                                    composable(err, move(response));
                                                }
                                            )
                                        )
                                    );

                                    self->do_write();
                                }
                            )
                        );

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
        m_strand.wrap(
            bind(
                &connection::on_frame_header_read,
                shared_from_this(),
                std::placeholders::_1
            )
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
        m_strand.wrap(
            [
                self = shared_from_this()
            ](const boost::system::error_code& ec, std::size_t n){
                self->m_write_in_progress = false;
                if (ec) {
                    throw std::runtime_error {
                        "Write error."
                    };
                }

                self->writing_buffer().consume(n);
            }
        )
    );
}

inline void connection::on_frame_header_read(const error_code& err)
{
    message::frame_header header;
    rbs::le_stream ss {m_read_buffer};

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
            m_strand.wrap(
                bind(
                    &connection::on_frame_read,
                    shared_from_this(),
                    std::placeholders::_1,
                    header.length,
                    is_final
                )
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

        rbs::le_stream is {rd_buffer};

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
        m_strand.wrap(
            bind(
                &connection::on_frame_header_read,
                shared_from_this() ,
                std::placeholders::_1
            )
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
    enum {started, connecting, start_reader, protocol_sending};
    auto conn = shared_from_this();

    return boost::asio::async_compose<
        CompletionToken, void(boost::system::error_code)
    >
    (
        [
            conn,
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
                        conn->sck().async_connect(
                            host ,
                            std::move(composable)
                        );

                        return;
                    }
                    case connecting:
                    {
                        state = start_reader;
                        conn->start_reader();

                        composable();

                        return;
                    }
                    case start_reader:
                    {
                        state = protocol_sending;
                        auto& sck = conn->sck();
                        boost::asio::async_write(
                            sck,
                            boost::asio::buffer("CP2", 3),
                            [
                                conn       = conn,
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
    auto conn = shared_from_this();

    return boost::asio::async_compose<
        CompletionToken, void(boost::system::error_code)
    >(
        [
            conn,
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

                        conn->invoke(
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