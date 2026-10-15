#include "RescueCommandBroker.h"
#include <chrono>
#include <sstream>
#include <random>

namespace georescux::backend::realtime {

namespace {
uint64_t getCurrentTimeMillis() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

std::vector<std::string> splitTopic(const std::string& str) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, '/')) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}
} // namespace

RescueCommandBroker::RescueCommandBroker(size_t maxQueueCapacity)
    : maxQueueCapacity_(maxQueueCapacity) {}

uint32_t RescueCommandBroker::registerClient(const std::string& role, MessageCallback callback) {
    std::lock_guard<std::mutex> lock(brokerMutex_);
    uint32_t id = nextClientId_++;
    uint64_t now = getCurrentTimeMillis();

    ClientSession session;
    session.clientId = id;
    session.role = role;
    session.callback = std::move(callback);
    session.connectedAtMs = now;
    session.lastHeartbeatMs = now;
    session.queuedPacketsCount = 0;
    session.isThrottled = false;

    clients_[id] = std::move(session);
    return id;
}

bool RescueCommandBroker::unregisterClient(uint32_t clientId) {
    std::lock_guard<std::mutex> lock(brokerMutex_);
    return clients_.erase(clientId) > 0;
}

bool RescueCommandBroker::subscribe(uint32_t clientId, const std::string& topicPattern) {
    std::lock_guard<std::mutex> lock(brokerMutex_);
    auto it = clients_.find(clientId);
    if (it == clients_.end()) return false;

    for (const auto& pat : it->second.subscribedTopics) {
        if (pat == topicPattern) return true; // already subscribed
    }
    it->second.subscribedTopics.push_back(topicPattern);
    return true;
}

bool RescueCommandBroker::unsubscribe(uint32_t clientId, const std::string& topicPattern) {
    std::lock_guard<std::mutex> lock(brokerMutex_);
    auto it = clients_.find(clientId);
    if (it == clients_.end()) return false;

    auto& vec = it->second.subscribedTopics;
    auto remIt = std::remove(vec.begin(), vec.end(), topicPattern);
    if (remIt != vec.end()) {
        vec.erase(remIt, vec.end());
        return true;
    }
    return false;
}

bool RescueCommandBroker::topicMatches(const std::string& pattern, const std::string& topic) {
    if (pattern == "#" || pattern == topic) return true;

    auto patTokens = splitTopic(pattern);
    auto topTokens = splitTopic(topic);

    size_t i = 0;
    for (; i < patTokens.size(); ++i) {
        if (patTokens[i] == "#") {
            return true; // matches everything remainder
        }
        if (i >= topTokens.size()) {
            return false;
        }
        if (patTokens[i] != "+" && patTokens[i] != topTokens[i]) {
            return false;
        }
    }
    return i == topTokens.size();
}

size_t RescueCommandBroker::publish(
    const std::string& topic,
    const std::string& payload,
    uint32_t senderId,
    int priority
) {
    CommandMessage msg;
    msg.topic = topic;
    msg.payload = payload;
    msg.timestampMs = getCurrentTimeMillis();
    msg.senderId = senderId;
    msg.priority = priority;

    std::vector<MessageCallback> dispatchList;

    {
        std::lock_guard<std::mutex> lock(brokerMutex_);
        if (messageQueue_.size() < maxQueueCapacity_) {
            messageQueue_.push(msg);
        }

        // Direct matching dispatch
        for (const auto& [cid, session] : clients_) {
            if (cid == senderId) continue; // don't echo back to sender

            for (const auto& pattern : session.subscribedTopics) {
                if (topicMatches(pattern, topic)) {
                    if (session.callback) {
                        dispatchList.push_back(session.callback);
                    }
                    break;
                }
            }
        }
    }

    for (const auto& cb : dispatchList) {
        try {
            cb(msg);
        } catch (...) {
            // Protect broker against rogue subscriber callbacks
        }
    }

    return dispatchList.size();
}

size_t RescueCommandBroker::processQueue(size_t maxCount) {
    std::lock_guard<std::mutex> lock(brokerMutex_);
    size_t processed = 0;
    while (!messageQueue_.empty() && processed < maxCount) {
        messageQueue_.pop();
        processed++;
    }
    return processed;
}

std::vector<uint8_t> RescueCommandBroker::encodeWebSocketFrame(
    const std::string& payload,
    WsOpcode opcode,
    bool mask
) {
    std::vector<uint8_t> frame;
    size_t len = payload.size();

    // Byte 0: FIN (0x80) | opcode
    uint8_t b0 = 0x80 | static_cast<uint8_t>(opcode);
    frame.push_back(b0);

    // Byte 1: Mask bit | Payload length indicator
    uint8_t maskBit = mask ? 0x80 : 0x00;

    if (len < 126) {
        frame.push_back(maskBit | static_cast<uint8_t>(len));
    } else if (len <= 0xFFFF) {
        frame.push_back(maskBit | 126);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(maskBit | 127);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }

    if (mask) {
        uint8_t maskingKey[4] = {0xDE, 0xAD, 0xBE, 0xEF};
        for (int i = 0; i < 4; ++i) {
            frame.push_back(maskingKey[i]);
        }
        for (size_t i = 0; i < len; ++i) {
            frame.push_back(static_cast<uint8_t>(payload[i]) ^ maskingKey[i % 4]);
        }
    } else {
        frame.insert(frame.end(), payload.begin(), payload.end());
    }

    return frame;
}

bool RescueCommandBroker::decodeWebSocketFrame(
    const uint8_t* data,
    size_t length,
    std::string& outPayload,
    WsOpcode& outOpcode,
    size_t& outBytesConsumed
) {
    if (data == nullptr || length < 2) return false;

    // Byte 0
    uint8_t b0 = data[0];
    bool fin = (b0 & 0x80) != 0;
    outOpcode = static_cast<WsOpcode>(b0 & 0x0F);
    (void)fin;

    // Byte 1
    uint8_t b1 = data[1];
    bool isMasked = (b1 & 0x80) != 0;
    uint64_t payloadLen = b1 & 0x7F;

    size_t headerSize = 2;
    if (payloadLen == 126) {
        if (length < 4) return false;
        payloadLen = (static_cast<uint64_t>(data[2]) << 8) | static_cast<uint64_t>(data[3]);
        headerSize += 2;
    } else if (payloadLen == 127) {
        if (length < 10) return false;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | static_cast<uint64_t>(data[2 + i]);
        }
        headerSize += 8;
    }

    size_t maskKeySize = isMasked ? 4 : 0;
    if (length < headerSize + maskKeySize + payloadLen) {
        return false; // incomplete frame
    }

    const uint8_t* maskKey = data + headerSize;
    const uint8_t* payloadData = data + headerSize + maskKeySize;

    outPayload.resize(payloadLen);
    if (isMasked) {
        for (size_t i = 0; i < payloadLen; ++i) {
            outPayload[i] = static_cast<char>(payloadData[i] ^ maskKey[i % 4]);
        }
    } else {
        std::copy(payloadData, payloadData + payloadLen, outPayload.begin());
    }

    outBytesConsumed = headerSize + maskKeySize + payloadLen;
    return true;
}

size_t RescueCommandBroker::getConnectedClientCount() const {
    std::lock_guard<std::mutex> lock(brokerMutex_);
    return clients_.size();
}

size_t RescueCommandBroker::getPendingQueueSize() const {
    std::lock_guard<std::mutex> lock(brokerMutex_);
    return messageQueue_.size();
}

} // namespace georescux::backend::realtime
