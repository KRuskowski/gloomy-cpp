# gloomy-cpp

C++23 respin of gloomy (`~/prsn/gloomy`, Rust/wgpu, ~18k lines: SDF primitives, flexbox/grid layout, themes, RON-declared UIs, rich text, virtualised DataGrid, charts, designer). Renders on raw Vulkan; windows and input through SDL2 (Vulkan surface) for now. First consumer: the X-conomy client (`~/dev/x_conomy`), which needs very large searchable tables and long searchable text (contracts).

Design: `docs/design.md` (read it first; section 2 is the guiding principle "deep data, accessible surface"). gloomy is a reference for the low level only, not for the widget layer.

Current state: phases 1-3 done (design, foundation, layout/input/focus/theme/basic widgets). Next: phase 4, the data layer (`TableSource`, queries, search) and the DataGrid.

## Build and test

```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build-debug -j4 && ctest --test-dir build-debug -j4
# Headless screenshot of the demo (any Vulkan device, lavapipe shown):
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json \
    ./build-debug/src/apps/demo/gloomy_demo --screenshot /tmp/demo.png
```

- GPU tests run on lavapipe: ctest sets `VK_ICD_FILENAMES` from the cache variable `GL_TEST_ICD` (`tests/CMakeLists.txt`); empty means the system default device.
- Validation (`VK_LAYER_KHRONOS_validation`) is on in every GPU test; any message fails the test (`tests/test_support/gpu_test.cc`). `GLOOMY_NO_VALIDATION=1` turns it off.
- Image tests compare with `tests/reference/*.png` (tolerance per channel, max differing pixels). On failure `build-debug/test_output/<name>_actual.png` and `_diff.png` are written. `GLOOMY_UPDATE_REFERENCES=1 ctest ...` rewrites references: always look at the new images (Read tool) before committing them.
- `GLOOMY_DEVICE=<index>` picks a physical device.
- Dependencies (system): Vulkan headers/loader, glslc, FreeType, HarfBuzz (pkg-config), libpng, SDL2 (`-DBUILD_PLATFORM_SDL2=OFF` to skip), GoogleTest (system or fetched).

## Layout

Like X-conomy: public headers `include/gloomy/<module>/`, implementation `src/gloomy/<module>/`, each module an OBJECT library in `cmake/BuildGloomyLib.cmake`, collected into the static library `gloomy`; SDL2 platform layer is the separate `gloomy_sdl2`. Tests `tests/<module>/test_<name>.cc`, registered by `tests/cmake/<module>/test_<name>.cmake`. Apps in `src/apps/`.

- Shaders: `src/gloomy/gfx/shaders/ui.{vert,frag}`, compiled by glslc at build time and embedded (`cmake/CompileShaders.cmake`, `cmake/EmbedFileScript.cmake`). Fonts in `assets/fonts/` (Liberation, OFL) are embedded the same way (`cmake/EmbedFiles.cmake`).
- One instance format for every primitive: `Instance` in `include/gloomy/gfx/draw_list.h`; must match the vertex attributes in `src/gloomy/gfx/ui_renderer.cc` (`CreatePipeline`) and `ui.vert`.
- Device ownership vs. UI rendering: `gfx::DeviceContext` (`Create` or `Wrap` a host device), `gfx::UiRenderer` (`Prepare` outside, `Draw` inside the caller's dynamic rendering), `gfx::OffscreenTarget`, `gfx::Swapchain`.
- UI frame model: `ui::Context` (`include/gloomy/ui/context.h`): immediate-mode declaration into a per-frame node tree; input resolves against the previous frame's layout (`ProcessEvents`, `HitTest`); results are swapped in at the end of `EndFrame()`; per-id state via `State<T>(id)`; overlay layers (`BeginLayer`: popup, tooltip, modal). Layout engine: `ui::LayoutTree` (`src/gloomy/ui/layout.cc`, Clay-style passes). Widgets: `include/gloomy/ui/widgets.h`. Themes: `src/gloomy/ui/theme.cc`.
- Headless UI tests drive a `testing::UiHarness` (`tests/test_support/ui_harness.h`) with synthetic events; no GPU needed. SDL event translation: `platform::TranslateEvent` (`src/gloomy/platform/sdl_window.cc`).
- Text: `text::FontSet` (faces, fallback chains), `text::Shaper` (HarfBuzz, font fallback runs, clusters = byte offsets), `text::GlyphCache` (FreeType coverage bitmaps, 4 subpixel bins, into `core::Atlas`), `text::TextSystem` (shape cache, wrap, draw).

## Conventions

Same as X-conomy / OTC.SDK branch `staging-v12`: Google C++ style, C++23, `snake_case.h/.cc`, include guards (no `#pragma once`), trailing return types, PascalCase functions, `member_` suffix, `kConstants`, `///` doxygen comments on the line above, `.clang-format`/`.clang-tidy`/`.editorconfig` in the repo (`clang-format -i` before committing). Python follows `.flake8`. Unit tests (GoogleTest) for all code; one test file per test binary. Markdown prose is never hard-wrapped.

Git: branch `gl-core`, commit as Karl Ruskowski <karl.ruskowski@optris.de> (repo-local config), unsigned, no Co-Authored-By; push to `origin` only.
