# gloomy-cpp design

gloomy-cpp is a GPU-accelerated, keyboard-first UI library in C++23 on raw Vulkan, built for data-heavy applications. Its first consumer is the X-conomy game client, which shows tables of thousands to millions of rows (markets, ships, stations, orders, contracts, logs) and long documents (contracts), all of which must be searchable. This document is the plan; where the code differs, the code is updated or this document is.

The Rust gloomy (`../gloomy`) is reference material for the low level: SDF primitives, the layout ideas, the semantic theme palette, headless snapshot testing. It is not the specification for the widget layer. gloomy's DataGrid is a stateless widget over a `DataSource` that sorts itself in place; its search, filters, selection and keyboard handling are left to the app. gloomy-cpp turns that inside out: the data layer (sources, queries, search) is the core of the library and the widgets are views over it.

## 1. Goals and non-goals

Goals, in priority order:

1. Tables of 1,000,000 rows that scroll, sort, filter and search at 60 fps, with every operation that touches all rows off the frame path.
2. Search everywhere: per table, per document, and globally across all views, with match highlighting, using one search core.
3. Keyboard-first: everything reachable without a mouse; an X3-style list control (arrows, Enter, type to jump) is the baseline for every list and grid.
4. A renderer that draws into a host engine's Vulkan frame, so X-conomy's future engine can composite the UI over its own scene.
5. Deterministic headless rendering for tests (lavapipe, reference PNGs).

