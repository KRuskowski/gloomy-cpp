#include "gloomy/ui/navigation.h"

#include <algorithm>

#include "gloomy/core/utf8.h"
#include "gloomy/data/search.h"

namespace gloomy::ui {

namespace {

constexpr double kPrefixTimeout = 1.0;

auto Folded(std::string_view s) -> std::string {
  std::string out;
  FoldCaseUtf8(s, out);
  return out;
}

}  // namespace

auto Navigate(Context &ctx, Id id, NavigationTarget &target,
              NavigationState &state) -> bool {
  if (!ctx.HasFocus(id)) {
    return false;
  }
  const std::int64_t n = target.ItemCount();
  if (n == 0) {
    return false;
  }
  const std::int64_t cur = target.Cursor();
  std::int64_t next = cur;
  if (ctx.ConsumeKey(Key::kDown) || ctx.ConsumeKey(Key::kRight)) {
    next = cur + 1;
  }
  if (ctx.ConsumeKey(Key::kUp) || ctx.ConsumeKey(Key::kLeft)) {
    next = cur - 1;
  }
  if (ctx.ConsumeKey(Key::kPageDown)) {
    next = cur + target.PageSize();
  }
  if (ctx.ConsumeKey(Key::kPageUp)) {
    next = cur - target.PageSize();
  }
  if (ctx.ConsumeKey(Key::kHome)) {
    next = 0;
  }
  if (ctx.ConsumeKey(Key::kEnd)) {
    next = n - 1;
  }
  bool acted = false;
  const std::string typed = ctx.TakeText();
  if (!typed.empty() && typed != " ") {
    if (ctx.time() - state.prefix_time > kPrefixTimeout) {
      state.prefix.clear();
    }
    const std::string folded = Folded(typed);
    const bool repeat = state.prefix.empty() || state.prefix == folded;
    state.prefix = repeat ? folded : state.prefix + folded;
    state.prefix_time = ctx.time();
    const std::int64_t start =
        std::max<std::int64_t>(0, cur + (repeat ? 1 : 0));
    for (std::int64_t k = 0; k < n; ++k) {
      const std::int64_t i = (start + k) % n;
      if (Folded(target.ItemLabel(i)).starts_with(state.prefix)) {
        next = i;
        break;
      }
    }
  }
  // Without a key that moved it, a missing cursor stays missing.
  if (next != cur) {
    next = std::clamp<std::int64_t>(next, 0, n - 1);
  }
  if (next != cur) {
    target.SetCursor(next);
    acted = true;
  }
  if (ctx.ConsumeKey(Key::kEnter) && target.Cursor() >= 0) {
    target.Activate(target.Cursor());
    acted = true;
  }
  return acted;
}

void NavigationProvider::SetQuery(std::string_view query) {
  terms_ = data::ParseTerms(query);
  items_.clear();
  if (terms_.empty()) {
    return;
  }
  const std::int64_t n = target_.ItemCount();
  for (std::int64_t i = 0; i < n; ++i) {
    const std::string label = Folded(target_.ItemLabel(i));
    const bool all =
        std::all_of(terms_.begin(), terms_.end(), [&](const std::string &t) {
          return label.find(t) != std::string::npos;
        });
    if (all) {
      items_.push_back(i);
    }
  }
}

void NavigationProvider::Collect(int limit, std::vector<SearchHit> &out) {
  for (std::size_t k = 0; k < items_.size() && static_cast<int>(k) < limit;
       ++k) {
    const std::int64_t item = items_[k];
    SearchHit hit;
    hit.label = target_.ItemLabel(item);
    hit.context = target_.ItemContext(item);
    data::FindMatches(hit.label, terms_, hit.highlights);
    hit.go = [&target = target_, item] {
      target.SetCursor(item);
    };
    out.push_back(std::move(hit));
  }
}

}  // namespace gloomy::ui
