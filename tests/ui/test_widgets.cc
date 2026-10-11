#include "gloomy/ui/widgets.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "test_support/ui_harness.h"

namespace gloomy::ui {
namespace {

using testing::UiHarness;

TEST(WidgetsTest, ButtonClickResolvesAgainstPreviousLayout) {
  UiHarness h;
  bool clicked = false;
  auto ui = [&](Context &ctx) {
    clicked = Button(ctx, "Undock");
  };
  h.Frame(ui);
  EXPECT_FALSE(clicked);
  const Vec2 c = h.Center(h.ctx().GetId("Undock"));
  h.Frame(ui, UiHarness::Click(c));
  EXPECT_TRUE(clicked);
  h.Frame(ui);
  EXPECT_FALSE(clicked);
}

TEST(WidgetsTest, KeysStayHeldUntilReleased) {
  UiHarness h;
  auto ui = [](Context &) {
  };
  h.Frame(ui);
  EXPECT_FALSE(h.ctx().IsKeyDown(Key::kW));
  h.Frame(ui, {InputEvent::KeyDown(Key::kW)});
  EXPECT_TRUE(h.ctx().IsKeyDown(Key::kW));
  // Held through frames without events, and still a single press.
  h.Frame(ui);
  EXPECT_TRUE(h.ctx().IsKeyDown(Key::kW));
  EXPECT_FALSE(h.ctx().ConsumeKey(Key::kW));
  InputEvent up = InputEvent::KeyDown(Key::kW);
  up.type = InputEvent::Type::kKeyUp;
  h.Frame(ui, {InputEvent::KeyDown(Key::kD), up});
  EXPECT_FALSE(h.ctx().IsKeyDown(Key::kW));
  EXPECT_TRUE(h.ctx().IsKeyDown(Key::kD));
  h.ctx().ReleaseKeys();
  EXPECT_FALSE(h.ctx().IsKeyDown(Key::kD));
}

TEST(WidgetsTest, ModifiersAloneAreHeldNotPressed) {
  UiHarness h;
  auto ui = [](Context &) {
  };
  // Shift pressed on its own: held, but no key press to consume.
  h.Frame(ui, {InputEvent::KeyDown(Key::kNone, kModShift)});
  EXPECT_TRUE(h.ctx().IsModDown(kModShift));
  EXPECT_FALSE(h.ctx().IsModDown(kModCtrl));
  EXPECT_FALSE(h.ctx().IsKeyDown(Key::kNone));
  std::uint8_t mods = 0;
  EXPECT_FALSE(h.ctx().ConsumeKeyAnyMods(Key::kNone, mods));
  // Shift+W, then Shift let go while W stays down.
  h.Frame(ui, {InputEvent::KeyDown(Key::kW, kModShift)});
  EXPECT_TRUE(h.ctx().IsKeyDown(Key::kW));
  EXPECT_TRUE(h.ctx().IsModDown(kModShift));
  InputEvent up = InputEvent::KeyDown(Key::kNone);
  up.type = InputEvent::Type::kKeyUp;
  h.Frame(ui, {up});
  EXPECT_TRUE(h.ctx().IsKeyDown(Key::kW));
  EXPECT_FALSE(h.ctx().IsModDown(kModShift));
  h.Frame(ui, {InputEvent::KeyDown(Key::kNone, kModShift)});
  h.ctx().ReleaseKeys();
  EXPECT_FALSE(h.ctx().IsModDown(kModShift));
}

TEST(WidgetsTest, PressThenReleaseElsewhereDoesNotClick) {
  UiHarness h;
  bool clicked = false;
  auto ui = [&](Context &ctx) {
    clicked = Button(ctx, "OK");
  };
  h.Frame(ui);
  const Vec2 c = h.Center(h.ctx().GetId("OK"));
  h.Frame(ui,
          {InputEvent::Move(c.x, c.y),
           InputEvent::Down(c.x, c.y),
           InputEvent::Move(400, 400),
           InputEvent::Up(400, 400)});
  EXPECT_FALSE(clicked);
}

TEST(WidgetsTest, TabMovesFocusAndEnterActivates) {
  UiHarness h;
  bool a = false;
  bool b = false;
  auto ui = [&](Context &ctx) {
    a = Button(ctx, "A");
    b = Button(ctx, "B");
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Key(Key::kTab));
  EXPECT_EQ(h.ctx().focused(), h.ctx().GetId("A"));
  h.Frame(ui, UiHarness::Key(Key::kTab));
  EXPECT_EQ(h.ctx().focused(), h.ctx().GetId("B"));
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(b);
  EXPECT_FALSE(a);
  // Wraps around, and Shift+Tab goes back.
  h.Frame(ui, UiHarness::Key(Key::kTab));
  EXPECT_EQ(h.ctx().focused(), h.ctx().GetId("A"));
  h.Frame(ui, UiHarness::Key(Key::kTab, kModShift));
  EXPECT_EQ(h.ctx().focused(), h.ctx().GetId("B"));
  EXPECT_TRUE(h.ctx().focus_visible());
}

TEST(WidgetsTest, EscapeDropsFocus) {
  UiHarness h;
  auto ui = [&](Context &ctx) {
    Button(ctx, "A");
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Key(Key::kTab));
  ASSERT_NE(h.ctx().focused(), kNoId);
  h.Frame(ui, UiHarness::Key(Key::kEscape));
  EXPECT_EQ(h.ctx().focused(), kNoId);
}

TEST(WidgetsTest, CheckboxTogglesByClickAndSpace) {
  UiHarness h;
  bool value = false;
  auto ui = [&](Context &ctx) {
    Checkbox(ctx, "Show gates", value);
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(h.ctx().GetId("Show gates"))));
  EXPECT_TRUE(value);
  // The click focused it, so Space toggles.
  h.Frame(ui, UiHarness::Key(Key::kSpace));
  EXPECT_FALSE(value);
  // Space does nothing without focus.
  h.ctx().SetFocus(kNoId);
  h.Frame(ui, UiHarness::Key(Key::kSpace));
  EXPECT_FALSE(value);
}

TEST(WidgetsTest, RadioButtonsSelectOne) {
  UiHarness h;
  int value = 0;
  auto ui = [&](Context &ctx) {
    RadioButton(ctx, "Buy", value, 0);
    RadioButton(ctx, "Sell", value, 1);
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(h.ctx().GetId("Sell"))));
  EXPECT_EQ(value, 1);
}

