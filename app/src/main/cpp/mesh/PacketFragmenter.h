#ifndef GEORESCUX_MESH_PACKET_FRAGMENTER_H
#define GEORESCUX_MESH_PACKET_FRAGMENTER_H

#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <chrono>
#include <optional>

namespace georescux::mesh {

#pragma pack(push, 1)
struct FragmentHeader {
    uint16_t messageId;
    uint8_t fragmentIndex : 4;  // 0 to 15
    uint8_t totalFragments : 4; // 1 to 16
    uint8_t checksum;
};
#pragma pack(pop)

/**
 * Segmentation and Reassembly (SAR) engine for BLE mesh packets.
 * Splits messages larger than BLE advertising MTU (20-24 bytes) into fragments
 * and reassembles them with missing-fragment timeout handling.
 */
class PacketFragmenter {
public:
    static constexpr size_t DEFAULT_MAX_FRAGMENT_PAYLOAD = 18; // bytes per fragment

    /**
     * Splits a large payload buffer into fragmented packets.
     */
    static std::vector<std::vector<uint8_t>> fragment(
        uint16_t messageId,
        const uint8_t* payload,
        size_t payloadLength,
        size_t maxFragmentPayload = DEFAULT_MAX_FRAGMENT_PAYLOAD
    );

    /**
     * Reassembly state manager.
     */
    class Reassembler {
    public:
        explicit Reassembler(int64_t timeoutSeconds = 60);

        /**
         * Feeds an incoming fragment.
         * Returns full reassembled message if this fragment completed the message,
         * or nullopt if still waiting for other fragments.
         */
        std::optional<std::vector<uint8_t>> feedFragment(const uint8_t* data, size_t length);

        /**
         * Cleans up partial messages that exceeded timeoutSeconds.
         */
        void pruneTimeouts();

    private:
        struct PendingMessage {
            uint8_t totalFragments{0};
            std::vector<std::vector<uint8_t>> fragments;
            std::vector<bool> received;
            std::chrono::steady_clock::time_point firstReceivedTime;
        };

        int64_t timeoutSeconds_;
        std::unordered_map<uint16_t, PendingMessage> pending_;
    };
};

} // namespace georescux::mesh

#endif // GEORESCUX_MESH_PACKET_FRAGMENTER_H
