#include <iostream>
#include <cassert>
#include <chrono>
#include "../models/SosEventDto.h"
#include "../models/BleMeshPacketDto.h"
#include "../storage/SqliteStorage.h"
#include "../filters/FirebaseAuthFilter.h"

using namespace georescux::backend;

void testSosEventDto() {
    std::cout << "[TEST] Running testSosEventDto..." << std::endl;

    // Valid event
    Json::Value validJson;
    validJson["eventId"] = "evt-12345";
    validJson["userId"] = "user-abc";
    validJson["startedAtMs"] = Json::Int64(1700000000000LL);
    validJson["isActive"] = true;
    validJson["latitude"] = 28.6139;
    validJson["longitude"] = 77.2090;
    validJson["note"] = "Trapped in room";

    std::string err;
    auto dto = models::SosEventDto::fromJson(validJson, err);
    assert(dto.has_value());
    assert(dto->eventId == "evt-12345");
    assert(dto->latitude.has_value() && *dto->latitude == 28.6139);
    assert(dto->note.has_value() && *dto->note == "Trapped in room");

    // Invalid latitude (> 90)
    Json::Value invalidLat = validJson;
    invalidLat["latitude"] = 99.9;
    auto badDto = models::SosEventDto::fromJson(invalidLat, err);
    assert(!badDto.has_value());
    assert(err.find("Invalid latitude") != std::string::npos);

    // Missing eventId
    Json::Value noId = validJson;
    noId.removeMember("eventId");
    auto noIdDto = models::SosEventDto::fromJson(noId, err);
    assert(!noIdDto.has_value());

    std::cout << "[PASS] testSosEventDto passed." << std::endl;
}

void testBleMeshPacketDto() {
    std::cout << "[TEST] Running testBleMeshPacketDto..." << std::endl;

    Json::Value packetJson;
    packetJson["packetId"] = "pkt-999";
    packetJson["originDeviceId"] = "dev-1";
    packetJson["originEmergencyId"] = "evt-12345";
    packetJson["timestampMs"] = Json::Int64(1700000000000LL);
    packetJson["ttl"] = 3;
    packetJson["type"] = "SOS";
    packetJson["latitude"] = 30.1;
    packetJson["longitude"] = 78.2;

    std::string err;
    auto dto = models::BleMeshPacketDto::fromJson(packetJson, err);
    assert(dto.has_value());
    assert(dto->packetId == "pkt-999");
    assert(dto->originDeviceId == "dev-1");
    assert(dto->ttl == 3);

    std::cout << "[PASS] testBleMeshPacketDto passed." << std::endl;
}

void testSqliteStorage() {
    std::cout << "[TEST] Running testSqliteStorage in-memory..." << std::endl;

    storage::SqliteStorage store(":memory:");
    assert(store.init());

    models::SosEventDto ev;
    ev.eventId = "sos-persisted-1";
    ev.userId = "uid-100";
    ev.startedAtMs = 1700000000000LL;
    ev.isActive = true;
    ev.latitude = 30.3;
    ev.longitude = 78.0;

    bool wasDup = false;
    assert(store.saveSosEvent(ev, wasDup));
    assert(!wasDup);

    // Idempotent retry with same eventId
    assert(store.saveSosEvent(ev, wasDup));
    assert(wasDup); // detected as duplicate/update

    // Fetch
    auto fetched = store.getSosEvent("sos-persisted-1");
    assert(fetched.has_value());
    assert(fetched->eventId == "sos-persisted-1");
    assert(fetched->userId == "uid-100");

    // Mesh packet deduplication
    models::BleMeshPacketDto pkt;
    pkt.packetId = "mesh-pkt-1";
    pkt.originDeviceId = "phone-A";
    pkt.type = "TEST";

    bool pktDup = false;
    assert(store.saveMeshPacket(pkt, pktDup));
    assert(!pktDup);

    assert(store.saveMeshPacket(pkt, pktDup));
    assert(pktDup); // duplicate detected

    std::cout << "[PASS] testSqliteStorage passed." << std::endl;
}

void testFirebaseAuthVerification() {
    std::cout << "[TEST] Running testFirebaseAuthVerification..." << std::endl;

    std::string err;
    // 1. Malformed token (not 3 dots)
    auto r1 = filters::FirebaseAuthFilter::verifyFirebaseToken("invalid.token", "georescex", err);
    assert(!r1.has_value());
    assert(err.find("Malformed JWT") != std::string::npos);

    // 2. Unsigned token (empty signature segment)
    auto r2 = filters::FirebaseAuthFilter::verifyFirebaseToken("header.payload.", "georescex", err);
    assert(!r2.has_value());
    assert(err.find("Rejecting unverified") != std::string::npos);

    std::cout << "[PASS] testFirebaseAuthVerification passed." << std::endl;
}

int main() {
    std::cout << "Running GeoRescuX Backend Automated Tests..." << std::endl;
    testSosEventDto();
    testBleMeshPacketDto();
    testSqliteStorage();
    testFirebaseAuthVerification();
    std::cout << "All backend tests completed successfully!" << std::endl;
    return 0;
}
