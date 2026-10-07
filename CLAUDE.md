# gloomy-cpp

C++23 respin of gloomy (`~/prsn/gloomy`, Rust/wgpu, ~18k lines: SDF primitives, flexbox/grid layout, themes, RON-declared UIs, rich text, virtualised DataGrid, charts, designer). Renders on raw Vulkan; windows and input through SDL2 (Vulkan surface) for now. First consumer: the X-conomy client (`~/dev/x_conomy`), which needs very large searchable tables and long searchable text (contracts).

## Conventions

Same as X-conomy / OTC.SDK branch `staging-v12`: Google C++ style, C++23, `snake_case.h/.cc`, include guards (no `#pragma once`), trailing return types, PascalCase functions, `member_` suffix, `kConstants`, `///` doxygen comments on the line above, `.clang-format`/`.clang-tidy`/`.editorconfig` in the repo. Python follows `.flake8`. Unit tests (GoogleTest) for all code; one test file per test binary. Markdown prose is never hard-wrapped.

## Testing

Headless rendering tests run on Mesa's lavapipe (`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json`): render offscreen, read back pixels, compare against reference images with a tolerance.
