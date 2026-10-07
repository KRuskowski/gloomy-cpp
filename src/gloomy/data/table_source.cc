#include "gloomy/data/table_source.h"

#include <charconv>
#include <cmath>
#include <cstdio>

namespace gloomy::data {

auto TableSource::GetInt64(int col, RowIndex row) const -> std::int64_t {
  return static_cast<std::int64_t>(GetDouble(col, row));
}

auto TableSource::GetDouble(int col, RowIndex row) const -> double {
  return static_cast<double>(GetInt64(col, row));
}

auto TableSource::GetText(int col, RowIndex row) const -> std::string_view {
  (void)col;
  (void)row;
  return {};
}

void TableSource::ReadInt64(int col, std::span<const RowIndex> rows,
                            std::span<std::int64_t> out) const {
  for (std::size_t i = 0; i < rows.size(); ++i) {
    out[i] = GetInt64(col, rows[i]);
  }
}

void TableSource::ReadDouble(int col, std::span<const RowIndex> rows,
                             std::span<double> out) const {
  for (std::size_t i = 0; i < rows.size(); ++i) {
    out[i] = GetDouble(col, rows[i]);
  }
}

void TableSource::FormatCell(RowIndex row, int col, std::string &out) const {
  out.clear();
  const ColumnInfo &info = Columns()[col];
  switch (info.type) {
    case ColumnType::kInt64:
      FormatInt(GetInt64(col, row), info.group_digits, out);
      break;
    case ColumnType::kDouble: {
      const double v = GetDouble(col, row);
      if (std::isnan(v)) {
        return;
      }
      FormatDouble(v, info.decimals, info.group_digits, out);
      break;
    }
    case ColumnType::kText:
      out.assign(GetText(col, row));
      return;
    case ColumnType::kBool:
      out.assign(GetInt64(col, row) != 0 ? "yes" : "no");
      return;
    case ColumnType::kEnum: {
      const std::int64_t v = GetInt64(col, row);
      if (v >= 0 && v < static_cast<std::int64_t>(info.enum_labels.size())) {
        out.assign(info.enum_labels[static_cast<std::size_t>(v)]);
      }
      return;
    }
  }
  out += info.unit;
}

auto TableSource::FindColumn(std::string_view name) const -> int {
  const std::span<const ColumnInfo> columns = Columns();
  for (std::size_t i = 0; i < columns.size(); ++i) {
    if (columns[i].name == name) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

namespace {

// Inserts ',' every three digits of the integer part of `digits` (which may
// start with '-').
void Group(std::string_view digits, std::string &out) {
  std::size_t start = 0;
  if (!digits.empty() && digits[0] == '-') {
    out.push_back('-');
    start = 1;
  }
  const std::size_t n = digits.size() - start;
  for (std::size_t i = 0; i < n; ++i) {
    if (i > 0 && (n - i) % 3 == 0) {
      out.push_back(',');
    }
    out.push_back(digits[start + i]);
  }
}

}  // namespace

void FormatInt(std::int64_t value, bool group, std::string &out) {
  char buffer[24];
  const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
  const std::string_view digits(buffer,
                                static_cast<std::size_t>(result.ptr - buffer));
  if (group) {
    Group(digits, out);
  } else {
    out.append(digits);
  }
}

void FormatDouble(double value, int decimals, bool group, std::string &out) {
  char buffer[64];
  const auto result = std::to_chars(buffer,
                                    buffer + sizeof(buffer),
                                    value,
                                    std::chars_format::fixed,
                                    decimals);
  const std::string_view text(buffer,
                              static_cast<std::size_t>(result.ptr - buffer));
  if (!group) {
    out.append(text);
    return;
  }
  const std::size_t dot = text.find('.');
  Group(text.substr(0, dot), out);
  if (dot != std::string_view::npos) {
    out.append(text.substr(dot));
  }
}

}  // namespace gloomy::data
