#include "tgw/gateway/local_rpc_upstream_client.h"

#include "tgw/common/logger.h"
#include "tgw/common/status.h"
#include "tgw/rpc/rpc_context.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace tgw {

namespace {

using FieldMap = std::map<std::string, std::string>;

bool IsHex(char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

int HexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return 0;
}

std::string UrlDecode(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == '+') {
            output.push_back(' ');
            continue;
        }
        if (c == '%' && i + 2 < input.size() && IsHex(input[i + 1]) && IsHex(input[i + 2])) {
            output.push_back(static_cast<char>(HexValue(input[i + 1]) * 16 + HexValue(input[i + 2])));
            i += 2;
            continue;
        }
        output.push_back(c);
    }

    return output;
}

FieldMap ParseQuery(const std::string& query) {
    FieldMap result;
    size_t start = 0;

    while (start <= query.size()) {
        size_t end = query.find('&', start);
        if (end == std::string::npos) {
            end = query.size();
        }

        std::string part = query.substr(start, end - start);
        if (!part.empty()) {
            size_t eq = part.find('=');
            std::string key = UrlDecode(part.substr(0, eq));
            std::string value = eq == std::string::npos ? "" : UrlDecode(part.substr(eq + 1));
            result[key] = value;
        }

        if (end == query.size()) {
            break;
        }
        start = end + 1;
    }

    return result;
}

void SkipWhitespace(const std::string& text, size_t* pos) {
    while (*pos < text.size() && std::isspace(static_cast<unsigned char>(text[*pos]))) {
        ++(*pos);
    }
}

std::string ParseJsonStringToken(const std::string& text, size_t* pos) {
    if (*pos >= text.size() || text[*pos] != '"') {
        throw std::runtime_error("expected json string");
    }

    ++(*pos);
    std::string result;

    while (*pos < text.size()) {
        char c = text[(*pos)++];
        if (c == '"') {
            return result;
        }
        if (c != '\\') {
            result.push_back(c);
            continue;
        }

        if (*pos >= text.size()) {
            throw std::runtime_error("invalid json escape");
        }

        char escaped = text[(*pos)++];
        switch (escaped) {
            case '"':
            case '\\':
            case '/':
                result.push_back(escaped);
                break;
            case 'b':
                result.push_back('\b');
                break;
            case 'f':
                result.push_back('\f');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            default:
                result.push_back(escaped);
                break;
        }
    }

    throw std::runtime_error("unterminated json string");
}

std::string ParseJsonScalarToken(const std::string& text, size_t* pos) {
    size_t start = *pos;
    while (*pos < text.size()) {
        char c = text[*pos];
        if (c == ',' || c == '}' || std::isspace(static_cast<unsigned char>(c))) {
            break;
        }
        ++(*pos);
    }
    return text.substr(start, *pos - start);
}

FieldMap ParseFlatJsonObject(const std::string& body) {
    FieldMap result;
    size_t pos = 0;

    SkipWhitespace(body, &pos);
    if (pos >= body.size()) {
        return result;
    }
    if (body[pos] != '{') {
        throw std::runtime_error("json body must be an object");
    }
    ++pos;

    while (true) {
        SkipWhitespace(body, &pos);
        if (pos < body.size() && body[pos] == '}') {
            return result;
        }

        std::string key = ParseJsonStringToken(body, &pos);
        SkipWhitespace(body, &pos);
        if (pos >= body.size() || body[pos] != ':') {
            throw std::runtime_error("expected ':' after json key");
        }
        ++pos;

        SkipWhitespace(body, &pos);
        std::string value;
        if (pos < body.size() && body[pos] == '"') {
            value = ParseJsonStringToken(body, &pos);
        } else {
            value = ParseJsonScalarToken(body, &pos);
        }
        result[key] = value;

        SkipWhitespace(body, &pos);
        if (pos < body.size() && body[pos] == ',') {
            ++pos;
            continue;
        }
        if (pos < body.size() && body[pos] == '}') {
            return result;
        }
        throw std::runtime_error("expected ',' or '}' in json object");
    }
}

std::string GetString(const FieldMap& fields, const std::string& key, const std::string& fallback = "") {
    auto it = fields.find(key);
    return it == fields.end() ? fallback : it->second;
}

int64_t GetInt64(const FieldMap& fields, const std::string& key, int64_t fallback = 0) {
    auto it = fields.find(key);
    if (it == fields.end() || it->second.empty() || it->second == "null") {
        return fallback;
    }
    return std::stoll(it->second);
}

