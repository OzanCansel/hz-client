#include <iostream>
#include <thread>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/signal_set.hpp>

#include <rbs/rbs.hpp>

#include <hz_client/connection.hpp>
#include <hz_client/message/ping.hpp>

std::string map_name = "map";

static constexpr int MIN_CONCURRENT_INVOCATIONS = 50000;
std::atomic<int> n_concurrent_invocations = 0;
std::atomic<int> n_executed = 0;

void benchmark(std::shared_ptr<hz_client::connection> connection)
{
    while (n_concurrent_invocations < MIN_CONCURRENT_INVOCATIONS)
    {
        connection->invoke(
            hz_client::message::request<hz_client::message::map_put>{ {}, { map_name, rand() % 50000, rand() % 50000 } },
            [connection, start = std::chrono::steady_clock::now()](const boost::system::error_code& code, std::vector<char>)
            {
                n_concurrent_invocations--;
                n_executed++;

                using namespace std::chrono;

                auto elapsed = steady_clock::now() - start;
                // std::cout << std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count() << std::endl;

                benchmark(connection);
            }
        );

        n_concurrent_invocations++;
    }
}

void connect(boost::asio::io_context& ctx)
{
    using endpoint = boost::asio::ip::tcp::endpoint;

    auto conn = std::make_shared<hz_client::connection>(ctx);

    conn->async_connect(
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

            hz_client::message::authentication req;

            static std::atomic<int> instance_id = 1;

            req.instance_name = "hz_client" + std::to_string(instance_id);
            req.cluster_name  = "dev";
            req.uid = boost::uuids::random_generator()();

            conn->async_authenticate(
                req,
                [conn](const boost::system::error_code& err){
                    if (err)
                        std::cout << "Could not authenticated" << std::endl;
                    else
                    {
                        benchmark(conn);
                        std::cout << "Successfully authenticated." << std::endl;
                    }
                }
            );
        }
    );
}

void print_speed(
    const boost::system::error_code& err,
    boost::asio::steady_timer& timer
)
{
    std::cout << "Ongoing invocations : " << n_concurrent_invocations
              << " IPS :" << n_executed
              << std::endl;

    n_executed = 0;

    timer.expires_at(
        timer.expiry() +
        std::chrono::seconds {1}
    );

    timer.async_wait(
        [&timer](const boost::system::error_code& err){
            print_speed(err, timer);
        }
    );
}

int main()
{
    using boost::asio::io_context;
    using boost::asio::signal_set;

    if (auto name = std::getenv("MAP_NAME"))
        map_name = name;

    io_context ctx;
    signal_set signals {ctx, SIGINT};

    signals.async_wait(
        [](const boost::system::error_code& err, int signal_number){
            std::cout << "handling signal" << signal_number << std::endl;
            exit(1);
        }
    );

    boost::asio::steady_timer timer {ctx, std::chrono::seconds{1}};

    print_speed({}, timer);
    connect(ctx);

    std::vector<std::thread> threads;

    return ctx.run();
}