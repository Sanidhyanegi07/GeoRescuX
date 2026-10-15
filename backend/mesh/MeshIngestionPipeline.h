#ifndef GEORESCUX_BACKEND_MESH_INGESTION_PIPELINE_H
#define GEORESCUX_BACKEND_MESH_INGESTION_PIPELINE_H

#include <vector>
#include <string>
#include <cstdint>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <thread>
#include <functional>

namespace georescux::backend::mesh {

struct IngestedPacket {
    std::string gatewayId;
    std::string senderId;
    uint32_t senderHash;
    uint16_t sequenceNumber;
    uint8_t priority;
    uint8_t ttl;
    double latitude;
    double longitude;
    double altitudeMeters;
    uint8_t batteryPercent;
    uint8_t medicalUrgency;
    uint16_t peopleCount;
    uint32_t timestampEpochSec;
    int64_t ingestionTimeMs;
};

/**
 * Thread-safe ring buffer and batch ingestion pipeline.
 * Decouples high-throughput network ingest from disk/database writes.
 */
class MeshIngestionPipeline {
public:
    using BatchFlushCallback = std::function<void(const std::vector<IngestedPacket>&)>;

    explicit MeshIngestionPipeline(
        size_t maxQueueCapacity = 20000,
        size_t batchFlushSize = 100,
        int flushIntervalMs = 250
    );

    ~MeshIngestionPipeline();

    // Non-copyable
    MeshIngestionPipeline(const MeshIngestionPipeline&) = delete;
    MeshIngestionPipeline& operator=(const MeshIngestionPipeline&) = delete;

    /**
     * Starts the background batch worker thread.
     */
    void start(BatchFlushCallback callback);

    /**
     * Stops the worker thread and flushes remaining queue items.
     */
    void stop();

    /**
     * Thread-safe ingestion of a packet.
     * Returns true if queued, false if buffer is full (overflow protection).
     */
    bool enqueue(IngestedPacket packet);

    [[nodiscard]] size_t getQueuedCount() const noexcept;
    [[nodiscard]] uint64_t getTotalIngested() const noexcept { return totalIngested_.load(); }
    [[nodiscard]] uint64_t getTotalDropped() const noexcept { return totalDropped_.load(); }

private:
    size_t maxQueueCapacity_;
    size_t batchFlushSize_;
    int flushIntervalMs_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<IngestedPacket> queue_;

    std::atomic<bool> isRunning_{false};
    std::thread workerThread_;
    BatchFlushCallback flushCallback_;

    std::atomic<uint64_t> totalIngested_{0};
    std::atomic<uint64_t> totalDropped_{0};

    void workerLoop();
};

} // namespace georescux::backend::mesh

#endif // GEORESCUX_BACKEND_MESH_INGESTION_PIPELINE_H
