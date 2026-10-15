#include "MeshIngestionPipeline.h"
#include <chrono>

namespace georescux::backend::mesh {

MeshIngestionPipeline::MeshIngestionPipeline(
    size_t maxQueueCapacity,
    size_t batchFlushSize,
    int flushIntervalMs
) : maxQueueCapacity_(std::max<size_t>(100, maxQueueCapacity)),
    batchFlushSize_(std::max<size_t>(1, batchFlushSize)),
    flushIntervalMs_(std::max(10, flushIntervalMs)) {}

MeshIngestionPipeline::~MeshIngestionPipeline() {
    stop();
}

void MeshIngestionPipeline::start(BatchFlushCallback callback) {
    if (isRunning_.exchange(true)) return;
    flushCallback_ = std::move(callback);
    workerThread_ = std::thread(&MeshIngestionPipeline::workerLoop, this);
}

void MeshIngestionPipeline::stop() {
    if (!isRunning_.exchange(false)) return;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        cv_.notify_all();
    }

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
}

bool MeshIngestionPipeline::enqueue(IngestedPacket packet) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= maxQueueCapacity_) {
            totalDropped_++;
            return false;
        }
        queue_.push_back(std::move(packet));
        totalIngested_++;
    }
    cv_.notify_one();
    return true;
}

size_t MeshIngestionPipeline::getQueuedCount() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void MeshIngestionPipeline::workerLoop() {
    std::vector<IngestedPacket> batch;
    batch.reserve(batchFlushSize_);

    while (isRunning_.load()) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(flushIntervalMs_), [this]() {
                return !isRunning_.load() || queue_.size() >= batchFlushSize_;
            });

            if (!isRunning_.load() && queue_.empty()) {
                break;
            }

            const size_t toFetch = std::min(batchFlushSize_, queue_.size());
            for (size_t i = 0; i < toFetch; ++i) {
                batch.push_back(std::move(queue_.front()));
                queue_.pop_front();
            }
        }

        if (!batch.empty()) {
            if (flushCallback_) {
                flushCallback_(batch);
            }
            batch.clear();
        }
    }

    // Drain remaining items on shutdown
    std::vector<IngestedPacket> remaining;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) {
            remaining.push_back(std::move(queue_.front()));
            queue_.pop_front();
        }
    }
    if (!remaining.empty() && flushCallback_) {
        flushCallback_(remaining);
    }
}

} // namespace georescux::backend::mesh
