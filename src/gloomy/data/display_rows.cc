#include "gloomy/data/display_rows.h"

namespace gloomy::data {

void DisplayRows::Build(const RowView &view,
                        const std::unordered_set<std::uint64_t> &collapsed) {
  lines_.clear();
  line_of_pos_.clear();
  line_of_group_.clear();
  if (view.groups.empty()) {
    identity_ = true;
    count_ = view.rows.size();
    return;
  }
  identity_ = false;
  line_of_pos_.assign(view.rows.size(), -1);
  line_of_group_.assign(view.groups.size(), -1);
  const std::size_t depth = view.query.group_by.size();
  // A group is hidden if any ancestor is collapsed; groups are in
  // pre-order, so a parent is decided before its children.
  std::vector<std::uint8_t> hidden(view.groups.size(), 0);
  for (std::size_t g = 0; g < view.groups.size(); ++g) {
    const RowGroup &group = view.groups[g];
    if (group.parent >= 0) {
      const auto p = static_cast<std::size_t>(group.parent);
      hidden[g] = hidden[p] != 0 || collapsed.contains(view.groups[p].id);
    }
    if (hidden[g] != 0) {
      continue;
    }
    line_of_group_[g] = static_cast<std::int64_t>(lines_.size());
    lines_.push_back({static_cast<std::int32_t>(g), 0});
    const bool leaf = group.level + 1U == depth;
    if (leaf && !collapsed.contains(group.id)) {
      for (std::uint32_t i = group.first; i < group.first + group.count; ++i) {
        line_of_pos_[i] = static_cast<std::int64_t>(lines_.size());
        lines_.push_back({-1, i});
      }
    }
  }
}

}  // namespace gloomy::data