TEST(WidgetsTest, SliderFollowsDragAndKeys) {
  UiHarness h;
  float value = 0.0F;
  auto ui = [&](Context &ctx) {
    Slider(ctx, "##volume", value, 0, 100);
  };
  h.Frame(ui);
  const Rect r = h.ctx().LastRect(h.ctx().GetId("##volume")).value();
  h.Frame(ui,
          {InputEvent::Move(r.x1 - 1, r.Center().y),
           InputEvent::Down(r.x1 - 1, r.Center().y)});
  EXPECT_FLOAT_EQ(value, 100.0F);
  h.Frame(ui, {InputEvent::Move(r.x0, r.Center().y)});
  EXPECT_FLOAT_EQ(value, 0.0F);
  h.Frame(ui, {InputEvent::Up(r.x0, r.Center().y)});
  h.Frame(ui, UiHarness::Key(Key::kRight));
  EXPECT_FLOAT_EQ(value, 1.0F);
  h.Frame(ui, UiHarness::Key(Key::kEnd));
  EXPECT_FLOAT_EQ(value, 100.0F);
}

TEST(WidgetsTest, TextFieldEditing) {
  UiHarness h;
  std::string text;
  TextFieldResult result;
  auto ui = [&](Context &ctx) {
    result = TextField(ctx, "##name", text);
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(h.ctx().GetId("##name"))));
  ASSERT_EQ(h.ctx().focused(), h.ctx().GetId("##name"));
  h.Frame(ui, {InputEvent::Text("Ore Mine"), InputEvent::Text(" 4")});
  EXPECT_EQ(text, "Ore Mine 4");
  EXPECT_TRUE(result.changed);
  h.Frame(ui, UiHarness::Key(Key::kBackspace, kModCtrl));
  EXPECT_EQ(text, "Ore Mine ");
  h.Frame(ui, UiHarness::Key(Key::kHome));
  h.Frame(ui, UiHarness::Key(Key::kRight, kModCtrl | kModShift));
  h.Frame(ui, UiHarness::Key(Key::kC, kModCtrl));
  EXPECT_EQ(h.ctx().clipboard().get(), "Ore");
  h.Frame(ui, UiHarness::Key(Key::kEnd));
  h.Frame(ui, UiHarness::Key(Key::kV, kModCtrl));
  EXPECT_EQ(text, "Ore Mine Ore");
  h.Frame(ui, UiHarness::Key(Key::kA, kModCtrl));
  h.Frame(ui, {InputEvent::Text("\xC3\x9F")});
  EXPECT_EQ(text, "\xC3\x9F");
  h.Frame(ui, UiHarness::Key(Key::kLeft));
  h.Frame(ui, UiHarness::Key(Key::kDelete));
  EXPECT_EQ(text, "");
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(result.submitted);
}

