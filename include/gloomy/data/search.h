#ifndef GLOOMY_DATA_SEARCH_H_
#define GLOOMY_DATA_SEARCH_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gloomy/data/table_source.h"

namespace gloomy::data {

/// Separates columns inside one corpus document.
inline constexpr char kSegmentSeparator = '\x1F';

/// Splits a search string into case-folded terms at spaces; text in double
/// quotes is one term. All terms must match (AND).
auto ParseTerms(std::string_view query) -> std::vector<std::string>;

/// True if every document matching `next` also matches `prev` (each old
/// term is contained in some new term), so a search for `next` may run on
/// the results of `prev` only.
auto TermsNarrow(std::span<const std::string> prev,
                 std::span<const std::string> next) -> bool;

/// A byte range [begin, end) in a text.
struct MatchRange {
  std::uint32_t begin = 0;
  std::uint32_t end = 0;
};

/// Appends every occurrence of any of the folded `terms` in `text`
/// (case-insensitive) to `out`, sorted and merged where they overlap.
/// Offsets are into `text` (folding keeps byte lengths).
void FindMatches(std::string_view text, std::span<const std::string> terms,
                 std::vector<MatchRange> &out);

/// Searchable text: a list of documents stored case-folded in one blob, so
/// tables, documents and global search share one search core
/// (docs/design.md section 10.3). A table corpus has one document per row,
/// the searchable columns joined by kSegmentSeparator.
class SearchCorpus {
 public:
  /// Builds a corpus over `columns` of `source` (all searchable columns if
  /// empty), one document per source row.
  /// @return nullptr if cancelled.
  static auto Build(const TableSource &source, std::span<const int> columns,
                    const std::atomic<bool> *cancel = nullptr)
      -> std::shared_ptr<SearchCorpus>;

  /// Like Build(), but shares corpora between callers: views of the same
  /// source version and columns (a grid and global search) get the same
  /// corpus while any of them holds it.
  static auto Shared(const TableSource &source, std::span<const int> columns,
                     const std::atomic<bool> *cancel = nullptr)
      -> std::shared_ptr<const SearchCorpus>;

  /// A copy of `base` extended with the rows `source` (append-only) added
  /// since `base` was built.
  static auto Extend(const SearchCorpus &base, const TableSource &source)
      -> std::shared_ptr<SearchCorpus>;

  /// Appends a document.
  void Add(std::string_view text);

  /// Folded text of document `doc`.
  auto Doc(std::uint32_t doc) const -> std::string_view {
    return std::string_view(blob_).substr(offsets_[doc],
                                          offsets_[doc + 1] - offsets_[doc]);
  }

  auto size() const -> std::uint32_t {
    return static_cast<std::uint32_t>(offsets_.size() - 1);
  }

  /// Bytes of text held.
  auto bytes() const -> std::size_t { return blob_.size(); }

  /// True if document `doc` contains all `terms`.
  auto Matches(std::uint32_t doc, std::span<const std::string> terms) const
      -> bool;

  /// Keeps (in order) the documents of `docs` that contain all `terms`.
  /// Large inputs are searched in parallel.
  /// @return False if cancelled; `docs` is then unspecified.
  auto Filter(std::vector<std::uint32_t> &docs,
              std::span<const std::string> terms,
              const std::atomic<bool> *cancel = nullptr) const -> bool;

  /// The segment (column position in the build list) containing byte
  /// `offset` of document `doc`.
  auto SegmentAt(std::uint32_t doc, std::uint32_t offset) const -> int;

  /// Source version and columns the corpus was built from.
  auto version() const -> std::uint64_t { return version_; }

  auto columns() const -> const std::vector<int> & { return columns_; }

 private:
  std::string blob_;
  std::vector<std::uint64_t> offsets_{0};
  std::uint64_t version_ = 0;
  std::vector<int> columns_;
};

}  // namespace gloomy::data

#endif  // GLOOMY_DATA_SEARCH_H_
