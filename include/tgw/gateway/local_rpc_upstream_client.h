#pragma once

#include "tgw/auth/token_service.h"
#include "tgw/gateway/upstream_client.h"
#include "tgw/service/file_meta_service.h"
#include "tgw/service/task_service.h"
#include "tgw/service/user_service.h"

namespace tgw {

class LocalRpcUpstreamClient : public IUpstreamClient {
public:
    LocalRpcUpstreamClient(
        UserServicePtr user_service,
        FileMetaServicePtr file_meta_service,
        TaskServicePtr task_service,
        TokenServicePtr token_service
    );

    HttpResponse Forward(const ForwardContext& ctx) override;

private:
    HttpResponse ForwardUserService(const ForwardContext& ctx);
    HttpResponse ForwardFileMetaService(const ForwardContext& ctx);
    HttpResponse ForwardTaskService(const ForwardContext& ctx);

private:
    UserServicePtr user_service_;
    FileMetaServicePtr file_meta_service_;
    TaskServicePtr task_service_;
    TokenServicePtr token_service_;
};

} // namespace tgw
