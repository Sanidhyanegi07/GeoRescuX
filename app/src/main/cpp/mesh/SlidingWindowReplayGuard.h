#ifndef GEORESCUX_MESH_SLIDING_WINDOW_REPLAY_GUARD_H
#define GEORESCUX_MESH_SLIDING_WINDOW_REPLAY_GUARD_H

#include <cstdint>
#include <unordered_map>
#include <string>

namespace georescux::mesh {

/**
 * IPsec-style 64-bit sliding window replay protector.
 * Tracks monotonically increasing sequence numbers per sender node.
 * Protects against delayed packet reinjection and replay attacks in real-time.
 */
class SlidingWindowReplayGuard {
public:
    static constexpr uint16_t WINDOW_SIZE = 64;

    struct WindowState {
        uint32_t maxSeqNumber{0};
        uint64_t bitmap{0}; // 1 = received, 0 = not received
    };

    /**
     * Checks if seqNumber is valid and not a replay.
     * If valid, updates the sliding window state and returns true.
     * If duplicate or too old, returns false.
     */
    bool checkAndUpdate(uint32_t senderHash, uint32_t seqNumber);

    void reset(uint32_t senderHash);
    void clearAll();

private:
    std::unordered_map<uint32_t, WindowState> senderWindows_;
};

} // namespace georescux::mesh

#endif // GEORESCUX_MESH_SLIDING_WINDOW_REPLAY_GUARD_H
