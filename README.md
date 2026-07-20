# imgui-painter

A rendering and styling toolkit for [Dear ImGui](https://github.com/ocornut/imgui), in C++ and
Rust — high-quality visuals without replacing ImGui's widget, layout, or input systems.

## Restyle a stock widget

The headline feature: that is a real `ImGui::Button()`. It keeps its ID, layout, input handling,
keyboard navigation, and return value. imgui-painter suppresses only the `ImGuiCol_` roles it
replaces and paints its own chrome underneath.

```cpp
#include "imgui_painter_decorators.h"

ip::Context ctx;                                   // long-lived, owns one native context
const ip::Material material = ip::raised_button(palette);

// ...per frame:
auto frame = ip::begin_frame(ctx);

bool clicked = ip::decorate_button(frame, material, [] {
    return ImGui::Button("Save");
});
```

No wrapper widget, no reimplementation, no fork of ImGui's logic. The same shape covers all seven
supported widgets: Button, Selectable, Checkbox, InputText, Slider, Combo, and TreeNode.

## Paint your own surfaces

```cpp
ip::Context ctx;                          // long-lived
auto frame  = ip::begin_frame(ctx);       // samples the host values for you
auto canvas = ip::window_canvas(frame);   // targets the current window's draw list

canvas.rounded_rect(rect, 6.0f)
    .shadow(drop)                         // painted first, so it sits behind
    .fill(surface_gradient)
    .band(top, top + hairline, highlight) // one-device-pixel bevel
    .border({1.0f, outline});
```

Shadows, multi-stop gradients, gloss bands, bevel hairlines, and stacked borders compose in call
order — no styling language, no cascade, no selectors. A `Canvas` accumulates everything drawn on
it and submits once when it goes out of scope.

## Install

```cmake
include(FetchContent)
FetchContent_Declare(imgui-painter
    GIT_REPOSITORY https://github.com/samanshaiza004/imgui-painter.git
    GIT_TAG        v0.1.1
)
FetchContent_MakeAvailable(imgui-painter)
target_link_libraries(your_app PRIVATE imgui_painter::imgui_painter)
```

Or install and `find_package(imgui-painter REQUIRED)` against the same target. CMake 3.16+, C++17.
Rust users: see [the Rust binding](bindings/rust).

## What you get

- **Seven widget decorators** that restyle stock ImGui widgets in place, preserving the last-item
  contract so tooltips, context menus, and drag/drop still attach normally.
- **A painting core with zero Dear ImGui dependency** — pure math in, a vertex/index mesh out. It
  rides whatever ImGui build you already linked, so there is never a second ImGui instance.
- **Shadows, four gradient modes, bands, and stacked borders** composing in explicit call order.
- **HiDPI-correct hairlines** — sub-pixel borders draw at one device pixel with proportionally
  reduced alpha instead of blurring.
- **A 9-token palette and recipe family** that also maps across all 56 stock ImGui colour roles.
- **C++ and Rust at parity**, both on the same C ABI.

> **Status: early release (v0.1.1).** The API is pre-1.0 and may still evolve. C++ and Rust are at
> feature parity; see [Project status](#project-status) for the specifics.

## Compatibility, in one paragraph

The painting core is independent of any particular Dear ImGui version. **The decorators are not** —
they reconstruct stock widget chrome geometry, which is internal ImGui detail upstream may change
in any release. They target Dear ImGui **1.91.9b**. The C++ header enforces this with a
`static_assert`; a mismatched version fails to compile rather than silently painting in the wrong
place. See [Compatibility](#compatibility) for the full story.

## Why

Dear ImGui's style system covers colors, rounding, spacing, and border sizes. Past that,
styling means writing `ImDrawList` geometry by hand — and hand-written geometry doesn't
compose. One moderately rich button (drop shadow, base gradient, gloss band, bevel highlight,
inset shadow for the pressed state, two borders) is easy in pieces and a mess of ordering bugs
and magic numbers together, rewritten per widget. Then the display scale changes and the
hairlines blur.

imgui-painter keeps that composition explicit and ordered:

```cpp
ip::Painter painter({uv.x, uv.y}, rect, radius);
painter.pixel_scale(scale)
    .shadow(outer)                            // elevation
    .fill(surface)                            // base
    .band(top, gloss_end, gloss)              // gloss
    .band(top, top + hairline, highlight)     // bevel
    .shadow(inset)                            // depth
    .border(outer_border)                     // outline
    .border(hairline, inner_border);          // inset outline
painter.draw(*ImGui::GetWindowDrawList());
```

Every line is a draw operation whose effect depends on the ones before it. There is no state
to push and pop and nothing that happens implicitly between two adjacent calls.

## Architecture

```
imgui-painter core     (C++17; ZERO Dear ImGui / cimgui dependency — pure
                        math in, a generic vertex/index mesh out)
        ↑ C API (capi/imgui_painter_c.h)
   ┌────┴──────────────────────────┐
include/*.h                    bindings/rust
(header-only C++: ImGui-free    (Rust adapter, recipes, and the
 core + fluent API, plus         widget decoration layer)
 opt-in recipes, host-value
 sampling, and decorators)
        ↑                               ↑
   host app (C++)                  host app (Rust)
```

The core never links Dear ImGui or cimgui — it rides whatever ImGui build the host already
linked, so there is never a second ImGui instance or an ABI-layout guess.

`draw()` is a **template** parameterized on the draw-list type rather than a concrete
`ImDrawList&`, which is what keeps even the C++ fluent header ImGui-dependency-free. It calls
`PrimReserve`/`PrimWriteVtx`/`PrimWriteIdx` generically, resolving against a real `ImDrawList`
or against a duck-typed mock (there is a compile-check test that does exactly that).

Writing through those public prim methods is deliberate. `ImDrawList`'s public _fields_ are
stable to read, but its _invariants_ — write-pointer bookkeeping, texture and clip-rect
stacking, large-mesh vertex-offset handling — are maintained by methods like `PrimReserve`, are
not covered by any ABI guarantee, and have changed across Dear ImGui versions.

## Using it from C++

The library builds with CMake (3.16+, C++17). Consume it with `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(imgui-painter
    GIT_REPOSITORY https://github.com/samanshaiza004/imgui-painter.git
    GIT_TAG        v0.1.1
)
FetchContent_MakeAvailable(imgui-painter)
target_link_libraries(your_app PRIVATE imgui_painter::imgui_painter)
```

or install it and use `find_package(imgui-painter REQUIRED)` against the same target. Either way
the include is `#include <imgui_painter.h>`.

Building the repo directly gives you the core library only — no network, no Dear ImGui, about two
seconds:

```sh
cmake -B build && cmake --build build
```

Examples and tests are opt-in (`-DIMGUI_PAINTER_BUILD_EXAMPLES=ON`,
`-DIMGUI_PAINTER_BUILD_TESTS=ON`) because they fetch full Dear ImGui and GLFW checkouts.

Which header you include decides whether you take a Dear ImGui dependency at all:

| Header                       | Adds                                                  | Includes `imgui.h`? |
| ---------------------------- | ----------------------------------------------------- | ------------------- |
| `imgui_painter.h`            | painting core, fluent API, `Context`/`Frame`/`Canvas` | no                  |
| `imgui_painter_recipes.h`    | `Palette`, material builders, `panel`/`inset_panel`   | no                  |
| `imgui_painter_imgui.h`      | automatic host-value sampling, `apply_imgui_colors`   | yes                 |
| `imgui_painter_decorators.h` | the seven widget decorators                           | yes                 |

The first two staying ImGui-free is load-bearing, not incidental: it is what lets `draw()` compile
against any type exposing `PrimReserve`/`PrimWriteVtx`/`PrimWriteIdx`, which CI checks against a
mock draw list.

Two host values drive correct output, and neither can be guessed safely:

| Value            | Where it comes from                        | Why it matters                                                                                                           |
| ---------------- | ------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------ |
| `white_pixel_uv` | `ImGui::GetFontTexUvWhitePixel()`          | Flat and gradient fills sample your atlas's solid-white texel. A wrong UV samples the wrong texel with no visible error. |
| `pixel_scale`    | `ImGui::GetIO().DisplayFramebufferScale.x` | Sub-pixel hairlines are drawn at one device pixel with proportionally reduced alpha. Skipping it blurs bevels on HiDPI.  |

`imgui_painter_imgui.h` samples both for you, once per frame:

```cpp
ip::Context ctx;                          // long-lived, owns one native context
auto frame = ip::begin_frame(ctx);        // samples both host values
auto canvas = ip::window_canvas(frame);   // targets the current window's draw list
canvas.rounded_rect(rect, 6.0f).fill(surface).border({1.0f, outline});
```

`Context` is reused across the whole frame; a `Canvas` accumulates every shape drawn on it and
submits once when it goes out of scope. `ip::Painter` remains available as the single-use
convenience path for one-off shapes — it creates and destroys one native context per instance, so
prefer `Context` when painting more than a couple of elements per frame.

`ip_begin` resets the pixel scale to `1.0`, so set it _after_ beginning a session — the fluent
`.pixel_scale()` call above is already in the right place.

### Plain C

The same operations are available directly on the C ABI, which is what every binding compiles
against:

```c
ip_ctx *ctx = ip_ctx_create();
ip_begin(ctx, white_pixel_uv);
ip_set_pixel_scale(ctx, 2.0f);
ip_rounded_rect(ctx, rect, 5.0f);
ip_add_shadow(ctx, &shadow);
ip_fill_gradient(ctx, &gradient);
ip_add_border(ctx, &border);
const ip_mesh mesh = ip_end(ctx);
/* copy mesh.vtx / mesh.idx into your draw list */
ip_ctx_destroy(ctx);
```

The mesh buffers are owned by the `ip_ctx` and stay valid until the next `ip_begin` or
`ip_ctx_destroy` — copy out anything you need to keep.

## Project status

Being precise about this matters more than making the library sound finished.

|                                                                   | C++                                  | Rust                              |
| ----------------------------------------------------------------- | ------------------------------------ | --------------------------------- |
| Painting core (shapes, gradients, shadows, borders, bands, lines) | ✅                                   | ✅                                |
| Fluent chaining API                                               | ✅                                   | ✅                                |
| Build system                                                      | ✅ CMake                             | ✅ Cargo                          |
| Reusable per-frame context                                        | ✅ `Context` → `Frame` → `Canvas`    | ✅ `Painter` → `Frame` → `Canvas` |
| Host values sampled automatically                                 | ✅                                   | ✅                                |
| **Widget decoration** (restyle a stock `ImGui::Button`)           | ✅ all seven widgets                 | ✅ all seven widgets              |
| Palette / recipes                                                 | ✅                                   | ✅                                |
| Examples and visual demo                                          | ✅ GLFW + OpenGL3                    | ✅ wgpu + winit                   |
| Tests                                                             | ✅ native, plus via the Rust binding | ✅                                |

Widget decoration covers Button, Selectable, Checkbox, InputText, Slider, Combo, and TreeNode in
both bindings.

The Rust binding got here first because it was the first consumer and the layer that proved the
design. That was a historical accident, not the intended end state: the C ABI exists specifically
so C++, C, Zig, C#, and Python bindings can sit on equal footing. The work that closed the gap is
recorded in **[docs/cpp-parity.md](docs/cpp-parity.md)**.

The two decorator implementations cannot share code — the geometry formulas read Dear ImGui's own
layout state, which lives on whichever side owns the context. **[docs/widget-anatomy.md](docs/widget-anatomy.md)**
is the single spec both implement, so they cannot drift silently.

### Compatibility

The painting core is independent of any particular Dear ImGui version.

The **decorators**, in both bindings, are not: they reconstruct stock widget chrome geometry,
which is internal Dear ImGui detail that upstream is entitled to change in any release. They
target Dear ImGui **1.91.9b** — in Rust via imgui-rs 0.12 fork rev
[`7a89260`](https://github.com/samanshaiza004/imgui-rs). A source-compatible ImGui bump can
compile cleanly while silently moving the widget away from the painted rectangle, so each side
guards it differently:

| | Guard | On a mismatch |
|---|---|---|
| C++ | `static_assert(IMGUI_VERSION_NUM == 19191)` | fails to compile, in every build mode |
| Rust | `debug_assert` on `igGetVersion()` | panics in debug; **compiled out in release** |

The C++ side pins harder on purpose, and offers `IMGUI_PAINTER_ALLOW_UNVERIFIED_IMGUI` as a named
opt-out for anyone who has rerun the visual gate themselves. On the Rust side,
[`VERIFIED_IMGUI_SYS`](VERIFIED_IMGUI_SYS) records the revision a human last visually verified and
CI fails when the resolved revision drifts from it. See the
[dependency-bump checklist](CONTRIBUTING.md#dependency-bump-checklist).

## Documentation

- **[The book](https://samanshaiza004.github.io/imgui-painter/)** — concepts, the C ABI,
  decorator anatomy, and recipes.
- **[Widget anatomy](docs/widget-anatomy.md)** — the chrome-geometry spec both decorator
  implementations follow, and why the non-obvious choices are deliberate.
- **[C++ parity design record](docs/cpp-parity.md)** — how the C++ side reached parity, kept as
  the record of what the gap was and why each choice was made.
- **[API reference](https://samanshaiza004.github.io/imgui-painter/api/imgui_painter/)** —
  rustdoc for the Rust binding.
- **[CONTRIBUTING.md](CONTRIBUTING.md)** — quality bar, visual gate, dependency-bump checklist.
- **[CHANGELOG.md](CHANGELOG.md)** — phase-by-phase development history.

## Repo layout

```
capi/imgui_painter_c.h            the C ABI — every language binding compiles against this
src/painter.cpp                   the core: tessellation, gradients, shadows, borders

include/imgui_painter.h           fluent API + Context/Frame/Canvas   (no imgui.h)
include/imgui_painter_recipes.h   Palette, materials, panel painters  (no imgui.h)
include/imgui_painter_imgui.h     host-value sampling, apply_imgui_colors
include/imgui_painter_decorators.h  the seven widget decorators

cmake/                            FindOrFetchImGui/GLFW, package config template
tests/cpp/                        native geometry + decorator tests (CTest)
examples/cpp/                     GLFW + OpenGL3 basic and gallery demos
bindings/rust/                    the Rust binding, its examples and benches
book/                             prose documentation (mdBook)
docs/                             widget-anatomy spec, design records, screenshots
```

## Testing

Native C++ tests run under CTest. The core suite drives the C ABI directly and needs no Dear
ImGui and no network, so it runs on a plain default build:

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

That is 40 geometry tests. The 15 decorator tests need a real Dear ImGui and are an explicit
opt-in (`-DIMGUI_PAINTER_BUILD_IMGUI_TESTS=ON`), bringing the total to 55.

The Rust binding carries its own 67 tests over the same core, plus a zero-allocation steady-state
check and a compile-check of the ImGui-free headers against a mock draw list. Testing one core
through two bindings is deliberate — it is the property worth protecting.

None of it covers final rasterized appearance, which is what the human visual gate in
[CONTRIBUTING.md](CONTRIBUTING.md#the-visual-gate) is for.

## License

Dual-licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE))
- MIT license ([LICENSE-MIT](LICENSE-MIT))

at your option.
