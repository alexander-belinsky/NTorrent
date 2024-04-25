#pragma once
#include "netlib_header.h"
#include "netlib_server.h"

namespace netlib {

    class StunSession {
    public:
        StunSession(std::string &stun_host, uint16_t port, asio::io_context *context, asio::ip::udp::socket &sock):
                socket_(sock), context_(context), timer(*context) {
            asio::ip::udp::resolver resolver(*context_);
            asio::ip::udp::resolver::query query(stun_host, std::to_string(port));
            asio::ip::udp::resolver::iterator iter = resolver.resolve(query);
            ep_ = *iter;
            reqString_ = {0 ,1 ,0 ,0 ,30 ,180 ,81 ,146 ,9 ,82 ,31 ,207 ,116 ,119 ,214 ,159 ,145 ,171 ,162 ,112};
            ansString_.resize(32);
            isRunning = true;
        }

        void sendRequest() {
            if (!isRunning)
                return;
            socket_.async_send_to(asio::buffer(reqString_.data(), reqString_.size()), ep_,
                                  [this] (std::error_code er, size_t length) {
                                      if (!er) {
                                          if (isRunning)
                                              getAnswer();
                                      } else {
                                          std::cerr << "STUN sending error: " << er.message() << "\n";
                                      }
                                  });
        }

        std::string getIpFromBytes() {
            std::string res;
            for (int i = 28; i < 32; i++) {
                res += std::to_string((int) ansString_[i]);
                if (i != 31)
                    res += '.';
            }
            return res;
        }

        uint32_t getPortFromBytes() {
            uint32_t res = 0;
            for (int i = 24; i < 28; i++)
                res = (res << 8) + ansString_[i];
            return res;
        }

        void getAnswer() {
            socket_.async_receive_from(asio::buffer(ansString_), tempEp_,
                                       [this](std::error_code er, size_t length) {
                                           if (!er) {
                                               if (!isRunning)
                                                   return;
                                               if (tempEp_ == ep_) {
                                                   std::string realHost = getIpFromBytes();
                                                   uint32_t realPort = getPortFromBytes();
                                                   realEp_ = asio::ip::udp::endpoint(
                                                           asio::ip::address_v4::from_string(realHost), realPort);
                                               }
                                               timer.expires_after(std::chrono::milliseconds(5000));
                                               timer.async_wait(
                                                       [this](asio::error_code ec) {
                                                           if (!ec) {
                                                               if (isRunning)
                                                                   sendRequest();
                                                           } else {
                                                               std::cerr << "Waiting error: " << ec.message() << "\n";
                                                           }
                                                       }
                                               );
                                           } else {
                                               std::cerr << er.message() << "\n";
                                           }
                                       });
        }

        asio::ip::udp::endpoint getEndpoint() {
            return realEp_;
        }

        void start() {
            socket_.send_to(asio::buffer(reqString_.data(), reqString_.size()), ep_);
            socket_.receive_from(asio::buffer(ansString_), tempEp_);
            if (tempEp_ == ep_) {
                std::string realHost = getIpFromBytes();
                uint32_t realPort = getPortFromBytes();
                realEp_ = asio::ip::udp::endpoint(
                        asio::ip::address_v4::from_string(realHost), realPort);
            }
            sendRequest();
        }

        void stop() {
            isRunning = false;
        }

    private:
        asio::steady_timer timer;
        asio::ip::udp::endpoint ep_;
        asio::ip::udp::endpoint tempEp_;

        asio::ip::udp::endpoint realEp_;
        asio::ip::udp::socket& socket_;
        asio::io_context* context_;
        std::vector<uint8_t> reqString_;
        std::vector<uint8_t> ansString_;
        bool isRunning;
    };
}