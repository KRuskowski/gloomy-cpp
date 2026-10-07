#include "gloomy/data/search.h"

#include <algorithm>
#include <cstring>
#include <mutex>

#include "gloomy/core/parallel.h"
#include "gloomy/core/utf8.h"

namespace gloomy::data {

namespace {

// Rows per cancellation check and per parallel work unit.
constexpr std::size_t kChunk = 4096;
constexpr std::size_t kMinParallel = 50000;

auto Contains(std::string_view hay, std::string_view needle) -> bool {
  if (needle.empty()) {
    return true;
  }
  if (needle.size() > hay.size()) {
    return false;
  }
  // memchr for the first byte, then compare; fast for short needles.
  const char first = needle[0];
  const char *p = hay.data();
  const char *end = hay.data() + hay.size() - needle.size() + 1;
  while (p < end) {
    p = static_cast<const char *>(
        std::memchr(p, first, static_cast<std::size_t>(end - p)));
    if (p == nullptr) {
      return false;
    }
    if (std::memcmp(p, needle.data(), needle.size()) == 0) {
      return true;
    }
    ++p;
  }
  return false;
}

}  // namespace

auto ParseTerms(std::string_view query) -> std::vector<std::string> {
  std::vector<std::string> terms;
  std::string folded;
  FoldCaseUtf8(query, folded);
  std::size_t i = 0;
  while (i < folded.size()) {
    while (i < folded.size() && folded[i] == ' ') {
      ++i;
    }
    if (i >= folded.size()) {
      break;
    }
    std::size_t end = 0;
    if (folded[i] == '"') {
      ++i;
      end = folded.find('"', i);
      if (end == std::string::npos) {
        end = folded.size();
      }
      if (end > i) {
        terms.push_back(folded.substr(i, end - i));
      }
      i = end + 1;
      continue;
    }
    end = folded.find(' ', i);
    if (end == std::string::npos) {
      end = folded.size();
    }
    terms.push_back(folded.substr(i, end - i));
    i = end;
  }
  return terms;
}

auto TermsNarrow(std::span<const std::string> prev,
                 std::span<const std::string> next) -> bool {
  for (const std::string &old_term : prev) {
    const bool covered =
        std::any_of(next.begin(), next.end(), [&](const std::string &t) {
          return t.find(old_term) != std::string::npos;
        });
    if (!covered) {
      return false;
    }
  }
  return true;
}

void FindMatches(std::string_view text, std::span<const std::string> terms,
                 std::vector<MatchRange> &out) {
  if (terms.empty() || text.empty()) {
    return;
  }
  std::string folded;
  FoldCaseUtf8(text, folded);
  const std::size_t first = out.size();
  for (const std::string &term : terms) {
    if (term.empty()) {
      continue;
    }
    std::size_t pos = 0;
    while ((pos = folded.find(term, pos)) != std::string::npos) {
      out.push_back({static_cast<std::uint32_t>(pos),
                     static_cast<std::uint32_t>(pos + term.size())});
      pos += term.size();
    }
  }
  std::sort(out.begin() + static_cast<std::ptrdiff_t>(first),
            out.end(),
            [](const MatchRange &a, const MatchRange &b) {
              return a.begin < b.begin;
            });
  // Merge overlaps.
  std::size_t w = first;
  for (std::size_t r = first; r < out.size(); ++r) {
    if (w > first && out[r].begin <= out[w - 1].end) {
      out[w - 1].end = std::max(out[w - 1].end, out[r].end);
    } else {
      out[w++] = out[r];
    }
  }
  out.resize(w);
}

auto SearchCorpus::Build(const TableSource &source,
                         std::span<const int> columns,
                         const std::atomic<bool> *cancel)
    -> std::shared_ptr<SearchCorpus> {
  auto corpus = std::make_shared<SearchCorpus>();
  corpus->version_ = source.Version();
  if (columns.empty()) {
    const std::span<const ColumnInfo> infos = source.Columns();
    for (std::size_t c = 0; c < infos.size(); ++c) {
      if (infos[c].searchable) {
        corpus->columns_.push_back(static_cast<int>(c));
      }
    }
  } else {
    corpus->columns_.assign(columns.begin(), columns.end());
  }
  const RowIndex rows = source.RowCount();
  const int chunks = ChunkCount(rows, kMinParallel);
  // Each chunk builds its own blob; they are concatenated afterwards.
  std::vector<std::string> blobs(static_cast<std::size_t>(chunks));
  std::vector<std::vector<std::uint64_t>> ends(
      static_cast<std::size_t>(chunks));
  std::atomic<bool> cancelled{false};
  ParallelChunks(
      rows, kMinParallel, [&](std::size_t begin, std::size_t end, int chunk) {
        std::string &blob = blobs[static_cast<std::size_t>(chunk)];
        std::vector<std::uint64_t> &e = ends[static_cast<std::size_t>(chunk)];
        e.reserve(end - begin);
        std::string cell;
        std::string folded;
        for (std::size_t r = begin; r < end; ++r) {
          if ((r - begin) % kChunk == 0 && cancel != nullptr &&
              cancel->load(std::memory_order_relaxed)) {
            cancelled = true;
            return;
          }
          for (std::size_t i = 0; i < corpus->columns_.size(); ++i) {
            if (i > 0) {
              blob.push_back(kSegmentSeparator);
            }
            source.FormatCell(
                static_cast<RowIndex>(r), corpus->columns_[i], cell);
            FoldCaseUtf8(cell, folded);
            blob += folded;
          }
          e.push_back(blob.size());
        }
      });
  if (cancelled) {
    return nullptr;
  }
  std::size_t total = 0;
  for (const std::string &b : blobs) {
    total += b.size();
  }
  corpus->blob_.reserve(total);
  corpus->offsets_.reserve(rows + 1);
  for (std::size_t c = 0; c < blobs.size(); ++c) {
    const std::uint64_t base = corpus->blob_.size();
    corpus->blob_ += blobs[c];
    for (const std::uint64_t e : ends[c]) {
      corpus->offsets_.push_back(base + e);
    }
  }
  return corpus;
}

namespace {

struct CacheEntry {
  const TableSource *source;
  std::uint64_t version;
  RowIndex rows;
  std::vector<int> columns;
  std::weak_ptr<const SearchCorpus> corpus;
};

std::mutex g_cache_mutex;
std::vector<CacheEntry> g_cache;

}  // namespace

auto SearchCorpus::Shared(const TableSource &source,
                          std::span<const int> columns,
                          const std::atomic<bool> *cancel)
    -> std::shared_ptr<const SearchCorpus> {
  std::vector<int> wanted(columns.begin(), columns.end());
  if (wanted.empty()) {
    const std::span<const ColumnInfo> infos = source.Columns();
    for (std::size_t c = 0; c < infos.size(); ++c) {
      if (infos[c].searchable) {
        wanted.push_back(static_cast<int>(c));
      }
    }
  }
  const std::uint64_t version = source.Version();
  const RowIndex rows = source.RowCount();
  {
    const std::lock_guard lock(g_cache_mutex);
    std::erase_if(g_cache,
                  [](const CacheEntry &e) { return e.corpus.expired(); });
    for (const CacheEntry &e : g_cache) {
      if (e.source == &source && e.version == version && e.rows == rows &&
          e.columns == wanted) {
        if (auto corpus = e.corpus.lock()) {
          return corpus;
        }
      }
    }
  }
  std::shared_ptr<const SearchCorpus> corpus = Build(source, wanted, cancel);
  if (corpus) {
    const std::lock_guard lock(g_cache_mutex);
    g_cache.push_back({&source, version, rows, wanted, corpus});
  }
  return corpus;
}

auto SearchCorpus::Extend(const SearchCorpus &base, const TableSource &source)
    -> std::shared_ptr<SearchCorpus> {
  auto corpus = std::make_shared<SearchCorpus>(base);
  corpus->version_ = source.Version();
  std::string cell;
  std::string folded;
  for (RowIndex r = base.size(); r < source.RowCount(); ++r) {
    for (std::size_t i = 0; i < corpus->columns_.size(); ++i) {
      if (i > 0) {
        corpus->blob_.push_back(kSegmentSeparator);
      }
      source.FormatCell(r, corpus->columns_[i], cell);
      FoldCaseUtf8(cell, folded);
      corpus->blob_ += folded;
    }
    corpus->offsets_.push_back(corpus->blob_.size());
  }
  return corpus;
}

void SearchCorpus::Add(std::string_view text) {
  std::string folded;
  FoldCaseUtf8(text, folded);
  blob_ += folded;
  offsets_.push_back(blob_.size());
}

auto SearchCorpus::Matches(std::uint32_t doc,
                           std::span<const std::string> terms) const -> bool {
  const std::string_view text = Doc(doc);
  return std::all_of(terms.begin(), terms.end(), [&](const std::string &t) {
    return Contains(text, t);
  });
}

auto SearchCorpus::Filter(std::vector<std::uint32_t> &docs,
                          std::span<const std::string> terms,
                          const std::atomic<bool> *cancel) const -> bool {
  if (terms.empty()) {
    return true;
  }
  std::vector<std::uint8_t> keep(docs.size(), 0);
  std::atomic<bool> cancelled{false};
  ParallelChunks(
      docs.size(), kMinParallel, [&](std::size_t begin, std::size_t end, int) {
        for (std::size_t i = begin; i < end; ++i) {
          if ((i - begin) % kChunk == 0 && cancel != nullptr &&
              cancel->load(std::memory_order_relaxed)) {
            cancelled = true;
            return;
          }
          keep[i] = Matches(docs[i], terms) ? 1 : 0;
        }
      });
  if (cancelled) {
    return false;
  }
  std::size_t w = 0;
  for (std::size_t i = 0; i < docs.size(); ++i) {
    if (keep[i] != 0) {
      docs[w++] = docs[i];
    }
  }
  docs.resize(w);
  return true;
}

auto SearchCorpus::SegmentAt(std::uint32_t doc, std::uint32_t offset) const
    -> int {
  const std::string_view text = Doc(doc);
  int segment = 0;
  for (std::uint32_t i = 0; i < offset && i < text.size(); ++i) {
    if (text[i] == kSegmentSeparator) {
      ++segment;
    }
  }
  return segment;
}

}  // namespace gloomy::data
