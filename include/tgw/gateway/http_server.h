#pragma once

#include "tgw/common/config.h"
#include "tgw/gateway/router.h"

#include <atomic>
#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace tgw
{
    class HttpServer
    {
        public:
            HttpServer(ServerConfig server_config,RuntimeConfig runtime_config,std::shared_ptr<Router> router);

            void Start();

        private:
            void DoAccept();

            std::string GenerateRequestId();

        private:
            ServerConfig server_config_;
            RuntimeConfig runtime_config_;
            std::shared_ptr<Router> router_;
        
            boost::asio::io_context io_context_;
            boost::asio::ip::tcp::acceptor acceptor_;
            boost::asio::thread_pool worker_pool_;
            
            std::atomic<uint64_t> request_seq_{0};
    };
}
