#pragma once

#include <memory>
#include <iostream>
#include <functional>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/compose.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>

#include <rbs/rbs.hpp>

#include "hz_client/message/authentication.hpp"
#include "hz_client/message/request.hpp"

namespace hz_client
{

class connection :
      public std::enable_shared_from_this<connection>
{
    using io_context = boost::asio::io_context;
    using socket     = boost::asio::ip::tcp::socket;
    using endpoint   = boost::asio::ip::tcp::endpoint;
    using streambuf  = boost::asio::streambuf;
    using error_code = boost::system::error_code;

public:

    connection(io_context& ctx)
        :   m_sck {ctx}
    {}

    socket& sck()
    {
        return m_sck;
    }

    boost::asio::streambuf& read_buffer()
    {
        return m_read_buffer;
    }

    boost::asio::streambuf& write_buffer()
    {
        return m_write_buffer;
    }

    void start_reader()
    {
        boost::asio::async_read(
            m_sck,
            m_read_buffer,
            boost::asio::transfer_exactly(message::frame_header::HEADER_SIZE),
            bind(
                &connection::on_frame_header_read,
                shared_from_this(),
                std::placeholders::_1
            )
        );
    }

private:

    // void on_connected(const error_code& err)
    // {
    //     if (err)
    //     {
    //         std::cerr << "Error occurred." << std::endl;

    //         return;
    //     }

    //     boost::asio::async_write(
    //         m_sck,
    //         boost::asio::buffer("CP2", 3),
    //         bind(
    //             &connection::on_protocol_sent ,
    //             shared_from_this() ,
    //             std::placeholders::_1
    //         )
    //     );
    //     boost::asio::async_read(
    //         m_sck,
    //         m_read_buffer,
    //         boost::asio::transfer_exactly(message::frame_header::HEADER_SIZE),
    //         bind(
    //             &connection::on_frame_header_read,
    //             shared_from_this() ,
    //             std::placeholders::_1
    //         )
    //     );
    // }

    // void on_protocol_sent(const error_code& err)
    // {
    //     std::cout << "protocol_sent" << std::endl;

    //     message::request<message::authentication> auth_req;

    //     auth_req.cluster_name = "dev";
    //     auth_req.uid = boost::uuids::uuid(boost::uuids::random_generator()());
    //     auth_req.instance_name = "hz.client_2";
    //     auth_req.header.correlation_id = rand();

    //     rbs::serialize_le(auth_req, m_write_buffer);

    //     boost::asio::async_write(
    //         m_sck,
    //         m_write_buffer,
    //         [](const error_code& err, std::size_t n_written){

    //         }
    //     );
    // }

    void on_frame_header_read(const error_code& err)
    {
        std::cout << "frame_header__read" << std::endl;
        message::frame_header header;
        rbs::le_stream ss {m_read_buffer};

        ss >> header;
        ss << header;

        if (header.flags | message::frame_header::IS_FINAL_FLAG)
            std::cout << "Final flag is seen" << std::endl;

        if (header.length > message::frame_header::HEADER_SIZE)
        {
            boost::asio::async_read(
                m_sck,
                m_read_buffer,
                boost::asio::transfer_exactly(header.length - 6),
                bind(
                    &connection::on_frame_read ,
                    shared_from_this() ,
                    std::placeholders::_1
                )
            );
        }
        else
        {
            boost::asio::async_read(
                m_sck,
                m_read_buffer,
                boost::asio::transfer_exactly(6),
                bind(
                    &connection::on_frame_header_read,
                    shared_from_this() ,
                    std::placeholders::_1
                )
            );
        }
    }

    void on_frame_read(const error_code& err)
    {
        std::cout << "frame_read" << std::endl;
        boost::asio::async_read(
            m_sck,
            m_read_buffer,
            boost::asio::transfer_exactly(6),
            bind(
                &connection::on_frame_header_read,
                shared_from_this() ,
                std::placeholders::_1
            )
        );
    }

    std::unordered_map<int, std::function<void()>> m_handlers;
    streambuf m_write_buffer;
    streambuf m_read_buffer;
    socket    m_sck;
};

template<typename Connection, typename CompletionToken>
auto async_connect(
    Connection& conn,
    boost::asio::ip::tcp::endpoint ep,
    CompletionToken&& token
)
{
    enum {started, connecting, start_reader, protocol_sending};

    return boost::asio::async_compose<
        CompletionToken, void(boost::system::error_code)
    >
    (
        [
            conn = &conn,
            state = started,
            host = std::move(ep)
        ]
        (
            auto& self,
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
                            std::move(self)
                        );

                        return;
                    }
                    case connecting:
                    {
                        state = start_reader;
                        conn->start_reader();

                        self();

                        return;
                    }
                    case start_reader:
                    {
                        state = protocol_sending;
                        boost::asio::async_write(
                            conn->sck(),
                            boost::asio::buffer("CP2", 3),
                            [self = std::move(self)]
                            (const boost::system::error_code& err, std::size_t) mutable
                            {
                                self(err);
                            }
                        );

                        return;
                    }
                }
            }

            self.complete(error);
        },
        token
    );
}

template<typename Connection, typename CompletionToken>
auto async_authenticate(
    const message::request<message::authentication>& req,
    Connection& conn,
    CompletionToken&& token
)
{
    enum {starting, authenticating};

    req.header.correlation_id = rand();
    rbs::serialize_le(req, conn.write_buffer());

    return boost::asio::async_compose<
        CompletionToken, void(boost::system::error_code)
    >(
        [
            conn = &conn,
            state = starting
        ]
        (
            auto& self,
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

                        boost::asio::async_write(
                            conn->sck(),
                            conn->write_buffer(),
                            [self = std::move(self)]
                            (const boost::system::error_code& err, std::size_t) mutable
                            {
                                self(err);
                            }
                        );

                        return;
                    }
                }
            }

            self.complete(error);
        },
        token
    );
}

}