TEST(WidgetsTest, DropdownByMouse) {
  UiHarness h;
  int selected = 0;
  const std::vector<std::string> items{"Ore", "Silicon", "Food"};
  bool changed = false;
  auto ui = [&](Context &ctx) {
    changed = Dropdown(ctx, "##good", selected, items);
  };
  h.Frame(ui);
  const Id id = h.ctx().GetId("##good");
  h.Frame(ui, UiHarness::Click(h.Center(id)));
  h.Frame(ui);
  // Item rows live under the popup's id scope; find row 2 by position.
  const Rect button = h.ctx().LastRect(id).value();
  const float row_h = h.ctx().theme().metrics.row_height;
  const Vec2 item2{button.Center().x, button.y1 + 4 + row_h * 2.5F};
  h.Frame(ui, UiHarness::Click(item2));
  EXPECT_TRUE(changed);
  EXPECT_EQ(selected, 2);
  h.Frame(ui);
  // The popup is closed: clicking there again does nothing.
  h.Frame(ui, UiHarness::Click(item2));
  EXPECT_EQ(selected, 2);
}

TEST(WidgetsTest, DropdownClosesOnOutsideClickWithoutReopening) {
  UiHarness h;
  int selected = 0;
  const std::vector<std::string> items{"Ore", "Silicon"};
  auto ui = [&](Context &ctx) {
    Dropdown(ctx, "##good", selected, items);
  };
  h.Frame(ui);
  const Id id = h.ctx().GetId("##good");
  const Vec2 c = h.Center(id);
  h.Frame(ui, UiHarness::Click(c));
  h.Frame(ui);
  ASSERT_EQ(h.ctx().draw_lists().size(), 2U) << "popup layer open";
  // Clicking the button again closes it (and must not reopen).
  h.Frame(ui, UiHarness::Click(c));
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U);
  // Open again, click somewhere else.
  h.Frame(ui, UiHarness::Click(c));
  h.Frame(ui, UiHarness::Click({600, 460}));
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U);
}

TEST(WidgetsTest, DropdownByKeyboard) {
  UiHarness h;
  int selected = 0;
  const std::vector<std::string> items{"Ore", "Silicon", "Food"};
  auto ui = [&](Context &ctx) {
    Dropdown(ctx, "##good", selected, items);
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Key(Key::kTab));
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  h.Frame(ui, UiHarness::Key(Key::kDown));
  h.Frame(ui, UiHarness::Key(Key::kDown));
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_EQ(selected, 2);
  // Escape closes without changing.
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  h.Frame(ui, UiHarness::Key(Key::kUp));
  h.Frame(ui, UiHarness::Key(Key::kEscape));
  h.Frame(ui);
  EXPECT_EQ(selected, 2);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U);
}

TEST(WidgetsTest, TabsSwitchByClickAndArrows) {
  UiHarness h;
  int tab = 0;
  const std::vector<std::string> labels{"Stations", "Ships", "Prices"};
  auto ui = [&](Context &ctx) {
    Tabs(ctx, "##objects", tab, labels);
  };
  h.Frame(ui);
  h.ctx().PushId("##objects");
  const Id ships = h.ctx().GetId(std::uint64_t{1});
  h.ctx().PopId();
  // Ids under PushId are only valid inside a frame; compute in-frame.
  Id ships_id = kNoId;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    ctx.PushId(ctx.GetId("##objects"));
    ships_id = ctx.GetId(std::uint64_t{1});
    ctx.PopId();
  });
  (void)ships;
  h.Frame(ui, UiHarness::Click(h.Center(ships_id)));
  EXPECT_EQ(tab, 1);
  h.Frame(ui, UiHarness::Key(Key::kRight));
  EXPECT_EQ(tab, 2);
  h.Frame(ui, UiHarness::Key(Key::kRight));
  EXPECT_EQ(tab, 0);
}

