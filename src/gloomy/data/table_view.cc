#include "gloomy/data/table_view.h"

namespace gloomy::data {

namespace {

// Recent results kept for instant Backspace and re-sorting.
constexpr std::size_t kRecentViews = 8;

}  // namespace

TableView::TableView(std::shared_ptr<const TableSource> source)
    : source_(std::move(source)), current_(std::make_shared<RowView>()) {}

TableView::~TableView() {
  cancel_ = true;
  if (worker_.joinable()) {
    worker_.request_stop();
    cv_.notify_all();
  }
}

void TableView::SetSource(std::shared_ptr<const TableSource> source) {
  source_ = std::move(source);
  dirty_ = true;
}

void TableView::SetQuery(const Query &query) {
  if (query == query_) {
    return;
  }
  query_ = query;
  dirty_ = true;
  // The data did not change: no need to wait for the refresh interval.
  last_refresh_ = -1e9;
}

auto TableView::Cached(const Query &query, std::uint64_t version) const
    -> std::shared_ptr<const RowView> {
  for (const auto &view : recent_) {
    if (view->version == version && view->query == query) {
      return view;
    }
  }
  return nullptr;
}

auto TableView::Run(const Job &job, const std::atomic<bool> *cancel) -> Done {
  Done done;
  done.ticket = job.ticket;
  done.corpus = job.corpus;
  const std::uint64_t version = job.source->Version();
  const bool append = job.append && job.prev;
  if (!job.query.search.empty() &&
      (!done.corpus || done.corpus->version() != version)) {
    if (append && done.corpus &&
        done.corpus->size() <= job.source->RowCount()) {
      // Append-only: extend the corpus instead of rebuilding it.
      done.corpus = SearchCorpus::Extend(*done.corpus, *job.source);
    } else {
      done.corpus =
          SearchCorpus::Shared(*job.source, job.query.search_columns, cancel);
    }
    if (!done.corpus) {
      return done;
    }
  }
  if (append) {
    auto result = ExecuteAppend(
        *job.source, job.query, *job.prev, done.corpus.get(), cancel);
    if (result) {
      done.view = std::make_shared<RowView>(std::move(*result));
    }
    return done;
  }
  auto result = Execute(
      *job.source, job.query, job.prev.get(), done.corpus.get(), cancel);
  if (result) {
    done.view = std::make_shared<RowView>(std::move(*result));
  }
  return done;
}

void TableView::Apply(Done done) {
  pending_ = false;
  if (!done.view || done.ticket != ticket_) {
    return;
  }
  if (done.corpus) {
    corpus_ = done.corpus;
  }
  current_ = done.view;
  terms_ = ParseTerms(current_->query.search);
  ++generation_;
  recent_.push_front(current_);
  if (recent_.size() > kRecentViews) {
    recent_.pop_back();
  }
}

void TableView::Start() {
  const std::uint64_t version = source_->Version();
  ++ticket_;
  dirty_ = false;
  if (auto cached = Cached(query_, version)) {
    RowView copy = *cached;
    copy.path = RowView::Path::kCached;
    Done done;
    done.view = std::make_shared<RowView>(std::move(copy));
    done.ticket = ticket_;
    Apply(std::move(done));
    return;
  }
  Job job;
  job.source = source_;
  job.query = query_;
  job.ticket = ticket_;
  // Start from the smallest compatible earlier result (Backspace then
  // retyping, switching sorts), else from scratch.
  job.prev = nullptr;
  auto consider = [&](const std::shared_ptr<const RowView> &view) {
    if (view->version != version || (!IsNarrowing(view->query, query_) &&
                                     !SameSelection(view->query, query_))) {
      return;
    }
    if (!job.prev || view->rows.size() < job.prev->rows.size()) {
      job.prev = view;
    }
  };
  consider(current_);
  for (const auto &view : recent_) {
    consider(view);
  }
  // Reuse the corpus if it covers the same columns of the same data.
  std::vector<int> columns = query_.search_columns;
  if (columns.empty()) {
    const std::span<const ColumnInfo> infos = source_->Columns();
    for (std::size_t c = 0; c < infos.size(); ++c) {
      if (infos[c].searchable) {
        columns.push_back(static_cast<int>(c));
      }
    }
  }
  if (corpus_ && corpus_->version() == version &&
      corpus_->columns() == columns) {
    job.corpus = corpus_;
  }
  // An append-only source that only grew: extend the current result.
  if (source_->AppendOnly() && query_.tree_parent < 0 &&
      current_->query == query_ && current_->version != version &&
      current_->version != 0 && current_->source_rows <= source_->RowCount()) {
    job.prev = current_;
    job.append = true;
    if (corpus_ && corpus_->columns() == columns) {
      job.corpus = corpus_;
    }
  }
  pending_ = true;
  if (source_->RowCount() < sync_threshold_) {
    Apply(Run(job, nullptr));
    return;
  }
  // Hand the job to the worker, cancelling whatever runs.
  {
    const std::lock_guard lock(mutex_);
    cancel_ = true;
    job_ = std::move(job);
  }
  if (!worker_.joinable()) {
    worker_ = std::jthread([this](std::stop_token stop) { WorkerLoop(stop); });
  }
  cv_.notify_all();
}

void TableView::WorkerLoop(const std::stop_token &stop) {
  while (!stop.stop_requested()) {
    Job job;
    {
      std::unique_lock lock(mutex_);
      cv_.wait(lock, stop, [&] { return job_.has_value(); });
      if (stop.stop_requested()) {
        return;
      }
      job = std::move(*job_);
      job_.reset();
      cancel_ = false;
    }
    Done done = Run(job, &cancel_);
    const std::lock_guard lock(mutex_);
    done_ = std::move(done);
  }
}

auto TableView::Update(double time) -> bool {
  const std::uint64_t before = generation_;
  // Pick up finished work.
  std::optional<Done> done;
  {
    const std::lock_guard lock(mutex_);
    done.swap(done_);
  }
  if (done) {
    const bool stale = done->ticket != ticket_;
    Apply(std::move(*done));
    if (stale) {
      // A newer job is still running.
      pending_ = true;
    }
  }
  // A data change re-queries, throttled, and never cancels a job in
  // flight (that job would never finish while data keeps changing); its
  // result is followed by another refresh if the data moved on.
  if (!dirty_ && !pending_ && current_->version != source_->Version() &&
      time - last_refresh_ >= refresh_seconds_) {
    dirty_ = true;
  }
  if (dirty_) {
    last_refresh_ = time;
    Start();
  }
  return generation_ != before;
}

void TableView::Finish() {
  while (true) {
    Update(last_refresh_);
    if (!pending_) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

auto TableView::PositionOfKey(std::uint64_t key) const -> std::int64_t {
  const std::vector<RowIndex> &rows = current_->rows;
  for (std::size_t i = 0; i < rows.size(); ++i) {
    if (source_->RowKey(rows[i]) == key) {
      return static_cast<std::int64_t>(i);
    }
  }
  return -1;
}

}  // namespace gloomy::data