RpcContext BuildRpcContext(const ForwardContext& ctx) {
    RpcContext rpc_ctx;
    rpc_ctx.request_id = ctx.request_id;
    rpc_ctx.caller = "tgw-gateway";
    rpc_ctx.timeout_ms = ctx.timeout_ms;
    rpc_ctx.deadline = ctx.deadline;
    rpc_ctx.headers = ctx.headers;

    auto trace = ctx.headers.find("X-Trace-Id");
    if (trace != ctx.headers.end()) {
        rpc_ctx.trace_id = trace->second;
    }

    auto user = ctx.headers.find("X-User-Id");
    if (user != ctx.headers.end()) {
        rpc_ctx.user_id = user->second;
    }

    return rpc_ctx;
}

ErrorCode MapRpcCode(tgw::rpc::RpcCode code) {
    switch (code) {
        case tgw::rpc::RPC_OK:
            return ErrorCode::OK;
        case tgw::rpc::RPC_INVALID_ARGUMENT:
            return ErrorCode::BAD_REQUEST;
        case tgw::rpc::RPC_UNAUTHORIZED:
            return ErrorCode::UNAUTHORIZED;
        case tgw::rpc::RPC_NOT_FOUND:
            return ErrorCode::NOT_FOUND;
        case tgw::rpc::RPC_TIMEOUT:
            return ErrorCode::RPC_TIMEOUT;
        case tgw::rpc::RPC_INTERNAL_ERROR:
        default:
            return ErrorCode::RPC_FAILED;
    }
}

int MapHttpStatus(tgw::rpc::RpcCode code) {
    switch (code) {
        case tgw::rpc::RPC_OK:
            return 200;
        case tgw::rpc::RPC_INVALID_ARGUMENT:
            return 400;
        case tgw::rpc::RPC_UNAUTHORIZED:
            return 401;
        case tgw::rpc::RPC_NOT_FOUND:
            return 404;
        case tgw::rpc::RPC_TIMEOUT:
            return 504;
        case tgw::rpc::RPC_INTERNAL_ERROR:
        default:
            return 502;
    }
}

HttpResponse BuildJsonResponse(int status, const ApiResponse& api_response, const std::string& request_id) {
    HttpResponse rsp;
    rsp.status = status;
    rsp.content_type = "application/json";
    rsp.body = api_response.ToJson();
    rsp.headers["X-Request-Id"] = request_id;
    return rsp;
}

HttpResponse BuildRpcResponse(
    const tgw::rpc::RpcMeta& meta,
    const std::string& data_json,
    const std::string& request_id
) {
    if (meta.code() == tgw::rpc::RPC_OK) {
        return BuildJsonResponse(
            200,
            ApiResponse::Success(data_json),
            request_id
        );
    }

    ApiResponse api_response;
    api_response.code = static_cast<int>(MapRpcCode(meta.code()));
    api_response.message = meta.message();
    api_response.data_raw_json = "null";

    return BuildJsonResponse(MapHttpStatus(meta.code()), api_response, request_id);
}

HttpResponse NotFound(const std::string& message, const std::string& request_id) {
    return BuildJsonResponse(404, ApiResponse::Error(ErrorCode::NOT_FOUND, message), request_id);
}

HttpResponse BadRequest(const std::string& message, const std::string& request_id) {
    return BuildJsonResponse(400, ApiResponse::Error(ErrorCode::BAD_REQUEST, message), request_id);
}

HttpResponse InternalError(const std::string& message, const std::string& request_id) {
    return BuildJsonResponse(500, ApiResponse::Error(ErrorCode::INTERNAL_ERROR, message), request_id);
}

HttpResponse DeadlineExceeded(const ForwardContext& ctx) {
    return BuildJsonResponse(
        504,
        ApiResponse::Error(ErrorCode::RPC_TIMEOUT, "deadline exceeded"),
        ctx.request_id
    );
}

bool IsDeadlineExceeded(const ForwardContext& ctx) {
    if (!ctx.DeadlineExceeded()) {
        return false;
    }

    ctx.CancelDeadline("deadline exceeded");
    return true;
}

std::string JsonPair(const std::string& key, const std::string& value, bool string_value) {
    std::ostringstream oss;
    oss << "\"" << JsonEscape(key) << "\":";
    if (string_value) {
        oss << "\"" << JsonEscape(value) << "\"";
    } else {
        oss << value;
    }
    return oss.str();
}

} // namespace