TEST(WidgetsTest, ListIsVirtualAndKeyboardDriven) {
  UiHarness h;
  ListState state;
  std::vector<std::string> names;
  for (int i = 0; i < 1000000; ++i) {
    names.push_back("Station " + std::to_string(i));
  }
  names[500] = "Zeta Prime";
  ListResult result;
  auto ui = [&](Context &ctx) {
    LayoutSpec layout;
    layout.height = SizeSpec::Fixed(220);
    result = List(
        ctx,
        "##stations",
        state,
        static_cast<int>(names.size()),
        [&](int i) -> std::string_view { return names[i]; },
        layout);
  };
  h.Frame(ui);
  h.Frame(ui);
  EXPECT_LT(h.ctx().node_count(), 60) << "only visible rows are nodes";
  h.Frame(ui, UiHarness::Key(Key::kTab));
  h.Frame(ui, UiHarness::Key(Key::kDown));
  EXPECT_EQ(state.selected, 0);
  h.Frame(ui, UiHarness::Key(Key::kPageDown));
  EXPECT_GT(state.selected, 5);
  h.Frame(ui, UiHarness::Key(Key::kEnd));
  EXPECT_EQ(state.selected, 999999);
  EXPECT_GT(state.scroll, 999990.0 * 22.0 - 220.0);
  h.Frame(ui, {InputEvent::Text("z")});
  EXPECT_EQ(state.selected, 500);
  EXPECT_LE(state.scroll, 500 * 22.0);
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(result.activated);
}

TEST(WidgetsTest, ListClickSelectsAndDoubleClickActivates) {
  UiHarness h;
  ListState state;
  ListResult result;
  auto ui = [&](Context &ctx) {
    LayoutSpec layout;
    layout.height = SizeSpec::Fixed(200);
    result = List(
        ctx,
        "##l",
        state,
        10,
        [](int) -> std::string_view { return "item"; },
        layout);
  };
  h.Frame(ui);
  const Rect r = h.ctx().LastRect(h.ctx().GetId("##l")).value();
  const Vec2 row3{r.Center().x, r.y0 + 1 + 22 * 3 + 11};
  h.Frame(ui, UiHarness::Click(row3));
  EXPECT_EQ(state.selected, 3);
  EXPECT_EQ(h.ctx().focused(), h.ctx().GetId("##l"));
  h.Frame(ui, UiHarness::Click(row3));
  EXPECT_TRUE(result.activated);
}

TEST(WidgetsTest, ScrollAreaScrollsWithWheel) {
  UiHarness h;
  auto ui = [&](Context &ctx) {
    LayoutSpec layout;
    layout.width = SizeSpec::Fixed(200);
    layout.height = SizeSpec::Fixed(100);
    BeginScroll(ctx, "##log", layout);
    for (int i = 0; i < 50; ++i) {
      Label(ctx, "Line " + std::to_string(i));
    }
    EndScroll(ctx);
  };
  h.Frame(ui);
  h.Frame(ui);
  const Vec2 c = h.Center(h.ctx().GetId("##log"));
  h.Frame(ui, {InputEvent::Move(c.x, c.y), InputEvent::Wheel(0, -2)});
  h.Frame(ui);
  double offset = 0.0;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    offset = ScrollOffset(ctx, "##log");
  });
  EXPECT_DOUBLE_EQ(offset, 80.0);
  h.Frame(ui, {InputEvent::Wheel(0, -1000)});
  h.Frame([&](Context &ctx) {
    ui(ctx);
    offset = ScrollOffset(ctx, "##log");
  });
  // Clamped at content height minus viewport.
  EXPECT_GT(offset, 500.0);
  EXPECT_LT(offset, 50 * 20.0);
}

