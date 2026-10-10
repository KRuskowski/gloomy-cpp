#ifndef GLOOMY_DATA_TABLE_SOURCE_H_
#define GLOOMY_DATA_TABLE_SOURCE_H_

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gloomy/core/color.h"

namespace gloomy::data {

/// Index of a row in its source.
using RowIndex = std::uint32_t;

/// Value type of a column; decides sorting, filtering and formatting.
enum class ColumnType : std::uint8_t {
  kInt64,
  kDouble,
  kText,
  kBool,
  /// Int64 values indexing ColumnInfo::enum_labels.
  kEnum,
};

/// An icon in front of a cell's text (DataGrid draws it from its
/// Context's icons).
struct CellIcon {
  /// An icon::IconId; 0xFFFFFFFF (icon::kNoIcon) draws none.
  std::uint32_t icon = 0xFFFFFFFFU;
  /// Alpha 0: the cell's text colour.
  Color color{0, 0, 0, 0};
};

/// Description of a column.
struct ColumnInfo {
  /// Stable key, used by saved views; never shown.
  std::string name;
  /// Header text.
  std::string title;
  ColumnType type = ColumnType::kText;
  std::vector<std::string> enum_labels;
  /// Optional colour per enum value (badges, chips); alpha 0 or a value
  /// past the end falls back to the palette's rotation.
  std::vector<Color> enum_colors;
  /// Digits after the decimal point for doubles.
  int decimals = 2;
  /// Appended to formatted numbers (" Cr", " m3").
  std::string unit;
  /// Thousands separators for numbers.
  bool group_digits = true;
  /// Included in quick search.
  bool searchable = true;
  /// Cells may have an explanation (docs/design.md section 2.3).
  bool explained = false;

  auto IsNumeric() const -> bool {
    return type == ColumnType::kInt64 || type == ColumnType::kDouble;
  }
};

/// The "why" behind a value.
struct Explanation {
  struct Item {
    std::string label;
    double value = 0.0;
  };

  /// One line, shown as tooltip or inline note.
  std::string reason;
  /// Optional breakdown (price = base + shortage premium + ...).
  std::vector<Item> breakdown;
  /// Optional navigation target, e.g. "station:42".
  std::string link;
};

/// Tabular data the UI reads but never copies. Views over it are
/// permutations of row indices (docs/design.md section 10.1).
///
/// Thread safety: queries may read a source from a worker thread. A source
/// shared with a TableView must not change while shared; publish a new
/// snapshot instead, or use synchronous views.
class TableSource {
 public:
  virtual ~TableSource() = default;

  virtual auto RowCount() const -> RowIndex = 0;

  virtual auto Columns() const -> std::span<const ColumnInfo> = 0;

  /// Changes whenever any cell changes; equal versions mean equal data.
  virtual auto Version() const -> std::uint64_t = 0;

  /// Integer value of an int64, bool or enum cell.
  virtual auto GetInt64(int col, RowIndex row) const -> std::int64_t;

  /// Numeric value of a number cell (ints convert). NaN means blank.
  virtual auto GetDouble(int col, RowIndex row) const -> double;

  /// Text of a text cell. The view stays valid while the source is
  /// unchanged.
  virtual auto GetText(int col, RowIndex row) const -> std::string_view;

  /// Batch reads; override when a single virtual call per batch matters.
  virtual void ReadInt64(int col, std::span<const RowIndex> rows,
                         std::span<std::int64_t> out) const;

  virtual void ReadDouble(int col, std::span<const RowIndex> rows,
                          std::span<double> out) const;

  /// Display text of a cell. The default formats by column type.
  virtual void FormatCell(RowIndex row, int col, std::string &out) const;

  /// A short series for sparkline cells (oldest first); false if none.
  virtual auto GetSeries(int col, RowIndex row, std::vector<float> &out) const
      -> bool {
    (void)col;
    (void)row;
    (void)out;
    return false;
  }

  /// The icon shown in front of a cell (a station's type, a ship's class);
  /// false if there is none.
  virtual auto GetIcon(RowIndex row, int col, CellIcon &out) const -> bool {
    (void)row;
    (void)col;
    (void)out;
    return false;
  }

  /// Stable identity of a row across versions (selection, focus).
  virtual auto RowKey(RowIndex row) const -> std::uint64_t { return row; }

  /// Fills the explanation of a cell; false if there is none.
  virtual auto Explain(RowIndex row, int col, Explanation &out) const -> bool {
    (void)row;
    (void)col;
    (void)out;
    return false;
  }

  /// True if new versions only append rows and never change old ones
  /// (logs); views then extend incrementally.
  virtual auto AppendOnly() const -> bool { return false; }

  /// Index of the column named `name`, -1 if none.
  auto FindColumn(std::string_view name) const -> int;
};

/// Formats an integer with optional thousands separators.
void FormatInt(std::int64_t value, bool group, std::string &out);

/// Formats a double with `decimals` digits and optional separators.
void FormatDouble(double value, int decimals, bool group, std::string &out);

}  // namespace gloomy::data

#endif  // GLOOMY_DATA_TABLE_SOURCE_H_
