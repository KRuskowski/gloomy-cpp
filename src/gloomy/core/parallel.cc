#include "gloomy/core/parallel.h"

#include <atomic>

namespace gloomy {

namespace {

std::atomic<int> g_threads{0};

}  // namespace

auto WorkerThreads() -> int {
  const int set = g_threads.load();
  if (set > 0) {
    return set;
  }
  const int hw = static_cast<int>(std::thread::hardware_concurrency());
  return std::clamp(hw / 2, 1, 4);
}

void SetWorkerThreads(int threads) {
  g_threads.store(std::max(1, threads));
}

auto ChunkCount(std::size_t count, std::size_t min_parallel) -> int {
  if (count < min_parallel || count < 2) {
    return 1;
  }
  return static_cast<int>(
      std::min<std::size_t>(static_cast<std::size_t>(WorkerThreads()), count));
}

void ParallelChunks(
    std::size_t count, std::size_t min_parallel,
    const std::function<void(std::size_t, std::size_t, int)> &fn) {
  const int chunks = ChunkCount(count, min_parallel);
  if (chunks <= 1) {
    fn(0, count, 0);
    return;
  }
  std::vector<std::jthread> threads;
  threads.reserve(static_cast<std::size_t>(chunks - 1));
  const std::size_t per = (count + chunks - 1) / chunks;
  for (int c = 1; c < chunks; ++c) {
    const std::size_t begin = per * static_cast<std::size_t>(c);
    const std::size_t end = std::min(count, begin + per);
    threads.emplace_back([&fn, begin, end, c] { fn(begin, end, c); });
  }
  fn(0, std::min(count, per), 0);
}

}  // namespace gloomy
