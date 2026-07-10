#include "tgw/common/config.h"
#include "tgw/common/logger.h"
#include "tgw/rpc/rpc_context.h"
#include "tgw/service/file_meta_service.h"
#include "tgw/service/task_service.h"
#include "tgw/service/user_service.h"

#include <iostream>

int main() {
    tgw::LogConfig log_config;
    log_config.level = "info";
    log_config.path = "logs/rpc_demo.log";

    tgw::Logger::Init(log_config);

    tgw::RpcContext ctx;
    ctx.request_id = "rpc-demo-001";
    ctx.trace_id = "trace-demo-001";
    ctx.caller = "rpc-demo";
    ctx.timeout_ms = 1000;

    tgw::UserServiceImpl user_service;
    tgw::FileMetaServiceImpl file_service;
    tgw::TaskServiceImpl task_service;

    // 1. Login
    tgw::rpc::LoginRequest login_req;
    login_req.set_username("admin");
    login_req.set_password("123456");

    auto login_rsp = user_service.Login(ctx, login_req);

    std::cout << "Login code: " << login_rsp.meta().code()
              << ", message: " << login_rsp.meta().message()
              << ", token: " << login_rsp.token()
              << std::endl;

    // 2. GetProfile
    tgw::rpc::GetProfileRequest profile_req;
    profile_req.set_user_id(login_rsp.user_id());

    auto profile_rsp = user_service.GetProfile(ctx, profile_req);

    std::cout << "Profile username: " << profile_rsp.profile().username()
              << ", email: " << profile_rsp.profile().email()
              << std::endl;

    // 3. CreateFileMeta
    tgw::rpc::CreateFileMetaRequest create_file_req;
    create_file_req.set_filename("demo.txt");
    create_file_req.set_size(1024);
    create_file_req.set_content_type("text/plain");
    create_file_req.set_owner_user_id(login_rsp.user_id());

    auto create_file_rsp = file_service.CreateFileMeta(ctx, create_file_req);

    std::cout << "CreateFileMeta file_id: " << create_file_rsp.file_id()
              << std::endl;

    // 4. GetFileMeta
    tgw::rpc::GetFileMetaRequest get_file_req;
    get_file_req.set_file_id(create_file_rsp.file_id());

    auto get_file_rsp = file_service.GetFileMeta(ctx, get_file_req);

    std::cout << "GetFileMeta filename: " << get_file_rsp.file().filename()
              << ", size: " << get_file_rsp.file().size()
              << std::endl;

    // 5. CreateTask
    tgw::rpc::CreateTaskRequest create_task_req;
    create_task_req.set_task_type("compress_file");
    create_task_req.set_payload("{\"file_id\":\"" + create_file_rsp.file_id() + "\"}");
    create_task_req.set_user_id(login_rsp.user_id());

    auto create_task_rsp = task_service.CreateTask(ctx, create_task_req);

    std::cout << "CreateTask task_id: " << create_task_rsp.task_id()
              << std::endl;

    // 6. SubmitTaskEvent，模拟 one-way 上报
    tgw::rpc::TaskEvent event;
    event.set_task_id(create_task_rsp.task_id());
    event.set_status(tgw::rpc::TASK_STATUS_SUCCESS);
    event.set_message("task finished");
    event.set_timestamp(1234567890);

    task_service.SubmitTaskEvent(ctx, event);

    // 7. GetTask
    tgw::rpc::GetTaskRequest get_task_req;
    get_task_req.set_task_id(create_task_rsp.task_id());

    auto get_task_rsp = task_service.GetTask(ctx, get_task_req);

    std::cout << "GetTask status: " << get_task_rsp.task().status()
              << ", result: " << get_task_rsp.task().result()
              << std::endl;

    return 0;
}