LocalRpcUpstreamClient::LocalRpcUpstreamClient(
    UserServicePtr user_service,
    FileMetaServicePtr file_meta_service,
    TaskServicePtr task_service,
    TokenServicePtr token_service
)
    : user_service_(std::move(user_service)),
      file_meta_service_(std::move(file_meta_service)),
      task_service_(std::move(task_service)),
      token_service_(std::move(token_service)) {}

HttpResponse LocalRpcUpstreamClient::Forward(const ForwardContext& ctx) {
    try {
        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        TGW_INFO(
            "local rpc forward, request_id={}, upstream={}, path={}",
            ctx.request_id,
            ctx.upstream,
            ctx.upstream_path
        );

        if (ctx.upstream == "UserService") {
            return ForwardUserService(ctx);
        }
        if (ctx.upstream == "FileMetaService") {
            return ForwardFileMetaService(ctx);
        }
        if (ctx.upstream == "TaskService") {
            return ForwardTaskService(ctx);
        }

        return NotFound("unknown upstream service: " + ctx.upstream, ctx.request_id);
    } catch (const std::invalid_argument& e) {
        return BadRequest(e.what(), ctx.request_id);
    } catch (const std::exception& e) {
        TGW_ERROR("local rpc forward exception, request_id={}, error={}", ctx.request_id, e.what());
        return InternalError(e.what(), ctx.request_id);
    }
}

HttpResponse LocalRpcUpstreamClient::ForwardUserService(const ForwardContext& ctx) {
    RpcContext rpc_ctx = BuildRpcContext(ctx);

    if (ctx.method == "POST" && ctx.upstream_path == "/login") {
        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto body = ParseFlatJsonObject(ctx.body);

        tgw::rpc::LoginRequest req;
        req.set_username(GetString(body, "username"));
        req.set_password(GetString(body, "password"));

        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto rsp = user_service_->Login(rpc_ctx, req);
        std::string token = rsp.token();
        if (rsp.meta().code() == tgw::rpc::RPC_OK && token_service_) {
            token = token_service_->IssueToken(
                rsp.user_id(),
                rsp.username(),
                rsp.role(),
                ctx.request_id
            );
        }

        std::ostringstream data;
        data << "{"
             << JsonPair("user_id", std::to_string(rsp.user_id()), false) << ","
             << JsonPair("username", rsp.username(), true) << ","
             << JsonPair("role", rsp.role(), true) << ","
             << JsonPair("token", token, true) << ","
             << JsonPair("token_type", "Bearer", true)
             << "}";

        return BuildRpcResponse(rsp.meta(), data.str(), ctx.request_id);
    }

    if (ctx.method == "GET" && ctx.upstream_path == "/profile") {
        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto query = ParseQuery(ctx.query);

        tgw::rpc::GetProfileRequest req;
        int64_t user_id = GetInt64(query, "user_id");
        if (user_id == 0 && !rpc_ctx.user_id.empty()) {
            user_id = std::stoll(rpc_ctx.user_id);
        }
        req.set_user_id(user_id);

        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto rsp = user_service_->GetProfile(rpc_ctx, req);

        std::ostringstream data;
        data << "{"
             << JsonPair("user_id", std::to_string(rsp.profile().user_id()), false) << ","
             << JsonPair("username", rsp.profile().username(), true) << ","
             << JsonPair("email", rsp.profile().email(), true) << ","
             << JsonPair("role", rsp.profile().role(), true)
             << "}";

        return BuildRpcResponse(rsp.meta(), data.str(), ctx.request_id);
    }

    return NotFound("UserService method not found: " + ctx.upstream_path, ctx.request_id);
}

