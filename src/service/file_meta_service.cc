#include "tgw/service/file_meta_service.h"

#include "tgw/common/logger.h"

#include <chrono>
#include <sstream>

namespace tgw {

namespace {

void FillMeta(
    tgw::rpc::RpcMeta* meta,
    tgw::rpc::RpcCode code,
    const std::string& message,
    const RpcContext& ctx
) {
    meta->set_code(code);
    meta->set_message(message);
    meta->set_request_id(ctx.request_id);
    meta->set_cost_ms(ctx.ElapsedMs());
}

bool FillTimeoutIfCancelled(tgw::rpc::RpcMeta* meta, const RpcContext& ctx) {
    if (!ctx.DeadlineExceeded()) {
        return false;
    }

    ctx.CancelDeadline("deadline exceeded");
    FillMeta(meta, tgw::rpc::RPC_TIMEOUT, "deadline exceeded", ctx);
    return true;
}

int64_t NowUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // namespace

tgw::rpc::CreateFileMetaResponse FileMetaServiceImpl::CreateFileMeta(
    const RpcContext& ctx,
    const tgw::rpc::CreateFileMetaRequest& request
) {
    TGW_INFO(
        "FileMetaService.CreateFileMeta called, request_id={}, filename={}, size={}",
        ctx.request_id,
        request.filename(),
        request.size()
    );

    tgw::rpc::CreateFileMetaResponse response;

    if (FillTimeoutIfCancelled(response.mutable_meta(), ctx)) {
        return response;
    }

    if (request.filename().empty() || request.size() < 0) {
        FillMeta(
            response.mutable_meta(),
            tgw::rpc::RPC_INVALID_ARGUMENT,
            "invalid file meta",
            ctx
        );
        return response;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream oss;
    oss << "file-" << (++file_seq_);

    std::string file_id = oss.str();

    tgw::rpc::FileMeta file;
    file.set_file_id(file_id);
    file.set_filename(request.filename());
    file.set_size(request.size());
    file.set_content_type(request.content_type());
    file.set_owner_user_id(request.owner_user_id());
    file.set_created_at(NowUnixSeconds());

    files_[file_id] = file;

    response.set_file_id(file_id);

    FillMeta(
        response.mutable_meta(),
        tgw::rpc::RPC_OK,
        "ok",
        ctx
    );

    return response;
}

tgw::rpc::GetFileMetaResponse FileMetaServiceImpl::GetFileMeta(
    const RpcContext& ctx,
    const tgw::rpc::GetFileMetaRequest& request
) {
    TGW_INFO(
        "FileMetaService.GetFileMeta called, request_id={}, file_id={}",
        ctx.request_id,
        request.file_id()
    );

    tgw::rpc::GetFileMetaResponse response;

    if (FillTimeoutIfCancelled(response.mutable_meta(), ctx)) {
        return response;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = files_.find(request.file_id());
    if (it == files_.end()) {
        FillMeta(
            response.mutable_meta(),
            tgw::rpc::RPC_NOT_FOUND,
            "file not found",
            ctx
        );
        return response;
    }

    *response.mutable_file() = it->second;

    FillMeta(
        response.mutable_meta(),
        tgw::rpc::RPC_OK,
        "ok",
        ctx
    );

    return response;
}

} // namespace tgw
