#pragma once

#include <drogon/HttpController.h>
#include <memory>
#include "../storage/SqliteStorage.h"

namespace georescux::backend::controllers {

class BleMeshController : public drogon::HttpController<BleMeshController> {
public:
    explicit BleMeshController(std::shared_ptr<storage::SqliteStorage> storage = nullptr);

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(BleMeshController::ingestPacket, "/api/v1/mesh/packets", drogon::Post, "georescux::backend::filters::FirebaseAuthFilter");
    ADD_METHOD_TO(BleMeshController::getPacket, "/api/v1/mesh/packets/{1}", drogon::Get, "georescux::backend::filters::FirebaseAuthFilter");
    METHOD_LIST_END

    void ingestPacket(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void getPacket(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        std::string packetId
    );

    void setStorage(std::shared_ptr<storage::SqliteStorage> storage) {
        storage_ = std::move(storage);
    }

private:
    std::shared_ptr<storage::SqliteStorage> storage_;
};

} // namespace georescux::backend::controllers
