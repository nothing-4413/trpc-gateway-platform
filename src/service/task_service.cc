#include "tgw/service/task_service.h"

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

int64_t NowUnixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

} // namespace

tgw::rpc::CreateTaskResponse TaskServiceImpl::CreateTask(
    const RpcContext& ctx,
    const tgw::rpc::CreateTaskRequest& request
) {
    TGW_INFO(
        "TaskService.CreateTask called, request_id={}, task_type={}, user_id={}",
        ctx.request_id,
        request.task_type(),
        request.user_id()
    );

    tgw::rpc::CreateTaskResponse response;

    if (request.task_type().empty()) {
        FillMeta(
            response.mutable_meta(),
            tgw::rpc::RPC_INVALID_ARGUMENT,
            "task_type is empty",
            ctx
        );
        return response;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream oss;
    oss << "task-" << (++task_seq_);

    std::string task_id = oss.str();
    int64_t now = NowUnixSeconds();

    tgw::rpc::TaskInfo task;
    task.set_task_id(task_id);
    task.set_task_type(request.task_type());
    task.set_status(tgw::rpc::TASK_STATUS_PENDING);
    task.set_result("");
    task.set_user_id(request.user_id());
    task.set_created_at(now);
    task.set_updated_at(now);

    tasks_[task_id] = task;

    response.set_task_id(task_id);

    FillMeta(
        response.mutable_meta(),
        tgw::rpc::RPC_OK,
        "ok",
        ctx
    );

    return response;
}

tgw::rpc::GetTaskResponse TaskServiceImpl::GetTask(
    const RpcContext& ctx,
    const tgw::rpc::GetTaskRequest& request
) {
    TGW_INFO(
        "TaskService.GetTask called, request_id={}, task_id={}",
        ctx.request_id,
        request.task_id()
    );

    tgw::rpc::GetTaskResponse response;

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tasks_.find(request.task_id());
    if (it == tasks_.end()) {
        FillMeta(
            response.mutable_meta(),
            tgw::rpc::RPC_NOT_FOUND,
            "task not found",
            ctx
        );
        return response;
    }

    *response.mutable_task() = it->second;

    FillMeta(
        response.mutable_meta(),
        tgw::rpc::RPC_OK,
        "ok",
        ctx
    );

    return response;
}

void TaskServiceImpl::SubmitTaskEvent(
    const RpcContext& ctx,
    const tgw::rpc::TaskEvent& event
) {
    TGW_INFO(
        "TaskService.SubmitTaskEvent called, request_id={}, task_id={}, status={}, message={}",
        ctx.request_id,
        event.task_id(),
        static_cast<int>(event.status()),
        event.message()
    );

    std::lock_guard<std::mutex> lock(mutex_);

    auto it = tasks_.find(event.task_id());
    if (it == tasks_.end()) {
        TGW_WARN(
            "SubmitTaskEvent ignored, task not found, request_id={}, task_id={}",
            ctx.request_id,
            event.task_id()
        );
        return;
    }

    it->second.set_status(event.status());
    it->second.set_result(event.message());
    it->second.set_updated_at(event.timestamp());
}

} // namespace tgw