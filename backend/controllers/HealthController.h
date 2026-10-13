#pragma once

#include <drogon/HttpController.h>

namespace georescux::backend::controllers {

class HealthController : public drogon::HttpController<HealthController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(HealthController::getHealth, "/health", drogon::Get);
    METHOD_LIST_END

    void getHealth(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );
};

} // namespace georescux::backend::controllers
