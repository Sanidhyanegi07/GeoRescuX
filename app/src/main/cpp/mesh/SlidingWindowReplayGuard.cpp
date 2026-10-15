#include "SlidingWindowReplayGuard.h"

namespace georescux::mesh {

bool SlidingWindowReplayGuard::checkAndUpdate(uint32_t senderHash, uint32_t seqNumber) {
    if (seqNumber == 0) return false;

    auto& state = senderWindows_[senderHash];

    // Case 1: First packet from sender
    if (state.maxSeqNumber == 0) {
        state.maxSeqNumber = seqNumber;
        state.bitmap = 1ULL;
        return true;
    }

    // Case 2: New packet ahead of current window
    if (seqNumber > state.maxSeqNumber) {
        const uint32_t diff = seqNumber - state.maxSeqNumber;
        if (diff < WINDOW_SIZE) {
            state.bitmap <<= diff;
            state.bitmap |= 1ULL;
        } else {
            state.bitmap = 1ULL; // Advanced past full window
        }
        state.maxSeqNumber = seqNumber;
        return true;
    }

    // Case 3: Packet inside window
    const uint32_t diff = state.maxSeqNumber - seqNumber;
    if (diff < WINDOW_SIZE) {
        const uint64_t mask = 1ULL << diff;
        if ((state.bitmap & mask) != 0) {
            return false; // Already received (duplicate/replay)
        }
        state.bitmap |= mask;
        return true;
    }

    // Case 4: Packet falls behind window (too old)
    return false;
}

void SlidingWindowReplayGuard::reset(uint32_t senderHash) {
    senderWindows_.erase(senderHash);
}

void SlidingWindowReplayGuard::clearAll() {
    senderWindows_.clear();
}

} // namespace georescux::mesh
