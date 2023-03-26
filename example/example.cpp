#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <rbs/rbs.hpp>

#include <hz_client/connection.hpp>

int main()
{
    using boost::asio::io_context;
    using endpoint = boost::asio::ip::tcp::endpoint;

    io_context ctx;

    auto conn = std::make_shared<hz_client::connection>(ctx);

    hz_client::async_connect(
        *conn,
        endpoint
        {
            boost::asio::ip::address_v4::from_string( "127.0.0.1" ) ,
            5701
        },
        [conn](const boost::system::error_code& err)
        {
            if (err)
                std::cerr << "Could not connected." << std::endl;
            else
                std::cerr << "Successfully connected." << std::endl;

            hz_client::message::request<hz_client::message::authentication> req;

            req.instance_name = "hz_client1";
            req.cluster_name  = "dev";

            hz_client::async_authenticate(
                req,
                *conn,
                [conn](const boost::system::error_code& err){
                    if (err)
                        std::cout << "Could not authenticated" << std::endl;
                    else
                        std::cout << "Successfully authenticated." << std::endl;
                }
            );
        }
    );

    ctx.run();
}