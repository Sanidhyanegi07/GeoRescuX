#pragma once

#include <string>
#include <vector>
#include <optional>
#include <mutex>
#include <sqlite3.h>
#include "../models/SosEventDto.h"
#include "../models/BleMeshPacketDto.h"

namespace georescux::backend::storage {

class SqliteStorage {
public:
    explicit SqliteStorage(const std::string& dbPath = "georescux_backend.db");
    ~SqliteStorage();

    bool init();

    // SOS events
    bool saveSosEvent(const models::SosEventDto& event, bool& wasDuplicate);
    std::optional<models::SosEventDto> getSosEvent(const std::string& eventId);
    std::vector<models::SosEventDto> getAllSosEvents(int limit = 100);

    // BLE mesh packets
    bool saveMeshPacket(const models::BleMeshPacketDto& packet, bool& wasDuplicate);
    std::optional<models::BleMeshPacketDto> getMeshPacket(const std::string& packetId);
    std::vector<models::BleMeshPacketDto> getAllMeshPackets(int limit = 100);

private:
    std::string dbPath_;
    sqlite3* db_{nullptr};
    std::mutex mutex_;

    bool createTables();
};

} // namespace georescux::backend::storage