// A list inside a scrolled panel: the wheel over the list scrolls the list
// while it can, then the panel; a list too short to scroll never stops the
// panel scrolling.
TEST(WidgetsTest, WheelPassesToThePanelWhenAListCannotScroll) {
  UiHarness h;
  int items = 3;
  ListState list;
  auto ui = [&](Context &ctx) {
    LayoutSpec layout;
    layout.width = SizeSpec::Fixed(200);
    layout.height = SizeSpec::Fixed(200);
    BeginScroll(ctx, "##panel", layout);
    LayoutSpec list_layout;
    list_layout.height = SizeSpec::Fixed(100);
    List(
        ctx,
        "##list",
        list,
        items,
        [](int) -> std::string_view { return "item"; },
        list_layout);
    for (int i = 0; i < 50; ++i) {
      Label(ctx, "Line " + std::to_string(i));
    }
    EndScroll(ctx);
  };
  auto panel_offset = [&] {
    double offset = 0.0;
    h.Frame([&](Context &ctx) {
      ui(ctx);
      offset = ScrollOffset(ctx, "##panel");
    });
    return offset;
  };
  auto wheel_over_list = [&](float lines) {
    // The list's place in the panel's coordinates, from the last layout.
    h.ctx().PushId("##panel");
    h.ctx().PushId("##content");
    const Vec2 c = h.Center(h.ctx().GetId("##list"));
    h.ctx().PopId();
    h.ctx().PopId();
    h.Frame(ui, {InputEvent::Move(c.x, c.y), InputEvent::Wheel(0, lines)});
  };
  h.Frame(ui);
  h.Frame(ui);
  wheel_over_list(-1);
  EXPECT_GT(panel_offset(), 0.0) << "3 items do not scroll: the panel does";

  // A long list scrolls itself first.
  items = 100;
  list.scroll = 0.0;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    ScrollOffset(ctx, "##panel") = 0.0;
  });
  h.Frame(ui);
  h.Frame(ui);
  wheel_over_list(-1);
  EXPECT_GT(list.scroll, 0.0);
  EXPECT_DOUBLE_EQ(panel_offset(), 0.0);
  // At the list's end, the panel scrolls.
  wheel_over_list(-1000);
  h.Frame(ui);
  EXPECT_GT(list.scroll, 1000.0);
  wheel_over_list(-1);
  EXPECT_GT(panel_offset(), 0.0);
}

TEST(WidgetsTest, ScrollAreaScrollsByDraggingTheBarAndClickingTheTrack) {
  UiHarness h;
  auto ui = [&](Context &ctx) {
    LayoutSpec layout;
    layout.width = SizeSpec::Fixed(200);
    layout.height = SizeSpec::Fixed(100);
    BeginScroll(ctx, "##log", layout);
    for (int i = 0; i < 50; ++i) {
      Label(ctx, "Line " + std::to_string(i));
    }
    EndScroll(ctx);
  };
  auto offset = [&] {
    double o = 0.0;
    h.Frame([&](Context &ctx) {
      ui(ctx);
      o = ScrollOffset(ctx, "##log");
    });
    return o;
  };
  h.Frame(ui);
  h.Frame(ui);
  const Rect area = h.ctx().LastRect(h.ctx().GetId("##log")).value();
  const float x = area.x1 - 0.5F * h.ctx().theme().metrics.scrollbar_width;
  // Drag the thumb (at the top) down by 30 pixels.
  h.Frame(ui,
          {InputEvent::Move(x, area.y0 + 4), InputEvent::Down(x, area.y0 + 4)});
  h.Frame(ui, {InputEvent::Move(x, area.y0 + 20)});
  h.Frame(ui, {InputEvent::Move(x, area.y0 + 34)});
  h.Frame(ui, {InputEvent::Up(x, area.y0 + 34)});
  const double dragged = offset();
  EXPECT_GT(dragged, 100.0);
  // A click on the track below the thumb pages down.
  h.Frame(ui, UiHarness::Click({x, area.y1 - 3}));
  EXPECT_GT(offset(), dragged + 50.0);
}

TEST(WidgetsTest, ModalBlocksInputBelow) {
  UiHarness h;
  bool base = false;
  bool ok = false;
  auto ui = [&](Context &ctx) {
    base = Button(ctx, "Base");
    ctx.BeginLayer(LayerKind::kModal, ctx.GetId("##confirm"));
    BeginPanel(ctx);
    Label(ctx, "Sell all cargo?");
    ok = Button(ctx, "Sell");
    End(ctx);
    ctx.EndLayer();
  };
  h.Frame(ui);
  h.Frame(ui, UiHarness::Click(h.Center(h.ctx().GetId("Base"))));
  EXPECT_FALSE(base);
  h.Frame(ui, UiHarness::Click(h.Center(h.ctx().GetId("Sell"))));
  EXPECT_TRUE(ok);
  // Tab stays inside the modal.
  h.Frame(ui, UiHarness::Key(Key::kTab));
  EXPECT_EQ(h.ctx().focused(), h.ctx().GetId("Sell"));
  h.Frame(ui, UiHarness::Key(Key::kTab));
  EXPECT_EQ(h.ctx().focused(), h.ctx().GetId("Sell"));
}

