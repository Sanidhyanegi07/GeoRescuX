#ifndef GEORESCUX_MESH_VECTOR_CLOCK_H
#define GEORESCUX_MESH_VECTOR_CLOCK_H

#include <unordered_map>
#include <string>
#include <cstdint>
#include <vector>

namespace georescux::mesh {

enum class CausalOrdering {
    HAPPENED_BEFORE,
    HAPPENED_AFTER,
    IDENTICAL,
    CONCURRENT
};

/**
 * Mattern / Fidge Vector Clock implementation for causal event ordering
 * in decentralized, partition-tolerant disaster mesh networks.
 */
class VectorClock {
public:
    VectorClock() = default;
    explicit VectorClock(std::string localNodeId);

    void tick() noexcept;

    /**
     * Merges an incoming vector clock from a peer message (element-wise maximum).
     */
    void merge(const VectorClock& other);

    /**
     * Compares two vector clocks to determine causal ordering.
     */
    static CausalOrdering compare(const VectorClock& a, const VectorClock& b) noexcept;

    void setClock(const std::string& nodeId, uint64_t val);
    [[nodiscard]] uint64_t getClock(const std::string& nodeId) const;

    [[nodiscard]] const std::string& getLocalNodeId() const noexcept { return localNodeId_; }
    [[nodiscard]] const std::unordered_map<std::string, uint64_t>& getClocks() const noexcept { return clocks_; }

    /**
     * Serializes clock into compact binary format.
     */
    [[nodiscard]] std::vector<uint8_t> serialize() const;
    static VectorClock deserialize(const uint8_t* data, size_t length);

private:
    std::string localNodeId_;
    std::unordered_map<std::string, uint64_t> clocks_;
};

} // namespace georescux::mesh

#endif // GEORESCUX_MESH_VECTOR_CLOCK_H
