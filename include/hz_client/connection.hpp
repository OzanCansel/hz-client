#pragma once

#include <memory>
#include <functional>

#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context_strand.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <rbs/rbs.hpp>

#include "hz_client/message/authentication.hpp"
#include "hz_client/message/request.hpp"
#include "hz_client/util/make_shallow_copyable.hpp"

namespace hz_client
{

class connection
{
    using io_context = boost::asio::io_context;
    using socket     = boost::asio::ip::tcp::socket;
    using strand     = boost::asio::io_context::strand;
    using endpoint   = boost::asio::ip::tcp::endpoint;
    using streambuf  = boost::asio::streambuf;
    using error_code = boost::system::error_code;

public:

    explicit inline connection(io_context& ctx);

    inline socket& sck();

    template<typename CompletionToken>
    auto async_connect(
        boost::asio::ip::tcp::endpoint ep,
        CompletionToken&& token
    );

    template<typename CompletionToken>
    auto async_authenticate(
        message::authentication req,
        CompletionToken&& token
    );

    template<typename T, typename CompletionToken>
    auto invoke(
        message::request<T> message,
        CompletionToken&& token
    );

private:

    inline void start_reader();
    inline void do_write();
    inline void on_frame_header_read(const error_code& err);
    inline void on_frame_read(const error_code& err, int n_read, bool is_final);
    inline void toggle_write_buffer();
    inline streambuf& writing_buffer();
    inline streambuf& pending_buffer();

    using byte_array_t    = std::vector<char>;
    using invocation_cb_t = std::function<void(error_code, byte_array_t)>;
    using handlers_t      = std::unordered_map<int64_t, invocation_cb_t>;

    byte_array_t m_received_message;
    bool         m_write_in_progress;
    uint64_t     m_correlation_id;
    handlers_t   m_handlers;
    int          m_active_buffer_idx;
    streambuf    m_write_buffers[2];
    streambuf    m_read_buffer;
    socket       m_sck;
};

}

#include "hz_client/impl/connection_impl.hpp"