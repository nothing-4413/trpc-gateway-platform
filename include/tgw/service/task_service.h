#pragma once

#include "tgw/rpc/rpc_context.h"

#include "task.pb.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace tgw {

class ITaskService {
public:
    virtual ~ITaskService() = default;

    virtual tgw::rpc::CreateTaskResponse CreateTask(
        const RpcContext& ctx,
        const tgw::rpc::CreateTaskRequest& request
    ) = 0;

    virtual tgw::rpc::GetTaskResponse GetTask(
        const RpcContext& ctx,
        const tgw::rpc::GetTaskRequest& request
    ) = 0;

    // one-way RPC 思想：
    // 客户端上报事件，服务端尽力处理，调用方不强依赖返回内容。
    virtual void SubmitTaskEvent(
        const RpcContext& ctx,
        const tgw::rpc::TaskEvent& event
    ) = 0;
};

class TaskServiceImpl : public ITaskService {
public:
    tgw::rpc::CreateTaskResponse CreateTask(
        const RpcContext& ctx,
        const tgw::rpc::CreateTaskRequest& request
    ) override;

    tgw::rpc::GetTaskResponse GetTask(
        const RpcContext& ctx,
        const tgw::rpc::GetTaskRequest& request
    ) override;

    void SubmitTaskEvent(
        const RpcContext& ctx,
        const tgw::rpc::TaskEvent& event
    ) override;

private:
    std::mutex mutex_;
    std::unordered_map<std::string, tgw::rpc::TaskInfo> tasks_;
    uint64_t task_seq_ = 0;
};

using TaskServicePtr = std::shared_ptr<ITaskService>;

} // namespace tgw