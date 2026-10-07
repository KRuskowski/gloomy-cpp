#ifndef GLOOMY_DATA_COLUMNAR_TABLE_H_
#define GLOOMY_DATA_COLUMNAR_TABLE_H_

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "gloomy/data/table_source.h"

namespace gloomy::data {

/// A cell value for ColumnarTable::AddRow().
using CellValue = std::variant<std::int64_t, double, std::string_view, bool>;

/// A table that owns its data column by column: numbers in typed vectors,
/// text in one blob per column. For generated data and logs.
class ColumnarTable : public TableSource {
 public:
  explicit ColumnarTable(std::vector<ColumnInfo> columns);

  /// Appends a row; values convert to each column's type.
  auto AddRow(std::span<const CellValue> values) -> RowIndex;

  auto AddRow(std::initializer_list<CellValue> values) -> RowIndex {
    return AddRow(std::span<const CellValue>(values.begin(), values.size()));
  }

  void SetInt64(int col, RowIndex row, std::int64_t value);

  void SetDouble(int col, RowIndex row, double value);

  void SetText(int col, RowIndex row, std::string_view value);

  /// Reserves room for `rows` rows.
  void Reserve(RowIndex rows);

  /// Marks rows as only ever appended (AppendOnly()).
  void SetAppendOnly(bool append_only) { append_only_ = append_only; }

  // TableSource.
  auto RowCount() const -> RowIndex override { return rows_; }

  auto Columns() const -> std::span<const ColumnInfo> override {
    return infos_;
  }

  auto Version() const -> std::uint64_t override { return version_; }

  auto GetInt64(int col, RowIndex row) const -> std::int64_t override;

  auto GetDouble(int col, RowIndex row) const -> double override;

  auto GetText(int col, RowIndex row) const -> std::string_view override;

  auto AppendOnly() const -> bool override { return append_only_; }

 private:
  struct TextRef {
    std::uint32_t offset;
    std::uint32_t length;
  };

  struct Column {
    std::vector<std::int64_t> ints;
    std::vector<double> doubles;
    std::string blob;
    std::vector<TextRef> texts;
  };

  void Append(int col, const CellValue &value);

  std::vector<ColumnInfo> infos_;
  std::vector<Column> columns_;
  RowIndex rows_ = 0;
  std::uint64_t version_ = 1;
  bool append_only_ = false;
};

/// A zero-copy adapter over data the app already keeps (structure of
/// arrays such as X-conomy's World tables): each column is a function of
/// the row index.
class FunctionTable : public TableSource {
 public:
  /// @param[in] row_count Current number of rows.
  /// @param[in] version Current data version (e.g. the simulation tick).
  FunctionTable(std::function<RowIndex()> row_count,
                std::function<std::uint64_t()> version);

  /// Adds an int64, bool or enum column.
  auto AddInt64(ColumnInfo info, std::function<std::int64_t(RowIndex)> get)
      -> int;

  auto AddDouble(ColumnInfo info, std::function<double(RowIndex)> get) -> int;

  /// The returned views must stay valid while the version is unchanged.
  auto AddText(ColumnInfo info, std::function<std::string_view(RowIndex)> get)
      -> int;

  /// Stable row identity; defaults to the row index.
  void SetRowKey(std::function<std::uint64_t(RowIndex)> key) {
    key_ = std::move(key);
  }

  /// Explanations for cells.
  void SetExplain(std::function<bool(RowIndex, int, Explanation &)> explain) {
    explain_ = std::move(explain);
  }

  // TableSource.
  auto RowCount() const -> RowIndex override { return row_count_(); }

  auto Columns() const -> std::span<const ColumnInfo> override {
    return infos_;
  }

  auto Version() const -> std::uint64_t override { return version_(); }

  auto GetInt64(int col, RowIndex row) const -> std::int64_t override;

  auto GetDouble(int col, RowIndex row) const -> double override;

  auto GetText(int col, RowIndex row) const -> std::string_view override;

  auto RowKey(RowIndex row) const -> std::uint64_t override {
    return key_ ? key_(row) : row;
  }

  auto Explain(RowIndex row, int col, Explanation &out) const -> bool override {
    return explain_ && explain_(row, col, out);
  }

 private:
  struct Column {
    std::function<std::int64_t(RowIndex)> ints;
    std::function<double(RowIndex)> doubles;
    std::function<std::string_view(RowIndex)> texts;
  };

  std::function<RowIndex()> row_count_;
  std::function<std::uint64_t()> version_;
  std::function<std::uint64_t(RowIndex)> key_;
  std::function<bool(RowIndex, int, Explanation &)> explain_;
  std::vector<ColumnInfo> infos_;
  std::vector<Column> columns_;
};

}  // namespace gloomy::data

#endif  // GLOOMY_DATA_COLUMNAR_TABLE_H_