Non-goals for now: right-to-left and bidirectional text (the text pipeline leaves room for it, section 7), IME composition beyond committed text, accessibility APIs, a visual designer, declarative UI files (gloomy's RON). Charts are limited to sparklines and bars inside cells until X-conomy needs more.

## 2. Guiding principle: deep data, accessible surface

The operator's principle for the whole library: a deep spreadsheet under the entire game, but as accessible as possible; the game must not become Rule the Waves. Power users get every column, filter, grouping and export; a newcomer sees an answer, a reason and one obvious next step. These are first-class concepts of the library, built into the data layer and the widgets, not features bolted onto a grid.

### 2.1 Answer-first views

Every view leads with the answer to the player's question in one line, with its reason, and offers exactly one action to drill down into the full table behind it. Example: "Best trade: Ore, Mine 4 to Smelter 2, +12,400 Cr per trip, because ore sells 30% under average at Mine 4", with "Show all trades" opening the trades table.

The rule that keeps answers honest: the answer is row one of the drill-down table. An `Answer` is a `ViewPreset` (2.4) over a source plus a headline formatter; the headline is computed from the first rows of that preset's query, and drilling down opens the same preset as a full grid with the answer row focused. No separate code path computes the headline, so it can never disagree with the table.

### 2.2 Progressive disclosure: glance, panel, table

Every data binding can be shown at three levels, all over the same `TableSource`, the same query and the same search string:

- Glance: a headline value, a delta, a sparkline and a status colour (`SummaryCard`).
- Panel: the key numbers, a compact grid of the preset's panel columns and the top rows (`SummaryPanel`).
- Table: the full `DataGrid` with all columns, filters, grouping, aggregates and export.

Enter (or a click) on a glance opens the panel, Enter on a panel opens the table, Backspace or Esc goes back up, and the search string and focused row carry over in both directions. A `DataBinding` (source, preset, search, focused `RowKey`) is the shared state; the three widgets are views of it.

### 2.3 Explanations attached to data

Any value can carry a short "why": a price with its cause, a profit with its breakdown, a ship's status with its reason. Explanations are part of the data layer:

- `TableSource::Explain(row, col, Explanation &out)` fills a one-line reason, optional breakdown items (label, value, sign) and an optional link target; `ColumnInfo::explained` says whether a column has explanations, so columns without them cost nothing per cell.
- `ExplainedValue` carries a scalar value plus its explanation for cards, labels and headlines.
- The grid marks explained cells with a small corner tick; the explanation shows as a tooltip on hover or focus, inline under the value in panels, and pinned in a side note with F1 or `?`. Explanations are searchable like any other text.

### 2.4 Saved and shareable views

A `ViewPreset` is a plain value: id, name, description, source id, the `Query` (sort, filters, search, grouping, aggregates), the column layout (order, widths, visibility, pinning, renderers), the panel columns and the headline formatter id. The app ships curated presets as defaults; players save their own, duplicate and edit curated ones, and share a preset as a short text string (copy, paste). Unknown columns in a shared preset are ignored, so presets survive game updates. Newcomers only ever see curated presets; power users keep their spreadsheets.

### 2.5 One keyboard and search contract everywhere

Tables, lists, documents, charts and the map behave the same: arrows move a cursor over items, Enter activates or drills down, Backspace or Esc goes back, `/` or Ctrl+F searches within the view, F3 and Shift+F3 move between matches, typing jumps by prefix, Ctrl+C copies the current item or selection, Ctrl+K searches globally. Every view implements one `NavigationTarget` interface (item count, cursor, activate, drill down, bounding box of the cursor for scrolling) and one `SearchProvider`. The map is drawn by X-conomy, not by gloomy, but implements the same two interfaces (objects in spatial order, searchable by name), so the same keys work there.

## 3. What maps from gloomy, what changes

| gloomy (Rust) | gloomy-cpp | Why |
|---|---|---|
| wgpu, winit | Raw Vulkan 1.3, SDL2 window and input in a separate platform module | Operator decision; host-engine embedding |
| `Widget` enum tree, rebuilt by the app, rendered by walking it | Immediate-mode declaration into a per-frame node tree, stable ids, persistent state keyed by id (section 4) | Same "UI is a function of state" idea, without the app owning widget structs; matches how the X-conomy client is written today (ImGui) |
| SDF instanced primitives (rect, circle, line), per-batch scissor | Kept and extended: one unified instance format for shapes and glyphs, per-instance clip rect, one draw call per atlas page | Fewer state changes, order preserved for free |
| `wgpu_text` (glyph_brush) | FreeType rasteriser, HarfBuzz shaping, own glyph atlas and shape cache | Real shaping, font fallback, control over caching |
| Flexbox + grid layout over the `Widget` tree | Flex + grid layout over flat node arrays, Clay-style pass order with text wrapping between width and height passes | Correct wrapped-text heights, data-oriented |
| Semantic palette, Dark/Light/High Contrast | Kept, extended with metrics (spacing, radii, font sizes) and derived widget styles | |
| `DataSource` with `cell_text` and in-place `sort` | `TableSource` with typed batch access; queries produce row permutations and never touch the data (section 10) | Million-row tables, several views over one source |
| `InteractionState` with string action ids | Integer ids (hashed id stack), focus scopes, overlay layers | No string building per widget per frame |
| Rich text tags (`<b>`, `<color=..>`) | Kept as inline markup, plus a block document model with sections, links and search (section 12) | Contracts |
| `gloomy-driver` headless driver | Headless `Context` with an offscreen target; tests drive input events and compare PNGs | |

## 4. Frame model

The UI is declared every frame in immediate mode, but nothing is drawn while declaring:

```cpp
ui.BeginFrame(input_events, dt);
if (ui::Button(ui, "Undock")) {
  Undock();
}
ui::DataGrid(ui, "market", market_view, market_grid_state);
ui.EndFrame();          // layout, then build the draw list
renderer.Record(cmd);   // inside the host's render pass
```

1. Declaration: widget functions push nodes into a flat `NodeTree` (parent, first child, next sibling, layout spec, paint data). Each node has a 64-bit id hashed from the id stack (`ui.PushId()`), like ImGui. Containers open and close (`BeginRow()` / `End()`).
2. Interaction: widget functions answer "was I clicked, hovered, dragged" from the previous frame's layout boxes, looked up by id. A widget that did not exist last frame cannot be clicked this frame, which never matters in practice and keeps the frame single-pass. Input events are applied to the previous frame's boxes at `BeginFrame()`, before declaration, so a click resolves exactly once.
3. Layout runs on the finished tree (section 8), then the paint pass walks it and appends to the `DrawList`. Virtualised widgets (grid, list, document) are one node each; they paint only their visible rows during the paint pass, once their box is known.
4. Persistent widget state (scroll offsets, text edit cursors, open/closed, animation clocks) lives in a `StateStore` keyed by node id, created on first use and garbage-collected when an id has not been seen for a few frames. Large or app-relevant state (grid column layout, sort, filters, selection) is owned by the app in plain structs (`GridState`) so it can be saved, restored and shared between views; the widget only mutates it.

Why not a retained widget tree: X-conomy's client is immediate-mode code over SoA tables and is being ported, and the expensive things in a data UI (sorting, filtering, searching, shaping) are cached explicitly by the layers that own them, not by a widget tree. Why not pure ImGui-style layout-while-declaring: flex layout and wrapped text need the whole tree before positions are known.

Frame cost is O(visible nodes): a 1,000,000-row grid declares one node and paints about 50 rows.

## 5. Module layout

Like X-conomy: public headers in `include/gloomy/<module>/`, implementation in `src/gloomy/<module>/`, each module an OBJECT library collected into the static library `gloomy` (`cmake/BuildGloomyLib.cmake`), tests in `tests/<module>/` registered by `tests/cmake/<module>/test_<name>.cmake`.

| Module | Contents | Depends on |
|---|---|---|
| `core` | `Vec2`, `Rect`, `Color`, ids and hashing, UTF-8, case folding, PNG I/O | nothing |
| `gfx` | Vulkan device context, offscreen target, swapchain, `DrawList`, `UiRenderer`, shaders | core, Vulkan |
| `text` | fonts (FreeType), shaping (HarfBuzz), glyph atlas, shape cache, line breaking | core, FreeType, HarfBuzz |
| `data` | `TableSource`, columnar tables, queries, search corpus and indices, fuzzy matcher | core (no graphics) |
| `ui` | `Context`, node tree, layout, input, focus, theme, widgets, overlays, docking | core, gfx (draw list), text, data |
| `platform` | SDL2 window, Vulkan surface, event translation, clipboard | gfx, ui, SDL2 |

`data` has no graphics dependency, so the X-conomy simulation side or tools can use the query and search engine without a GPU. `platform` is the only module that sees SDL2; a host engine replaces it.

## 6. Vulkan renderer

### 6.1 Ownership

Device ownership is separable from UI rendering:

- `gfx::DeviceContext` holds instance, physical device, device, a graphics queue and its family, plus a small memory helper. It is either created by gloomy (`DeviceContext::Create()`, optionally with validation layers and a surface-compatible queue) or wraps handles the host engine owns (`DeviceContext::Wrap()`); in the latter case gloomy never destroys them.
- `gfx::UiRenderer` takes a `DeviceContext&` and the colour format it renders to. It owns only its pipeline, descriptor set, glyph atlas image and per-frame buffers. It records into a command buffer the caller provides: `Upload(cmd)` outside rendering (atlas uploads, buffer copies), `Draw(cmd, extent)` inside a dynamic-rendering scope the caller began. The host decides load ops, depth, what is underneath.
- `gfx::OffscreenTarget` (image, memory, view, readback buffer) and `gfx::Swapchain` (for gloomy's own windows) are siblings of the renderer, not part of it. `gfx::Frame` helpers run the begin/submit/present loop for the standalone case.

Vulkan 1.3 core features only: dynamic rendering (no render pass objects, so no render pass compatibility to negotiate with a host) and synchronization2. lavapipe and all current desktop drivers provide 1.3.

### 6.2 One instance format for everything

Every UI element is an instanced quad with one 96-byte instance layout:

```cpp
struct Instance {
  Vec4 rect;      // x0, y0, x1, y1 in pixels (line: p0, p1)
  Vec4 uv;        // atlas u0, v0, u1, v1 (glyphs, images)
  Vec4 radii;     // corner radii tl, tr, br, bl; circle radius; line width
  Vec4 clip;      // x0, y0, x1, y1 clip rectangle
  uint32 color;   // RGBA8, fill or start of gradient
  uint32 color2;  // RGBA8, gradient end
  float stroke;   // border width, 0 = filled
  float soft;     // edge softness, > 1 for shadows
  uint32 kind;    // rect, circle, line, glyph, image | atlas page << 8
  float3 pad;
};
```

The vertex shader expands the quad by the softness and antialiasing margin; the fragment shader evaluates the signed distance (rounded box with per-corner radii, circle, capsule line) or samples the atlas, multiplies by the clip-rect coverage and writes premultiplied alpha. Shadows are rectangles with large softness drawn before their box. Clipping per instance means clip changes never split a draw call, so a frame is one draw call per atlas page in submission order, and painter's order is exactly declaration order within a layer. Rounded clipping (clip radius) can be added to the clip vector later.

Instances are written to a host-visible, persistently mapped buffer per frame in flight, grown by doubling. A full-screen grid of 60 rows by 12 columns is about 10,000 glyph instances, under 1 MB per frame.

Colour is blended in gamma space into a UNORM target (`R8G8B8A8_UNORM` offscreen, `B8G8R8A8_UNORM` swapchain), like most UI toolkits; this keeps reference PNGs byte-identical to what is shown. Shaders are GLSL compiled to SPIR-V at build time with `glslc` and embedded as `uint32_t` arrays (`cmake/CompileShaders.cmake`).

### 6.3 Glyph atlas

Glyphs are rasterised as 8-bit coverage bitmaps at the exact pixel size with FreeType light hinting and four horizontal subpixel positions, not as SDF or MSDF. Data UIs are dominated by small text (11 to 14 px), where coverage bitmaps are clearly sharper than distance fields; zoomable or very large text can get an MSDF path later behind the same `kind`. The atlas is a 2D array image (`R8_UNORM`, 2048 x 2048, pages added on demand up to a limit) packed with a skyline allocator; uploads are batched per frame through a staging buffer in `Upload()`. When the atlas is full, it is cleared and refilled from the next frame's needs (one-frame hitch, rare).

### 6.4 Offscreen and readback

`OffscreenTarget` renders the same command stream into an image and copies it to a host buffer for PNG output. All image tests use it on lavapipe; the demo app has `--screenshot` for the same path on any device.

## 7. Text

- `text::FontSet` loads faces with FreeType from files or memory (Liberation Sans and Liberation Mono are embedded, SIL OFL) and keeps a fallback chain (for example Liberation Sans, then DejaVu Sans, then a CJK face if present).
- `text::Shaper` itemises a UTF-8 string into runs by font coverage (first face in the chain that has the code point), lets HarfBuzz guess script and direction per run, and shapes. The output is a `ShapedText`: glyph ids, faces, advances, offsets, and cluster byte offsets back into the source string. Cluster offsets are what makes caret placement, selection and search highlighting work on shaped text.
- `text::ShapeCache` caches shaped single-line strings keyed by (hash of bytes, font, size), evicted by last-used frame. A grid shows the same few thousand strings frame after frame; shaping is paid once per string.
- `text::LineBreaker` wraps shaped paragraphs at break opportunities (spaces and after hyphens now; UAX #14 later) and measures. Paragraph layouts are cached by (text, style, width).
- Bidi: runs carry a direction; the line layout reorders runs once a bidi algorithm (SheenBidi or FriBidi) is added. Not in the first phases.

Rich inline markup (`<b>`, `<i>`, `<color=#rrggbb>`, `<size=..>`, `<mono>`, `<link=target>`) from gloomy is kept and parsed into style runs; plain strings never pay for it.

## 8. Layout engine

Nodes carry a layout spec: direction (row, column, grid, stack/overlay), padding, gap, main and cross alignment, per-axis sizing (`Fixed(px)`, `Fit` (content, with min/max), `Grow(weight)`, `Percent`), and grid templates (`Px`, `Fr`, `Auto` tracks, explicit cell and span). The algorithm follows Clay's pass order so wrapped text gets correct heights:

1. Fit widths bottom-up (text: longest unbreakable word as minimum, full line as preferred).
2. Distribute widths top-down: grow nodes share leftover space by weight, shrink when over-full, respecting min/max.
3. Wrap text to the final widths (via the text module's measure callback).
4. Fit heights bottom-up, then 5. distribute heights top-down.
6. Positions top-down, applying alignment, scroll offsets and clipping.

Layout runs over the flat node arrays with no allocation in steady state. Text measurement is injected (`MeasureText` callback), so layout tests need no fonts.

## 9. Input, focus and keyboard

- Platform-neutral `InputEvent`s (pointer move/button/wheel, key down/up with modifiers, committed text) are queued by the platform layer and consumed in `BeginFrame()`.
- Hover and press resolve against the previous frame's boxes, top overlay layer first. A pressed widget captures the pointer until release (drag resize, scrollbar, slider).
- Focus is a node id. Tab and Shift+Tab move through focusable nodes in tree order within the current focus scope (a panel, a modal, a popup). F6 / Shift+F6 cycle panels, Esc closes the top overlay or leaves the current scope.
- Composite widgets (list, grid, tree, tabs, document) own their inner navigation: arrows, Page Up/Down, Home/End, Enter to activate, Space to toggle, Shift to extend, Ctrl+A, and type-to-jump: printable keys typed within one second form a prefix that jumps to the next row whose key column starts with it (X3 list behaviour).
- Shortcuts are bound to command ids in a `Keymap` (rebindable, saved with settings), and every command is in the command palette with its binding shown. Widgets consume keys before the keymap only when focused and when the key is theirs (a text field eats letters, a grid does not eat Ctrl+P).
- The clipboard is behind an interface the platform implements (SDL2 for now).

## 10. Data layer

The data layer is the core of the library. Its rules: data stays where the app keeps it; a view is a permutation of row indices; every pass over all rows is incremental, cancellable and off the frame path once the table is large.

### 10.1 Sources

```cpp
enum class ColumnType : uint8_t { kInt64, kDouble, kText, kBool, kEnum };
using RowIndex = uint32_t;

class TableSource {
 public:
  virtual auto RowCount() const -> RowIndex = 0;
  virtual auto Columns() const -> std::span<const ColumnInfo> = 0;
  /// Changes whenever any cell changes. Equal versions mean equal data.
  virtual auto Version() const -> uint64_t = 0;
  /// Typed batch reads; the default per-row versions are overridable.
  virtual void ReadInt64(int col, std::span<const RowIndex> rows,
                         std::span<int64_t> out) const;
  virtual void ReadDouble(int col, std::span<const RowIndex> rows,
                          std::span<double> out) const;
  /// Single-cell reads; text views stay valid while the source is
  /// unchanged.
  virtual auto GetInt64(int col, RowIndex row) const -> int64_t;
  virtual auto GetDouble(int col, RowIndex row) const -> double;
  virtual auto GetText(int col, RowIndex row) const -> std::string_view;
  /// Display text for one cell (formatting, units, thousands separators).
  virtual void FormatCell(RowIndex row, int col, std::string &out) const;
  /// Stable identity of a row across versions, for selection and focus.
  virtual auto RowKey(RowIndex row) const -> uint64_t { return row; }
  /// The "why" behind a cell (section 2.3); false if there is none.
  virtual auto Explain(RowIndex row, int col, Explanation &out) const
      -> bool { return false; }
  /// True if new versions only append rows (logs).
  virtual auto AppendOnly() const -> bool { return false; }
};
```

`ColumnInfo` names the column, its type, enum labels, an optional parent row column (tree data), whether it is searchable and whether it has explanations. Implementations:

- `ColumnarTable`: owns typed column vectors (`std::vector<int64_t>`, `std::vector<double>`, a text column as one byte blob plus offsets), appends rows, bumps the version. Good for logs and generated data.
- `FunctionTable`: zero-copy adapter over structure-of-arrays the app already has (X-conomy's `World` tables are exactly that): each column is a callable `(RowIndex) -> value`, plus optional row keys and explanations.

Batch reads exist so the sort and filter loops call a virtual function once per 4,096-row chunk, not once per cell.

### 10.2 Queries and views

A `Query` is a value type and is what a "saved view" stores:

```cpp
struct Query {
  std::vector<SortKey> sort;      // column, direction, nulls first/last
  std::vector<Filter> filters;    // AND of per-column filters
  std::string search;             // quick search across searchable columns
  std::vector<int> search_columns;
  std::vector<int> group_by;      // row grouping, outermost first
  std::vector<Aggregate> aggregates;
};
```

Filters are typed: numbers (`=`, `!=`, `<`, `<=`, `>`, `>=`, between, blank), text (contains, not contains, equals, starts with, ends with, blank; case-insensitive), enum and bool (in set; the filter popup lists distinct values with counts, computed on demand), and arbitrary predicates for app-defined filters.

Executing a query against a source yields a `RowView`: a `std::vector<RowIndex>` of the matching source rows in display order (4 bytes per row, 4 MB at a million rows), plus, when grouping, a group structure (group key, aggregate values, child range) that the grid flattens into display rows according to expanded groups. Tree data flattens the same way from parent links. The data itself is never copied, and many views (a market table sorted by price, the same table filtered by sector) share one source.

Execution pipeline, each stage operating on index vectors and typed key columns:

1. Filter: evaluate each filter column-wise over 4,096-row chunks into a selection vector; cheapest and most selective filters first.
2. Search: substring match over the search corpus (10.3), restricted to the surviving rows.
3. Sort: extract each sort key once into a contiguous key array (int64, double, or for text a 64-bit prefix key built from the case-folded first 8 bytes plus the string view for ties), then a stable sort of the index vector comparing prefix keys first. Above 100,000 rows the sort runs as parallel chunk sorts plus a merge on the worker pool.
4. Group and aggregate: one pass over the sorted view (grouping columns are implicit leading sort keys).

Incrementality, which is what keeps typing and live data smooth:

- Narrowing: if the new query only tightens the old one (a search string extended by a character, a filter made stricter, the same sort), it runs on the previous result instead of all rows, and the sort order is preserved, so no re-sort. Backspace pops a short stack of previous results.
- Re-sort only: changing the sort reorders the existing filtered rows.
- Append-only sources: new rows are filtered, searched (the corpus is extended, not rebuilt), sorted among themselves and merged into the view (`ExecuteAppend`).
- Value changes (prices every tick): the view is recomputed when the source version changes, but throttled (default at most every 250 ms for large tables), and selection and focus survive because they are kept as `RowKey`s, not positions.

Execution model: each `TableView` (`include/gloomy/data/table_view.h`) owns one worker thread and runs its queries against the shared `std::shared_ptr<const TableSource>`; sorts and corpus builds split further across `WorkerThreads()` (at most 4). The UI keeps showing the previous `RowView` until the new one is ready (with a subtle busy indicator), then swaps it in at the start of a frame; a newer query cancels an older one at the next chunk boundary. Sources must not change while a snapshot is shared: an app either publishes immutable snapshots (X-conomy can produce per-tick UI tables) or opts into synchronous execution, which tables under about 50,000 rows use anyway because it finishes well within a frame.

### 10.3 Search core

One search core serves tables, documents and global search:

- `SearchCorpus`: a list of documents, each a span of normalised text (UTF-8, case-folded, same byte length as the original so offsets map back one to one) stored in one contiguous blob with an offsets array, plus per-document segment boundaries (which column of a row, which block of a document). A table corpus has one document per row with the searchable columns concatenated with a separator; a document corpus has one per paragraph.
- Matching is literal case-insensitive substring search (what people expect when looking for a station name or a contract clause), with multiple space-separated terms ANDed. A query yields `Match{doc, segment, begin, end}` in original byte offsets, which the grid and document view turn into highlight rectangles through the shaped clusters.
- `SearchCorpus::Filter` scans the blob with a `memchr` first-byte search plus verification, in parallel chunks for large corpora; narrowing (a longer query searched only within the previous hits) happens one level up, in query execution. The corpus of the 1,000,000-row sample market is built in about 0.5 s on 4 threads, once per data version.
- `TrigramIndex` (planned, not built yet) is the optional index for large, mostly static corpora (logs, long documents): posting lists of document ids per case-folded trigram, built incrementally as documents are appended, intersected to get candidates which are then verified by scan. Queries shorter than three bytes fall back to scanning.
- Corpora are built lazily on the first search of a table or document and kept up to date incrementally for append-only sources; for changed sources they are rebuilt in the background when the version changes, and the old corpus serves until then.
- `FuzzyMatcher` (subsequence match with scoring for word starts, consecutive runs and case, like fzf) is used for the command palette and for global search over names, not for data cells.

### 10.4 Global search

A `SearchRegistry` holds `SearchProvider`s (each table view, each open document, the command list, app-specific ones like "stations by name"). Global search fans a query out to all providers on the worker pool with a per-provider result cap and returns typed results (provider, label, context snippet with highlight ranges, a navigation callback). The command palette is the same UI with only the command provider and fuzzy matching.

## 11. Widgets

Designed from what professional data tools and games need, not from gloomy's list. Tiers say what comes first; the architecture (nodes, overlays, focus scopes, data layer) is the same for all of them.

### 11.1 Tier 1: what the X-conomy client needs first

- Data grid, the centrepiece (11.2).
- Answer-first and progressive disclosure widgets (section 2): `SummaryCard` (glance: headline `ExplainedValue`, delta, sparkline, status colour, drill-down), `SummaryPanel` (panel: key numbers and a compact grid of the preset's panel columns), `AnswerLine` (one-line answer with reason and a single drill-down action), `ExplainedValue` display (value with a "why" tooltip or inline note), and a `PresetBar` to pick, save, duplicate and share `ViewPreset`s.
- List: the X3-style single-column list (keyboard selection, Enter activates, type to jump, virtualised), implemented as a grid without header.
- Search field: incremental, debounced to the frame, shows match count and busy state, Up/Down or F3 for next/previous match, Esc clears.
- Document view for contracts (section 12).
- Panels and layout: split panes with draggable splitters, tabbed panel stacks, and a docking workspace whose layout tree (splits with ratios, tab stacks of panel ids) is a serialisable value. Panels are drawn by app callbacks per panel id. Drag-to-dock comes after keyboard and splitter handling.
- Overlays: tooltips (delayed, follow the hovered node), context menus (nested, keyboard navigable), dropdowns and popups, modal dialogs (focus scope, Esc/Enter), toasts (timed, stacked, actionable).
- Command palette (Ctrl+P / Ctrl+Shift+P) and global search (Ctrl+K), sections 10.4 and 9.
- Basics: label, rich label, button, icon button, checkbox, toggle, radio group, slider, number field, text field, dropdown/combo with search, tabs, scroll container, progress bar, separator, badge.

### 11.2 Data grid

State the app owns and can save as a `ViewPreset` (section 2.4):

- Columns: order, width, visibility, pinned left or right, column groups (multi-level headers), per-column renderer and formatter.
- Query: multi-column sort (click sorts, Shift+click adds a key, the header shows the key's rank), per-column typed filters (header filter popup), quick search, grouping with aggregates (sum, min, max, mean, count), tree rows.
- Selection: a set of `RowKey`s (rows) with anchor and cursor, Ctrl+click toggle, Shift+click and Shift+arrows extend, Ctrl+A; optional cell-range selection for copy. Copy (Ctrl+C) writes the selection as TSV with headers.
- Scroll position as first visible display row plus a pixel offset, so a million rows at 24 px (24,000,000 px) never suffers float precision loss.

Behaviour: virtual rows and columns (only visible cells format, shape and paint); column resize by dragging header edges (double-click autosizes from visible rows), reorder by dragging headers or Alt+Left/Right, hide from the header context menu (which also has sort, filter, pin, group by, autosize); keyboard navigation of the cursor cell; Enter activates the row (app callback), type-to-jump on the primary sort column.

Cell renderers: text, number (right aligned, units, thousands separators, signed colouring), bar (value against a column range), sparkline (source returns a small series), age-coloured text (value plus age to colour ramp, X-conomy's knowledge age), enum badge, icon plus text, and custom renderers that paint into the cell rect through the `DrawList`.

### 11.3 Tier 2 and later

Tree view (shares the grid's flattened-tree view), property grid, forms with field validation and error display, date and time fields, charts beyond sparklines (line, bar, candlestick for price history), drag and drop between views, multiple windows, a theme editor.

## 12. Document view

Contracts are long, structured texts with references to game objects. The document model:

- `Document`: a list of blocks (heading with level, paragraph, list item with depth, quote, code, rule, simple table) with inline style runs (bold, italic, monospace, colour, highlight, link with a target string such as `station:42`). Built in code or parsed from a Markdown subset with gloomy's inline tags.
- Layout per block is cached by width. The view is virtualised for variable heights: block heights are estimated, replaced by exact heights as blocks are laid out, and kept in a Fenwick tree so scroll offset to block and back are O(log n). Only blocks in the viewport are shaped.
- Headings form an outline (section navigator, jump by keyboard). Links are focusable and activate an app callback. Text can be selected and copied.
- Search uses a document corpus (one document per block); matches are highlighted and F3 / Shift+F3 move between them, scrolling the block into view. Persistent highlight sets (for example "clauses that changed") use the same highlight path.

## 13. Theming

`Theme` is a semantic palette (background, surface, surface_alt, primary, accent, text, text_muted, text_disabled, hover, active, focus, selection, success, warning, error, info, border, divider) plus metrics (base font size, spacing unit, radii, row height, border width). Dark (default), Light and High Contrast are built in. Widget styles are derived from the theme at use, with per-call overrides. An X3-style theme for X-conomy (thin borders, translucent panels, cyan accent) is just another theme. Themes are plain structs; a text format can come later.

## 14. Keeping large data at 60 fps

The frame budget is 16.6 ms; the rules that keep the UI inside it:

- Per-frame work is proportional to what is visible. Everything else is cached (shaped strings, paragraph layouts, glyphs) or computed off the frame path (queries, corpora, indices).
- Work that touches all rows runs on the worker pool against immutable snapshots, cancellable at chunk boundaries; the UI swaps results in when ready and never waits.
- Typing in a search field narrows the previous result; it never rescans what is already excluded.
- Selection, focus and scroll anchors are `RowKey`s, so a re-query never makes the view jump.
- Rendering is one draw call per atlas page from one mapped buffer.

The benchmark (phase 5) measures frame time at 1,000,000 rows while scrolling, re-sorting and typing a search, on lavapipe and on the real GPU, and reports query latencies separately.

### 14.1 Measured

`gloomy_bench` (`src/apps/bench/main.cc`, release build, 1280 x 800, 1,000,000 rows of the sample market, grid plus search field, offscreen) on the development machine. UI is the CPU time of a frame (input, declaration, layout, painting into draw lists); GPU is upload, draw and wait.

| Scenario | UI avg | UI p95 | GPU avg (RTX 3060) | GPU avg (lavapipe) |
|---|---:|---:|---:|---:|
| Idle | 0.16 ms | 0.19 ms | 1.1 ms | 17 ms |
| Page Down every frame | 0.43 ms | 0.78 ms | 1.3 ms | 22 ms |
| Wheel scrolling | 0.23 ms | 0.31 ms | 1.2 ms | 16 ms |
| Home/End jumps | 0.26 ms | 0.39 ms | 1.2 ms | 20 ms |
| Typing a search | 0.26 ms | 0.50 ms | 1.3 ms | 16 ms |
| Re-sorting | 0.35 ms | 0.80 ms | 2.2 ms | 18 ms |

Query latency (time until the new rows show; frames keep rendering meanwhile): first search keystroke about 0.5 s (corpus build), later keystrokes 10 to 70 ms (narrowing), Backspace 15 to 35 ms (cached results), sorting a million rows 0.2 to 0.9 s depending on the column (text sorts are slowest). The worst single UI frame seen was about 5 ms, when a new result is applied and the cursor's position is looked up (O(n)).

## 15. Testing

- Unit tests (GoogleTest) for core, layout (with a fake text measure), data (queries, narrowing, search, corpora), text (shaping results, line breaking) and widget behaviour (a headless `Context` fed with synthetic input events; assertions on state and on the produced draw list).
- Image tests render offscreen on lavapipe (`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`) and compare against reference PNGs in `tests/reference/` with a per-channel tolerance and a maximum count of differing pixels. On mismatch the actual image and a diff image are written to the build tree. `GLOOMY_UPDATE_REFERENCES=1` rewrites the references; every rewrite is reviewed by looking at the images.
- Debug builds enable `VK_LAYER_KHRONOS_validation`; a validation message fails the test (the debug messenger counts errors and tests assert zero).

## 16. Phases

1. This document.
2. Foundation: CMake project, Vulkan context, offscreen target, swapchain on an SDL2 window, SDF primitive pipeline, text with shaping and the atlas, minimal demo, headless image tests.
3. Layout engine, input and focus, theming, basic widgets (label, button, text field, checkbox, slider, dropdown, tabs, scroll container, list).
4. Data layer and the data grid, search field, document view.
5. Showcase example and benchmark.

Docking, overlays and the command palette are spread over phases 3 to 5 as the showcase needs them.

## 17. Status

What exists (October 2026), against this plan. File pointers are in `CLAUDE.md`.

Built:

- Renderer: device context (create or wrap a host device), offscreen target, swapchain, one-pipeline SDF and glyph renderer with per-instance clipping; validation clean on lavapipe and an RTX 3060.
- Text: FreeType/HarfBuzz shaping with font fallback, glyph atlas, shape cache, wrapping, rich multi-run layout (`LayoutRich`).
- UI core: immediate-mode context with stable ids, layout engine, input, focus and Tab order, focus rings, popups, tooltips, modals, floating layers; Dark, Light and High Contrast themes.
- Widgets: label, heading, button, checkbox, radio, slider, progress, text field, dropdown, tabs, scroll area, X3-style list, search field, tooltips, context menus, dialogs, toasts, split panes, and the docking workspace (`DockLayout`, `DockSpace`: drag-to-dock, serialisable layouts).
- Data: `TableSource`, `ColumnarTable`, `FunctionTable`, queries with typed filters, multi-key parallel sort, quick search, grouping and aggregates, narrowing/re-sort/cache/append paths, background `TableView`s, shared search corpora, fuzzy matching.
- Data grid: everything in 11.2 (including grouping, tree rows, column groups and sparkline cells) except right pinning, cell-range selection and icon cells; plus a standard cell context menu ("only this value", "hide this value", copy).
- Section 2: `ExplainedLabel`, `Sparkline`, `SummaryCard` (glance), `SummaryPanel` (panel), `AnswerLine`, `ViewPreset` with shareable text encoding, `PresetBar`, and `NavigationTarget` with `Navigate()` and `NavigationProvider` (the shared keyboard and search contract for views the app draws, such as the map).
- Line charts (`LineChart`: multiple series, second axis, legend toggles, crosshair values, zoom and pan, min/max decimation), the chart X-conomy's client draws with ImPlot today.
- Document view with outline jumps, links, search, highlights and text selection; the Finder (command palette and global search over tables, documents and commands).
- Showcase (`gloomy_showcase`) and benchmark (`gloomy_bench`, section 14.1).

Not built yet: right-pinned columns and cell-range selection in the grid; trigram index; bidirectional text; number, date and time fields; chart types beyond lines and sparklines (bars, candlesticks); the windowed path runs in tests on a headless surface (`VK_EXT_headless_surface` on lavapipe) but has not been seen in a real window here, since this machine has no display.

