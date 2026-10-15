#ifndef GEORESCUX_BACKEND_RESCUE_COMMAND_BROKER_H
#define GEORESCUX_BACKEND_RESCUE_COMMAND_BROKER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <cstdint>
#include <memory>
#include <queue>

namespace georescux::backend::realtime {

enum class WsOpcode : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

struct CommandMessage {
    std::string topic;
    std::string payload;
    uint64_t timestampMs;
    uint32_t senderId;
    int priority; // 0=Telemetry, 1=Info, 2=Warning, 3=CRITICAL_SOS
};

class RescueCommandBroker {
public:
    using MessageCallback = std::function<void(const CommandMessage&)>;

    struct ClientSession {
        uint32_t clientId;
        std::string role; // "RESCUER", "COMMAND_CENTER", "DRONE", "CIVILIAN"
        std::vector<std::string> subscribedTopics;
        MessageCallback callback;
        uint64_t connectedAtMs;
        uint64_t lastHeartbeatMs;
        size_t queuedPacketsCount;
        bool isThrottled;
    };

    RescueCommandBroker(size_t maxQueueCapacity = 10000);
    ~RescueCommandBroker() = default;

    /**
     * Registers a new subscriber session.
     */
    uint32_t registerClient(const std::string& role, MessageCallback callback);

    /**
     * Unregisters a client session.
     */
    bool unregisterClient(uint32_t clientId);

    /**
     * Subscribes a client to a topic or wildcard pattern (e.g. "incident/+/triage").
     */
    bool subscribe(uint32_t clientId, const std::string& topicPattern);

    /**
     * Unsubscribes a client from a topic.
     */
    bool unsubscribe(uint32_t clientId, const std::string& topicPattern);

    /**
     * Publishes a message into the broker. Dispatches immediately to matching clients.
     */
    size_t publish(
        const std::string& topic,
        const std::string& payload,
        uint32_t senderId = 0,
        int priority = 1
    );

    /**
     * Flushes queued messages up to maxCount.
     */
    size_t processQueue(size_t maxCount = 100);

    /**
     * Encodes a payload into an RFC-6455 compliant WebSocket frame.
     */
    static std::vector<uint8_t> encodeWebSocketFrame(
        const std::string& payload,
        WsOpcode opcode = WsOpcode::TEXT,
        bool mask = false
    );

    /**
     * Decodes an RFC-6455 frame. Returns false if frame is incomplete or malformed.
     */
    static bool decodeWebSocketFrame(
        const uint8_t* data,
        size_t length,
        std::string& outPayload,
        WsOpcode& outOpcode,
        size_t& outBytesConsumed
    );

    /**
     * Checks if a topic matches a subscription pattern with '+' and '#' wildcards.
     */
    static bool topicMatches(const std::string& pattern, const std::string& topic);

    [[nodiscard]] size_t getConnectedClientCount() const;
    [[nodiscard]] size_t getPendingQueueSize() const;

private:
    size_t maxQueueCapacity_;
    uint32_t nextClientId_ = 1001;
    mutable std::mutex brokerMutex_;

    std::unordered_map<uint32_t, ClientSession> clients_;
    std::queue<CommandMessage> messageQueue_;
};

} // namespace georescux::backend::realtime

#endif // GEORESCUX_BACKEND_RESCUE_COMMAND_BROKER_H
