#pragma once

#include <map>
#include <string>

namespace tgw
{
    struct HttpRequest
    {
        std::string method;
        std::string path;
        std::string query;
        std::string body;

        std::map<std::string, std::string> headers;

        std::string client_ip;
        std::string request_id;
    };

    struct HttpResponse 
    {
        int status = 200;
        std::string content_type = "application/json";
        std::string body;
        std::map<std::string, std::string> headers;
    };
}