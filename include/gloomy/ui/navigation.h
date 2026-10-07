#ifndef GLOOMY_UI_NAVIGATION_H_
#define GLOOMY_UI_NAVIGATION_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "gloomy/ui/context.h"
#include "gloomy/ui/finder.h"

namespace gloomy::ui {

/// The keyboard and search contract of every view (docs/design.md section
/// 2.5), for views gloomy does not draw itself, such as a game map: items
/// in a stable order with a cursor, labels for type-to-jump and search.
class NavigationTarget {
 public:
  virtual ~NavigationTarget() = default;

  virtual auto ItemCount() const -> std::int64_t = 0;

  /// The cursor item, -1 if none.
  virtual auto Cursor() const -> std::int64_t = 0;

  virtual void SetCursor(std::int64_t item) = 0;

  /// Enter.
  virtual void Activate(std::int64_t item) = 0;

  /// Text of an item, for type-to-jump and search.
  virtual auto ItemLabel(std::int64_t item) const -> std::string = 0;

  /// Muted context for search results (sector, owner, ...).
  virtual auto ItemContext(std::int64_t item) const -> std::string {
    (void)item;
    return {};
  }

  /// Items Page Up/Down move by.
  virtual auto PageSize() const -> std::int64_t { return 10; }
};

/// Type-to-jump state of a navigable view.
struct NavigationState {
  std::string prefix;
  double prefix_time = -10.0;
};

/// The standard keys for `target` while node `id` has focus: arrows,
/// Page Up/Down, Home/End move the cursor, Enter activates, typing jumps
/// to the next item starting with the typed prefix.
/// @return True if the cursor moved or an item was activated.
auto Navigate(Context &ctx, Id id, NavigationTarget &target,
              NavigationState &state) -> bool;

/// Makes a NavigationTarget searchable in the Finder: hits are items whose
/// label contains the search terms; choosing one moves the cursor there.
class NavigationProvider : public SearchProvider {
 public:
  NavigationProvider(std::string name, NavigationTarget &target)
      : name_(std::move(name)), target_(target) {}

  auto name() const -> std::string_view override { return name_; }

  void SetQuery(std::string_view query) override;

  void Collect(int limit, std::vector<SearchHit> &out) override;

  auto count() const -> std::size_t override { return items_.size(); }

 private:
  std::string name_;
  NavigationTarget &target_;
  std::vector<std::string> terms_;
  std::vector<std::int64_t> items_;
};

}  // namespace gloomy::ui

#endif  // GLOOMY_UI_NAVIGATION_H_
