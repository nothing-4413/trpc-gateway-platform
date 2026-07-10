#pragma once

#include "tgw/rpc/rpc_context.h"

#include "file_meta.pb.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace tgw {

class IFileMetaService {
public:
    virtual ~IFileMetaService() = default;

    virtual tgw::rpc::CreateFileMetaResponse CreateFileMeta(
        const RpcContext& ctx,
        const tgw::rpc::CreateFileMetaRequest& request
    ) = 0;

    virtual tgw::rpc::GetFileMetaResponse GetFileMeta(
        const RpcContext& ctx,
        const tgw::rpc::GetFileMetaRequest& request
    ) = 0;
};

class FileMetaServiceImpl : public IFileMetaService {
public:
    tgw::rpc::CreateFileMetaResponse CreateFileMeta(
        const RpcContext& ctx,
        const tgw::rpc::CreateFileMetaRequest& request
    ) override;

    tgw::rpc::GetFileMetaResponse GetFileMeta(
        const RpcContext& ctx,
        const tgw::rpc::GetFileMetaRequest& request
    ) override;

private:
    std::mutex mutex_;
    std::unordered_map<std::string, tgw::rpc::FileMeta> files_;
    uint64_t file_seq_ = 0;
};

using FileMetaServicePtr = std::shared_ptr<IFileMetaService>;

} // namespace tgw