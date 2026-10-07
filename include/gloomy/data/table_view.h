#ifndef GLOOMY_DATA_TABLE_VIEW_H_
#define GLOOMY_DATA_TABLE_VIEW_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "gloomy/data/query.h"
#include "gloomy/data/search.h"
#include "gloomy/data/table_source.h"

namespace gloomy::data {

/// A live view of a table: the current query, its result, and the work to
/// keep the result up to date without ever blocking a frame
/// (docs/design.md sections 10.2 and 14).
///
/// Small tables are queried synchronously in Update(). Large tables are
/// queried on a worker thread against the shared source, which must then
/// not change while shared (publish a new snapshot with SetSource()). The
/// previous result stays visible until the new one is ready; a newer query
/// cancels an older one.
class TableView {
 public:
  explicit TableView(std::shared_ptr<const TableSource> source);
  TableView(const TableView &) = delete;
  auto operator=(const TableView &) -> TableView & = delete;
  ~TableView();

  /// Replaces the source (a new snapshot, or the same object after its
  /// version changed).
  void SetSource(std::shared_ptr<const TableSource> source);

  /// Requests a new query; the result arrives through Update().
  void SetQuery(const Query &query);

  /// Call once per frame. Starts or finishes work; re-queries when the
  /// source version changed, at most every `refresh_seconds`.
  /// @param[in] time Monotonic seconds.
  /// @return True if the displayed rows changed.
  auto Update(double time = 0.0) -> bool;

  /// Waits for pending work and applies it (tests, benchmarks).
  void Finish();

  /// Displayed rows, in display order.
  auto rows() const -> std::span<const RowIndex> { return current_->rows; }

  auto view() const -> const RowView & { return *current_; }

  /// The requested query (may still be running).
  auto query() const -> const Query & { return query_; }

  /// Folded search terms of the displayed result, for highlighting.
  auto terms() const -> const std::vector<std::string> & { return terms_; }

  auto busy() const -> bool { return pending_; }

  auto source() const -> const TableSource & { return *source_; }

  auto source_ptr() const -> const std::shared_ptr<const TableSource> & {
    return source_;
  }

  /// Increments whenever rows() changes.
  auto generation() const -> std::uint64_t { return generation_; }

  /// Tables with fewer rows are queried synchronously.
  void SetSyncThreshold(RowIndex rows) { sync_threshold_ = rows; }

  /// Minimum seconds between re-queries caused by data changes.
  void SetRefreshInterval(double seconds) { refresh_seconds_ = seconds; }

  /// Position of the row with key `key` in rows(), or -1. O(n).
  auto PositionOfKey(std::uint64_t key) const -> std::int64_t;

 private:
  struct Job {
    std::shared_ptr<const TableSource> source;
    Query query;
    std::shared_ptr<const RowView> prev;
    std::shared_ptr<const SearchCorpus> corpus;
    std::uint64_t ticket = 0;
    /// Extend `prev` with appended rows instead of running the query.
    bool append = false;
  };

  struct Done {
    std::shared_ptr<const RowView> view;
    std::shared_ptr<const SearchCorpus> corpus;
    std::uint64_t ticket = 0;
  };

  void Start();

  auto Run(const Job &job, const std::atomic<bool> *cancel) -> Done;

  void Apply(Done done);

  void WorkerLoop(const std::stop_token &stop);

  auto Cached(const Query &query, std::uint64_t version) const
      -> std::shared_ptr<const RowView>;

  std::shared_ptr<const TableSource> source_;
  Query query_;
  std::shared_ptr<const RowView> current_;
  std::shared_ptr<const SearchCorpus> corpus_;
  std::deque<std::shared_ptr<const RowView>> recent_;
  std::vector<std::string> terms_;
  std::uint64_t generation_ = 0;
  RowIndex sync_threshold_ = 50000;
  double refresh_seconds_ = 0.25;
  double last_refresh_ = -1e9;
  bool dirty_ = true;
  bool pending_ = false;
  std::uint64_t ticket_ = 0;

  // Worker.
  std::mutex mutex_;
  std::condition_variable_any cv_;
  std::optional<Job> job_;
  std::optional<Done> done_;
  std::atomic<bool> cancel_{false};
  std::jthread worker_;
};

}  // namespace gloomy::data

#endif  // GLOOMY_DATA_TABLE_VIEW_H_