HttpResponse LocalRpcUpstreamClient::ForwardFileMetaService(const ForwardContext& ctx) {
    RpcContext rpc_ctx = BuildRpcContext(ctx);

    if (ctx.method == "POST" && ctx.upstream_path == "/meta/create") {
        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto body = ParseFlatJsonObject(ctx.body);

        tgw::rpc::CreateFileMetaRequest req;
        req.set_filename(GetString(body, "filename"));
        req.set_size(GetInt64(body, "size"));
        req.set_content_type(GetString(body, "content_type"));
        int64_t owner_user_id = GetInt64(body, "owner_user_id");
        if (owner_user_id == 0 && !rpc_ctx.user_id.empty()) {
            owner_user_id = std::stoll(rpc_ctx.user_id);
        }
        req.set_owner_user_id(owner_user_id);

        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto rsp = file_meta_service_->CreateFileMeta(rpc_ctx, req);

        std::ostringstream data;
        data << "{" << JsonPair("file_id", rsp.file_id(), true) << "}";

        return BuildRpcResponse(rsp.meta(), data.str(), ctx.request_id);
    }

    if (ctx.method == "GET" && ctx.upstream_path == "/meta") {
        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto query = ParseQuery(ctx.query);

        tgw::rpc::GetFileMetaRequest req;
        req.set_file_id(GetString(query, "file_id"));

        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto rsp = file_meta_service_->GetFileMeta(rpc_ctx, req);

        std::ostringstream data;
        data << "{"
             << JsonPair("file_id", rsp.file().file_id(), true) << ","
             << JsonPair("filename", rsp.file().filename(), true) << ","
             << JsonPair("size", std::to_string(rsp.file().size()), false) << ","
             << JsonPair("content_type", rsp.file().content_type(), true) << ","
             << JsonPair("owner_user_id", std::to_string(rsp.file().owner_user_id()), false) << ","
             << JsonPair("created_at", std::to_string(rsp.file().created_at()), false)
             << "}";

        return BuildRpcResponse(rsp.meta(), data.str(), ctx.request_id);
    }

    return NotFound("FileMetaService method not found: " + ctx.upstream_path, ctx.request_id);
}

HttpResponse LocalRpcUpstreamClient::ForwardTaskService(const ForwardContext& ctx) {
    RpcContext rpc_ctx = BuildRpcContext(ctx);

    if (ctx.method == "POST" && ctx.upstream_path == "/create") {
        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto body = ParseFlatJsonObject(ctx.body);

        tgw::rpc::CreateTaskRequest req;
        req.set_task_type(GetString(body, "task_type"));
        req.set_payload(GetString(body, "payload"));
        int64_t user_id = GetInt64(body, "user_id");
        if (user_id == 0 && !rpc_ctx.user_id.empty()) {
            user_id = std::stoll(rpc_ctx.user_id);
        }
        req.set_user_id(user_id);

        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto rsp = task_service_->CreateTask(rpc_ctx, req);

        std::ostringstream data;
        data << "{" << JsonPair("task_id", rsp.task_id(), true) << "}";

        return BuildRpcResponse(rsp.meta(), data.str(), ctx.request_id);
    }

    if (ctx.method == "GET" && ctx.upstream_path == "/get") {
        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto query = ParseQuery(ctx.query);

        tgw::rpc::GetTaskRequest req;
        req.set_task_id(GetString(query, "task_id"));

        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto rsp = task_service_->GetTask(rpc_ctx, req);

        std::ostringstream data;
        data << "{"
             << JsonPair("task_id", rsp.task().task_id(), true) << ","
             << JsonPair("task_type", rsp.task().task_type(), true) << ","
             << JsonPair("status", std::to_string(static_cast<int>(rsp.task().status())), false) << ","
             << JsonPair("result", rsp.task().result(), true) << ","
             << JsonPair("user_id", std::to_string(rsp.task().user_id()), false) << ","
             << JsonPair("created_at", std::to_string(rsp.task().created_at()), false) << ","
             << JsonPair("updated_at", std::to_string(rsp.task().updated_at()), false)
             << "}";

        return BuildRpcResponse(rsp.meta(), data.str(), ctx.request_id);
    }

    if (ctx.method == "POST" && ctx.upstream_path == "/event") {
        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        auto body = ParseFlatJsonObject(ctx.body);

        tgw::rpc::TaskEvent event;
        event.set_task_id(GetString(body, "task_id"));
        event.set_status(static_cast<tgw::rpc::TaskStatus>(GetInt64(body, "status")));
        event.set_message(GetString(body, "message"));
        event.set_timestamp(GetInt64(body, "timestamp"));

        if (IsDeadlineExceeded(ctx)) {
            return DeadlineExceeded(ctx);
        }

        task_service_->SubmitTaskEvent(rpc_ctx, event);

        std::ostringstream data;
        data << "{"
             << JsonPair("accepted", "true", false) << ","
             << JsonPair("task_id", event.task_id(), true)
             << "}";

        return BuildJsonResponse(200, ApiResponse::Success(data.str()), ctx.request_id);
    }

    return NotFound("TaskService method not found: " + ctx.upstream_path, ctx.request_id);
}

} // namespace tgw
