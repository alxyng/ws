#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <random>
#include <boost/asio.hpp>
#include "ws.hpp"

using boost::asio::ip::tcp;

using T = tcp::socket;
class session : public ws::session<T> {
public:
    session(tcp::socket socket) :
        ws::session<T>(socket_), socket_(std::move(socket)),
        timer_(socket_.get_io_service()), unif_(-1.0, 1.0), angle_(0.0)
    {
        std::cout << "session()\n";
    }

    ~session() {
        std::cout << "~session()\n";
    }

private:
    tcp::socket socket_;
    boost::asio::deadline_timer timer_;
    std::uniform_real_distribution<double> unif_;
    std::default_random_engine re_;
    double angle_;

    void small_change() {
        double delta = unif_(re_);
        angle_ += delta;
        if (angle_ < -20.0)
            angle_ = -20.0;
        else if (angle_ > 20.0)
            angle_ = 20.0;
    }

    void timer_cb(const boost::system::error_code &ec) {

        if (!ec) {
            write(ws::message::opcode::binary,
                boost::asio::buffer(&angle_, sizeof (angle_)), [this]()
            {
                auto self(shared_from_this());
                small_change();
                timer_.expires_from_now(boost::posix_time::milliseconds(100));
                timer_.async_wait([this, self](const boost::system::error_code &ec) {
                    timer_cb(ec);
                });
            });
        }
    }

    void on_open() override {
        auto self(shared_from_this());
        std::cout << "WebSocket connection open\n";
        timer_.expires_from_now(boost::posix_time::milliseconds(100));
        timer_.async_wait([this, self](const boost::system::error_code &ec) {
            timer_cb(ec);
        });
    }

    void on_msg(const ws::message &) override {
        std::cout << "WebSocket message received\n";
        read();
    }

    void on_close() override {
        std::cout << "WebSocket connection closed\n";
    }

    void on_error() override {
        std::cout << "WebSocket connection error\n";
    }
};

class server {
public:
    server(boost::asio::io_service& io_service,
        const tcp::endpoint& endpoint) :
        acceptor_(io_service, endpoint), socket_(io_service)
    {
        accept();
    }

private:
    tcp::acceptor acceptor_;
    tcp::socket socket_;

    void accept() {
        acceptor_.async_accept(socket_,
            [this](const boost::system::error_code &ec)
        {
            if (!ec) {
                std::make_shared<session>(std::move(socket_))->start();
            }

            accept();
        });
    }
};

int main(int, const char **) {
    const unsigned short PORT = 4567;

    try {
        boost::asio::io_service io_service;
        tcp::endpoint endpoint(tcp::v4(), PORT);
        server server(io_service, endpoint);
        io_service.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return EXIT_SUCCESS;
}
