#ifndef WS_SERVER_HPP
#define WS_SERVER_HPP

#include <memory>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

namespace ws {

template <typename T>
class server {
public:
    server(boost::asio::io_service &io_service,
        const tcp::endpoint& endpoint) :
        io_service_(io_service), acceptor_(io_service, endpoint),
        socket_(io_service)
    {
        accept();
    }

    void accept() {
        acceptor_.async_accept(socket_,
            [this](const boost::system::error_code &ec)
        {
            if (!ec) {
                std::make_shared<T>(std::move(socket_))->start();
            }
            accept();
        });
    }
private:
    boost::asio::io_service &io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

} /* namespace ws */

#endif /* WS_SERVER_HPP */
