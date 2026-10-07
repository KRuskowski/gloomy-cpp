#ifndef GLOOMY_DATA_DISPLAY_ROWS_H_
#define GLOOMY_DATA_DISPLAY_ROWS_H_

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "gloomy/data/query.h"

namespace gloomy::data {

/// What a grid shows line by line: data rows and, with grouping, group
/// header lines, leaving out the contents of collapsed groups. Without
/// grouping it is the identity and allocates nothing.
class DisplayRows {
 public:
  /// One line: a group header (`group` >= 0, index into RowView::groups)
  /// or a data row (`pos` indexes RowView::rows).
  struct Line {
    std::int32_t group = -1;
    std::uint32_t pos = 0;
  };

  /// Rebuilds from `view`; groups whose id is in `collapsed` hide their
  /// rows and subgroups. For tree data, rows whose RowKey (from `source`)
  /// is in `collapsed` hide their descendants.
  void Build(const RowView &view,
             const std::unordered_set<std::uint64_t> &collapsed,
             const TableSource *source = nullptr);

  auto size() const -> std::size_t {
    return identity_ ? count_ : lines_.size();
  }

  auto At(std::size_t i) const -> Line {
    return identity_ ? Line{-1, static_cast<std::uint32_t>(i)} : lines_[i];
  }

  /// Line of the data row at view position `pos`; -1 if it is hidden in a
  /// collapsed group.
  auto LineOfPos(std::uint32_t pos) const -> std::int64_t {
    return identity_ ? static_cast<std::int64_t>(pos) : line_of_pos_[pos];
  }

  /// Line of the header of group `group`, -1 if hidden.
  auto LineOfGroup(std::int32_t group) const -> std::int64_t {
    return identity_ ? -1 : line_of_group_[static_cast<std::size_t>(group)];
  }

  auto grouped() const -> bool { return !identity_; }

 private:
  bool identity_ = true;
  std::size_t count_ = 0;
  std::vector<Line> lines_;
  std::vector<std::int64_t> line_of_pos_;
  std::vector<std::int64_t> line_of_group_;
};

}  // namespace gloomy::data

#endif  // GLOOMY_DATA_DISPLAY_ROWS_H_
