#include "gloomy/data/columnar_table.h"

#include <cmath>
#include <limits>

namespace gloomy::data {

namespace {

auto ToInt(const CellValue &v) -> std::int64_t {
  if (const auto *i = std::get_if<std::int64_t>(&v)) {
    return *i;
  }
  if (const auto *d = std::get_if<double>(&v)) {
    return static_cast<std::int64_t>(*d);
  }
  if (const auto *b = std::get_if<bool>(&v)) {
    return *b ? 1 : 0;
  }
  return 0;
}

auto ToDouble(const CellValue &v) -> double {
  if (const auto *d = std::get_if<double>(&v)) {
    return *d;
  }
  if (const auto *i = std::get_if<std::int64_t>(&v)) {
    return static_cast<double>(*i);
  }
  if (const auto *b = std::get_if<bool>(&v)) {
    return *b ? 1.0 : 0.0;
  }
  return std::numeric_limits<double>::quiet_NaN();
}

}  // namespace

ColumnarTable::ColumnarTable(std::vector<ColumnInfo> columns)
    : infos_(std::move(columns)), columns_(infos_.size()) {}

void ColumnarTable::Reserve(RowIndex rows) {
  for (std::size_t c = 0; c < columns_.size(); ++c) {
    switch (infos_[c].type) {
      case ColumnType::kDouble:
        columns_[c].doubles.reserve(rows);
        break;
      case ColumnType::kText:
        columns_[c].texts.reserve(rows);
        break;
      default:
        columns_[c].ints.reserve(rows);
        break;
    }
  }
}

void ColumnarTable::Append(int col, const CellValue &value) {
  Column &c = columns_[col];
  switch (infos_[col].type) {
    case ColumnType::kDouble:
      c.doubles.push_back(ToDouble(value));
      break;
    case ColumnType::kText: {
      std::string_view text;
      if (const auto *s = std::get_if<std::string_view>(&value)) {
        text = *s;
      }
      c.texts.push_back({static_cast<std::uint32_t>(c.blob.size()),
                         static_cast<std::uint32_t>(text.size())});
      c.blob.append(text);
      break;
    }
    default:
      c.ints.push_back(ToInt(value));
      break;
  }
}

auto ColumnarTable::AddRow(std::span<const CellValue> values) -> RowIndex {
  for (std::size_t c = 0; c < columns_.size(); ++c) {
    Append(static_cast<int>(c),
           c < values.size() ? values[c] : CellValue{std::int64_t{0}});
  }
  ++version_;
  return rows_++;
}

void ColumnarTable::SetInt64(int col, RowIndex row, std::int64_t value) {
  columns_[col].ints[row] = value;
  ++version_;
}

void ColumnarTable::SetDouble(int col, RowIndex row, double value) {
  columns_[col].doubles[row] = value;
  ++version_;
}

void ColumnarTable::SetText(int col, RowIndex row, std::string_view value) {
  Column &c = columns_[col];
  c.texts[row] = {static_cast<std::uint32_t>(c.blob.size()),
                  static_cast<std::uint32_t>(value.size())};
  c.blob.append(value);
  ++version_;
}

auto ColumnarTable::GetInt64(int col, RowIndex row) const -> std::int64_t {
  const Column &c = columns_[col];
  if (infos_[col].type == ColumnType::kDouble) {
    return static_cast<std::int64_t>(c.doubles[row]);
  }
  if (infos_[col].type == ColumnType::kText) {
    return 0;
  }
  return c.ints[row];
}

auto ColumnarTable::GetDouble(int col, RowIndex row) const -> double {
  const Column &c = columns_[col];
  if (infos_[col].type == ColumnType::kDouble) {
    return c.doubles[row];
  }
  if (infos_[col].type == ColumnType::kText) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  return static_cast<double>(c.ints[row]);
}

auto ColumnarTable::GetText(int col, RowIndex row) const -> std::string_view {
  const Column &c = columns_[col];
  if (infos_[col].type != ColumnType::kText) {
    return {};
  }
  const TextRef ref = c.texts[row];
  return std::string_view(c.blob).substr(ref.offset, ref.length);
}

// FunctionTable

FunctionTable::FunctionTable(std::function<RowIndex()> row_count,
                             std::function<std::uint64_t()> version)
    : row_count_(std::move(row_count)), version_(std::move(version)) {}

auto FunctionTable::AddInt64(ColumnInfo info,
                             std::function<std::int64_t(RowIndex)> get) -> int {
  if (info.type == ColumnType::kDouble || info.type == ColumnType::kText) {
    info.type = ColumnType::kInt64;
  }
  infos_.push_back(std::move(info));
  columns_.push_back({std::move(get), {}, {}});
  return static_cast<int>(infos_.size() - 1);
}

auto FunctionTable::AddDouble(ColumnInfo info,
                              std::function<double(RowIndex)> get) -> int {
  info.type = ColumnType::kDouble;
  infos_.push_back(std::move(info));
  columns_.push_back({{}, std::move(get), {}});
  return static_cast<int>(infos_.size() - 1);
}

auto FunctionTable::AddText(ColumnInfo info,
                            std::function<std::string_view(RowIndex)> get)
    -> int {
  info.type = ColumnType::kText;
  infos_.push_back(std::move(info));
  columns_.push_back({{}, {}, std::move(get)});
  return static_cast<int>(infos_.size() - 1);
}

auto FunctionTable::GetInt64(int col, RowIndex row) const -> std::int64_t {
  const Column &c = columns_[col];
  if (c.ints) {
    return c.ints(row);
  }
  if (c.doubles) {
    return static_cast<std::int64_t>(c.doubles(row));
  }
  return 0;
}

auto FunctionTable::GetDouble(int col, RowIndex row) const -> double {
  const Column &c = columns_[col];
  if (c.doubles) {
    return c.doubles(row);
  }
  if (c.ints) {
    return static_cast<double>(c.ints(row));
  }
  return std::numeric_limits<double>::quiet_NaN();
}

auto FunctionTable::GetText(int col, RowIndex row) const -> std::string_view {
  const Column &c = columns_[col];
  return c.texts ? c.texts(row) : std::string_view();
}

}  // namespace gloomy::data
