#pragma once

#include <drogon/HttpController.h>
#include "../storage/SqliteStorage.h"
#include "../spatial/DbscanClusterer.h"
#include "../dispatch/RescueResourceMatcher.h"
#include <memory>

namespace georescux::backend::controllers {

class ClusterController : public drogon::HttpController<ClusterController> {
public:
    explicit ClusterController(std::shared_ptr<storage::SqliteStorage> storage = nullptr);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ClusterController::getClusters, "/api/v1/clusters", drogon::Get);
    ADD_METHOD_TO(ClusterController::matchDispatch, "/api/v1/dispatch/match", drogon::Post);
    METHOD_LIST_END

    void getClusters(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void matchDispatch(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

private:
    std::shared_ptr<storage::SqliteStorage> storage_;
};

} // namespace georescux::backend::controllers
