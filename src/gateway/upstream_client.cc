#include "tgw/gateway/upstream_client.h"

#include "tgw/common/status.h"

#include <sstream>

namespace tgw {

HttpResponse MockUpstreamClient::Forward(const ForwardContext& ctx) {
    std::ostringstream data;

    data << "{";
    data << "\"mock\":true,";
    data << "\"upstream\":\"" << JsonEscape(ctx.upstream) << "\",";
    data << "\"upstream_path\":\"" << JsonEscape(ctx.upstream_path) << "\",";
    data << "\"method\":\"" << JsonEscape(ctx.method) << "\",";
    data << "\"request_id\":\"" << JsonEscape(ctx.request_id) << "\",";
    data << "\"timeout_ms\":" << ctx.timeout_ms << ",";
    data << "\"header_count\":" << ctx.headers.size();

    if (!ctx.query.empty()) {
        data << ",\"query\":\"" << JsonEscape(ctx.query) << "\"";
    }

    data << "}";

    HttpResponse rsp;
    rsp.status = 200;
    rsp.content_type = "application/json";
    rsp.body = ApiResponse::Success(data.str()).ToJson();
    rsp.headers["X-Request-Id"] = ctx.request_id;
    rsp.headers["X-Upstream"] = ctx.upstream;

    return rsp;
}

} // namespace tgw