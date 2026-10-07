#ifndef GLOOMY_CORE_PARALLEL_H_
#define GLOOMY_CORE_PARALLEL_H_

#include <algorithm>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

namespace gloomy {

/// Worker threads used for large data jobs. Defaults to half the hardware
/// threads, at most 4, since gloomy shares the CPU with the application.
auto WorkerThreads() -> int;

/// Overrides WorkerThreads() (1 disables parallelism).
void SetWorkerThreads(int threads);

/// Splits [0, count) into contiguous chunks and runs `fn(begin, end, chunk)`
/// on up to WorkerThreads() threads; returns when all are done. Runs inline
/// when `count` is below `min_parallel`.
void ParallelChunks(
    std::size_t count, std::size_t min_parallel,
    const std::function<void(std::size_t, std::size_t, int)> &fn);

/// Number of chunks ParallelChunks() will use for `count` items.
auto ChunkCount(std::size_t count, std::size_t min_parallel) -> int;

}  // namespace gloomy

#endif  // GLOOMY_CORE_PARALLEL_H_
