# imgui-painter

A rendering and styling toolkit for [Dear ImGui](https://github.com/ocornut/imgui), written
in C++. It makes high-quality visuals as easy as `PushStyleColor`, without replacing ImGui's
widget, layout, or input systems.

```cpp
#include "imgui_painter.h"

const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
const ip_rect rect = {{p.x, p.y}, {p.x + 200.0f, p.y + 32.0f}};

ip::Painter({uv.x, uv.y}, rect, 5.0f)
    .pixel_scale(ImGui::GetIO().DisplayFramebufferScale.x)
    .shadow(drop_shadow)     // painted first, so it sits behind
    .fill(surface_gradient)
    .border(outline)
    .draw(*ImGui::GetWindowDrawList());
```

Shadows, multi-stop gradients, gloss bands, bevel hairlines, and stacked borders compose in
call order — no styling language, no cascade, no selectors.

**What it is not:** a design system, a widget-set replacement, Qt, or CSS. It is closer to a
2D rendering framework specialized for Dear ImGui than to "a styling helper."

> **Status: early.** The C++ layer is the painting core plus a header-only fluent wrapper.
> It has **no build system yet**, and the widget-decoration layer currently exists only in the
> Rust binding. See [Project status](#project-status) — it is deliberately specific about what
> does and does not exist.

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
include/imgui_painter.h        bindings/rust
(header-only C++ fluent         (Rust adapter; also hosts the widget
 wrapper, ImGui-free)            decoration layer)
        ↑                               ↑
   host app (C++)                  host app (Rust)
```

The core never links Dear ImGui or cimgui — it rides whatever ImGui build the host already
linked, so there is never a second ImGui instance or an ABI-layout guess.

`draw()` is a **template** parameterized on the draw-list type rather than a concrete
`ImDrawList&`, which is what keeps even the C++ fluent header ImGui-dependency-free. It calls
`PrimReserve`/`PrimWriteVtx`/`PrimWriteIdx` generically, resolving against a real `ImDrawList`
or against a duck-typed mock (there is a compile-check test that does exactly that).

Writing through those public prim methods is deliberate. `ImDrawList`'s public *fields* are
stable to read, but its *invariants* — write-pointer bookkeeping, texture and clip-rect
stacking, large-mesh vertex-offset handling — are maintained by methods like `PrimReserve`, are
not covered by any ABI guarantee, and have changed across Dear ImGui versions.

## Using it from C++

There is no build system yet, so add the sources to your own build:

```
capi/imgui_painter_c.cpp
src/painter.cpp
```

with `capi/` and `include/` on the include path, compiled as **C++17**. Then
`#include "imgui_painter.h"`.

Two host values must be supplied — neither can be guessed safely, and both are sampled
automatically only by the Rust binding today:

| Value | Where it comes from | Why it matters |
|---|---|---|
| `white_pixel_uv` | `ImGui::GetFontTexUvWhitePixel()` | Flat and gradient fills sample your atlas's solid-white texel. A wrong UV samples the wrong texel with no visible error. |
| `pixel_scale` | `ImGui::GetIO().DisplayFramebufferScale.x` | Sub-pixel hairlines are drawn at one device pixel with proportionally reduced alpha. Skipping it blurs bevels on HiDPI. |

`ip_begin` resets the pixel scale to `1.0`, so set it *after* constructing the `Painter` — the
fluent `.pixel_scale()` call above is already in the right place.

`ip::Painter` is **single-use**: it creates and destroys one `ip_ctx` per instance, so it is
one object per shape. A reusable per-frame context is a known gap — see
[Project status](#project-status).

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

| | C++ | Rust |
|---|---|---|
| Painting core (shapes, gradients, shadows, borders, bands, lines) | ✅ | ✅ |
| Fluent chaining API | ✅ | ✅ |
| Build system | ❌ none | ✅ Cargo |
| Reusable per-frame context | ❌ one context per shape | ✅ `Painter` → `Frame` → `Canvas` |
| Host values sampled automatically | ❌ caller supplies | ✅ |
| **Widget decoration** (restyle a stock `ImGui::Button`) | ❌ | ✅ |
| Palette / recipes | ❌ | ✅ |
| Examples and visual demo | ❌ | ✅ |
| Tests | via the Rust binding | ✅ |

The Rust binding is further along because it was the first consumer and the layer that proved
the design. That is a historical accident, not the intended end state: the C ABI exists
specifically so C++, C, Zig, C#, and Python bindings can sit on equal footing. What C++ needs
in order to catch up is written up in **[docs/cpp-parity.md](docs/cpp-parity.md)**.

### Compatibility

The painting core is independent of any particular Dear ImGui version.

The **decorators** (Rust-only today) are not: they reconstruct stock widget chrome geometry,
which is internal Dear ImGui detail that upstream is entitled to change in any release. They
target Dear ImGui **1.91.9b** via imgui-rs 0.12 fork rev
[`7a89260`](https://github.com/samanshaiza004/imgui-rs). A source-compatible ImGui bump can
compile cleanly while silently moving the widget away from the painted rectangle, so
[`VERIFIED_IMGUI_SYS`](VERIFIED_IMGUI_SYS) records the revision a human last visually verified
and CI fails when the resolved revision drifts from it. See the
[dependency-bump checklist](CONTRIBUTING.md#dependency-bump-checklist).

## Documentation

- **[The book](https://samanshaiza004.github.io/imgui-painter/)** — concepts, the C ABI,
  decorator anatomy, and recipes.
- **[C++ parity plan](docs/cpp-parity.md)** — what C++ needs to match the Rust binding.
- **[API reference](https://samanshaiza004.github.io/imgui-painter/api/imgui_painter/)** —
  rustdoc for the Rust binding.
- **[CONTRIBUTING.md](CONTRIBUTING.md)** — quality bar, visual gate, dependency-bump checklist.
- **[CHANGELOG.md](CHANGELOG.md)** — phase-by-phase development history.

## Repo layout

```
include/imgui_painter.h   header-only C++ fluent wrapper over capi/
capi/imgui_painter_c.h    the C ABI — every language binding compiles against this
src/painter.cpp           the core: tessellation, gradients, shadows, borders
bindings/rust/            the Rust binding (+ the widget decoration layer)
book/                     prose documentation (mdBook)
docs/                     design findings, parity plan, case studies, screenshots
```

## Testing

The core's tests currently run through the Rust binding (`cargo test`), which also
compile-checks `include/imgui_painter.h` against a mock draw list to catch header rot. A native
C++ test target is part of the [parity plan](docs/cpp-parity.md).

Automated tests cover mesh geometry, lifecycle cleanup, composition invariants, and a
zero-allocation steady state — not final rasterized appearance, which is what the human visual
gate in [CONTRIBUTING.md](CONTRIBUTING.md#the-visual-gate) is for.

## License

Dual-licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE))
- MIT license ([LICENSE-MIT](LICENSE-MIT))

at your option.
