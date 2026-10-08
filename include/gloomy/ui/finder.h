#ifndef GLOOMY_UI_FINDER_H_
#define GLOOMY_UI_FINDER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gloomy/data/document.h"
#include "gloomy/data/search.h"
#include "gloomy/data/table_view.h"
#include "gloomy/ui/context.h"

namespace gloomy::ui {

/// One result of a global search or the command palette.
struct SearchHit {
  std::string label;
  /// Muted second text: where it is, what it belongs to.
  std::string context;
  /// Shown at the right, e.g. a shortcut.
  std::string detail;
  /// Byte ranges of `label` to highlight.
  std::vector<data::MatchRange> highlights;
  /// What activating the hit does.
  std::function<void()> go;
};

/// A source of search results: a table, a document, the commands, or
/// anything the app adds (docs/design.md section 10.4).
class SearchProvider {
 public:
  virtual ~SearchProvider() = default;

  /// Group title in the results.
  virtual auto name() const -> std::string_view = 0;

  /// Starts searching for `query` (may run in the background).
  virtual void SetQuery(std::string_view query) = 0;

  /// Appends up to `limit` current results.
  virtual void Collect(int limit, std::vector<SearchHit> &out) = 0;

  /// Total results (may exceed what Collect() returns).
  virtual auto count() const -> std::size_t = 0;

  /// True while results are still being computed.
  virtual auto busy() const -> bool { return false; }

  /// Called once per frame before Collect().
  virtual void Update(double time) { (void)time; }
};

/// A command for the palette.
struct Command {
  std::string title;
  /// Display only, e.g. "Ctrl+F".
  std::string shortcut;
  std::function<void()> run;
};

/// Commands, matched fuzzily and ranked by score.
class CommandProvider : public SearchProvider {
 public:
  void Add(Command command) { commands_.push_back(std::move(command)); }

  auto name() const -> std::string_view override { return "Commands"; }

  void SetQuery(std::string_view query) override;

  void Collect(int limit, std::vector<SearchHit> &out) override;

  auto count() const -> std::size_t override { return ranked_.size(); }

  auto commands() const -> const std::vector<Command> & { return commands_; }

 private:
  struct Ranked {
    std::size_t index;
    int score;
    std::vector<std::uint32_t> positions;
  };

  std::vector<Command> commands_;
  std::vector<Ranked> ranked_;
};

/// Rows of a table, searched through their own TableView (so large tables
/// are searched in the background, with narrowing and caching).
class TableProvider : public SearchProvider {
 public:
  /// @param[in] label_column Column shown as the hit's label.
  /// @param[in] context_columns Columns joined into the context text.
  /// @param[in] on_select Called with the source row when a hit is chosen.
  TableProvider(std::string name,
                std::shared_ptr<const data::TableSource> source,
                int label_column, std::vector<int> context_columns,
                std::function<void(data::RowIndex)> on_select);

  auto name() const -> std::string_view override { return name_; }

  void SetQuery(std::string_view query) override;

  void Update(double time) override { view_.Update(time); }

  void Collect(int limit, std::vector<SearchHit> &out) override;

  auto count() const -> std::size_t override;

  auto busy() const -> bool override { return view_.busy(); }

 private:
  std::string name_;
  data::TableView view_;
  int label_column_;
  std::vector<int> context_columns_;
  std::function<void(data::RowIndex)> on_select_;
  bool empty_query_ = true;
};

/// Blocks of a document.
class DocumentProvider : public SearchProvider {
 public:
  DocumentProvider(std::string name, const data::Document &doc,
                   std::function<void(const data::DocMatch &)> on_select);

  auto name() const -> std::string_view override { return name_; }

  void SetQuery(std::string_view query) override;

  void Collect(int limit, std::vector<SearchHit> &out) override;

  auto count() const -> std::size_t override { return matches_.size(); }

 private:
  std::string name_;
  const data::Document &doc_;
  std::function<void(const data::DocMatch &)> on_select_;
  std::vector<data::DocMatch> matches_;
  std::vector<std::string> terms_;
  /// The document's version the matches are for: an app may replace the
  /// document's content between SetQuery() and Collect().
  std::uint64_t matched_version_ = 0;

  void Match();
};

/// State of a Finder; owned by the app.
struct FinderState {
  bool open = false;
  std::string query;
  /// Index into the flat list of shown hits.
  int selected = 0;
  /// Query the providers last saw.
  std::string searched = "\x01";
};

/// The command palette and global search in one: a modal search box with
/// results grouped by provider. Up/Down choose, Enter activates (the hit's
/// go() runs and the finder closes), Escape closes.
/// @return True if a hit was activated.
auto Finder(Context &ctx, std::string_view label,
            std::span<SearchProvider *const> providers, FinderState &state,
            std::string_view placeholder = "Search everything") -> bool;

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_FINDER_H_
