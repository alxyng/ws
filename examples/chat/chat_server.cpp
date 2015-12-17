//
// chat_server.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2015 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright (c) 2015 Alex Young (alexyoung91 at googlemail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <set>
#include <boost/asio.hpp>
#include "ws.hpp"
#include "chat_message.hpp"

using boost::asio::ip::tcp;

//----------------------------------------------------------------------

typedef std::deque<chat_message> chat_message_queue;

//----------------------------------------------------------------------

class chat_participant {
public:
    chat_participant(tcp::socket socket) : socket_(std::move(socket)) { }
    virtual ~chat_participant() {}
    virtual void deliver(const chat_message& msg) = 0;
protected:
    tcp::socket socket_;
};

//typedef std::shared_ptr<chat_participant> chat_participant_ptr;
typedef chat_participant * chat_participant_ptr;

//----------------------------------------------------------------------

class chat_room
{
public:
  void join(chat_participant_ptr participant)
  {
    participants_.insert(participant);
    for (auto msg: recent_msgs_)
      participant->deliver(msg);
  }

  void leave(chat_participant_ptr participant)
  {
    participants_.erase(participant);
  }

  void deliver(const chat_message& msg)
  {
    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > max_recent_msgs)
      recent_msgs_.pop_front();

    for (auto participant: participants_)
      participant->deliver(msg);
  }

private:
  std::set<chat_participant_ptr> participants_;
  enum { max_recent_msgs = 100 };
  chat_message_queue recent_msgs_;
};

//----------------------------------------------------------------------

using T = tcp::socket;
class chat_session : public chat_participant, public ws::session<T> {
public:
    chat_session(tcp::socket socket, chat_room& room) :
        chat_participant(std::move(socket)), ws::session<T>(socket_),
        room_(room) { }

    ~chat_session() { }

    void start() {
        ws::session<T>::start();
    }

private:
    void on_open() override {
        std::cout << "on_open\n";
        room_.join(this);

        // TODO: method to get headers
        // for (auto &kv : headers_)
        //     std::cout << kv.first << ": " << kv.second << std::endl;
        // std::cout << std::endl;
    }

    void on_msg(const ws::message &msg) override {
        std::cout << "on_msg\n";

        if (msg.get_opcode() != ws::message::opcode::text)
            return;

        const std::vector<unsigned char> &payload = msg.get_payload();

        if (payload.size() < 4 || payload.size() > chat_message::header_length +
            chat_message::max_body_length)
        {
            return;
        }

        std::memcpy(read_msg_.data(), payload.data(), payload.size());

        if (!read_msg_.decode_header())
            return;

        if (payload.size() != read_msg_.length())
            return;

        room_.deliver(read_msg_);
        read();
    }

    void on_close() override {
        std::cout << "on_close\n";
        room_.leave(this);
    }

    void on_error() override {
        std::cout << "on_error\n";
    }

    void deliver(const chat_message& msg) override {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress) {
            do_write();
        }
    }

    void do_write() {
        write(ws::message::opcode::text,
            boost::asio::buffer(write_msgs_.front().data(),
            write_msgs_.front().length()), [this]()
        {
            write_msgs_.pop_front();
            if (!write_msgs_.empty())
                do_write();
        });
    }

    chat_room& room_;
    chat_message read_msg_;
    chat_message_queue write_msgs_;
};

//----------------------------------------------------------------------

class chat_server {
public:
    chat_server(boost::asio::io_service& io_service,
        const tcp::endpoint& endpoint) :
        acceptor_(io_service, endpoint), socket_(io_service)
    {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(socket_,
            [this](boost::system::error_code ec)
        {
            if (!ec) {
                std::make_shared<chat_session>(
                    std::move(socket_), room_)->start();
            }

            do_accept();
        });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    chat_room room_;
};

//----------------------------------------------------------------------

int main(int, const char **) {
    const unsigned short PORT = 4567;

    try {
        boost::asio::io_service io_service;
        tcp::endpoint endpoint(tcp::v4(), PORT);
        chat_server server(io_service, endpoint);
        io_service.run();
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return EXIT_SUCCESS;
}
