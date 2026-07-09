#pragma once

#include<sstream>
#include<string>

namespace tgw
{
enum class ErrorCode
{
    OK = 0,

    BAD_REQUEST = 40000,
    UNAUTHORIZED = 40100,
    FORBIDDEN = 40300,
    NOT_FOUND = 40400,

    RATE_LIMITED = 42900,

    RPC_TIMEOUT = 50400,
    RPC_FAILED = 50500,

    INTERNAL_ERROR = 50000
};

inline std::string JsonEscape(const std::string& input) {
    std::ostringstream oss;

    for (char c : input) {
        switch (c) {
            case '"':
                oss << "\\\"";
                break;
            case '\\':
                oss << "\\\\";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                oss << c;
                break;
        }
    }

    return oss.str();
}

struct ApiResponse
{
    int code = 0;
    std::string message = "ok";
    std::string data_raw_json = "null";

    static ApiResponse Success(const std::string& raw_json = "null")
    {
        ApiResponse rsp;
        rsp.code = static_cast<int>(ErrorCode::OK);
        rsp.message = "ok";
        rsp.data_raw_json = raw_json.empty() ? "null" :raw_json;
        return rsp;
    }

    static ApiResponse Error(ErrorCode code, const std::string& message) {
        ApiResponse rsp;
        rsp.code = static_cast<int>(code);
        rsp.message = message;
        rsp.data_raw_json = "null";
        return rsp;
    }

    std::string ToJson() const {
        std::ostringstream oss;

        oss << "{";
        oss << "\"code\":" << code << ",";
        oss << "\"message\":\"" << JsonEscape(message) << "\",";
        oss << "\"data\":" << data_raw_json;
        oss << "}";

        return oss.str();
    }
};
}