TEST(WidgetsTest, TooltipAppearsAfterDelay) {
  UiHarness h;
  auto ui = [&](Context &ctx) {
    Button(ctx, "Info");
    Tooltip(ctx, ctx.GetId("Info"), "Price rose: ore shortage at Mine 4");
  };
  h.Frame(ui);
  const Vec2 c = h.Center(h.ctx().GetId("Info"));
  h.Frame(ui, {InputEvent::Move(c.x, c.y)});
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 1U);
  h.AdvanceTime(0.6);
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 2U);
}

TEST(WidgetsTest, DisabledButtonNeverActivatesAndExplainsWhy) {
  UiHarness h;
  auto ui = [&](Context &ctx) {
    DisabledButton(ctx, "Pay 23,773 Cr", "You have 7,960 Cr");
  };
  h.Frame(ui);
  const Id id = h.ctx().GetId("Pay 23,773 Cr");
  const Vec2 c = h.Center(id);
  h.Frame(ui, UiHarness::Click(c));
  // Not focusable, so Enter cannot press it either.
  EXPECT_NE(h.ctx().focused(), id);
  h.Frame(ui, {InputEvent::Move(c.x, c.y)});
  h.AdvanceTime(0.6);
  h.Frame(ui);
  EXPECT_EQ(h.ctx().draw_lists().size(), 2U);
}

TEST(WidgetsTest, SearchFieldStepsAndClears) {
  UiHarness h;
  std::string query;
  SearchFieldResult r;
  auto ui = [&](Context &ctx) {
    SearchFieldOptions opt;
    opt.status = "3 of 17";
    r = SearchField(ctx, "##find", query, opt);
  };
  h.Frame(ui);
  h.ctx().SetFocus(SearchFieldId(h.ctx(), "##find"));
  h.Frame(ui, {InputEvent::Text("ore")});
  EXPECT_TRUE(r.changed);
  EXPECT_EQ(query, "ore");
  h.Frame(ui, UiHarness::Key(Key::kEnter));
  EXPECT_TRUE(r.next);
  h.Frame(ui, UiHarness::Key(Key::kF3, kModShift));
  EXPECT_TRUE(r.previous);
  h.Frame(ui, UiHarness::Key(Key::kEscape));
  EXPECT_TRUE(r.cleared);
  EXPECT_TRUE(query.empty());
  EXPECT_TRUE(h.ctx().HasFocus(SearchFieldId(h.ctx(), "##find")));
  // A second Escape leaves the field.
  h.Frame(ui, UiHarness::Key(Key::kEscape));
  EXPECT_EQ(h.ctx().focused(), kNoId);
}

TEST(WidgetsTest, SearchFieldClearButton) {
  UiHarness h;
  std::string query = "silicon";
  SearchFieldResult r;
  auto ui = [&](Context &ctx) {
    r = SearchField(ctx, "##find", query);
  };
  h.Frame(ui);
  Id clear = kNoId;
  h.Frame([&](Context &ctx) {
    ui(ctx);
    ctx.PushId("##find");
    clear = ctx.GetId("##clear");
    ctx.PopId();
  });
  h.Frame(ui, UiHarness::Click(h.Center(clear)));
  EXPECT_TRUE(r.cleared);
  EXPECT_TRUE(query.empty());
}

TEST(WidgetsTest, StateIsDroppedWhenUnused) {
  UiHarness h;
  std::string text = "x";
  auto with = [&](Context &ctx) {
    TextField(ctx, "##t", text);
  };
  auto without = [&](Context &) {
  };
  h.Frame(with);
  for (int i = 0; i < 200; ++i) {
    h.Frame(without);
  }
  // No crash and the field works again from scratch.
  h.Frame(with);
  h.Frame(with);
  SUCCEED();
}

}  // namespace
}  // namespace gloomy::ui
