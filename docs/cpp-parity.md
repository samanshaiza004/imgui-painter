# C++ parity design record

> **This work is complete.** Shipped in 0.1.0; C++ and Rust are at feature parity. This is a
> historical design record, not a roadmap — nothing below is outstanding.
>
> All seven items landed: the CMake build, the reusable per-frame context, automatic host-value
> sampling, palette and recipes, GLFW + OpenGL3 examples, all seven widget decorators, and native
> C++ tests (40 geometry + 15 decorator, under CTest).
>
> Two things outlived the plan and are the live documents to read instead:
> [widget-anatomy.md](widget-anatomy.md), the chrome-geometry spec both bindings implement, and
> the [CHANGELOG](../CHANGELOG.md) entry for 0.1.0. The one open question below — whether C++ or
> Rust is the reference binding going forward — was not settled by this work.
>
> The filename stays put because the 0.1.0 release notes and the book both link to it.

It is kept because the reasoning is worth preserving: what the gap actually was, and why each
choice was made the way it was.

This exists because the Rust binding got ahead by accident of history — it was the first
consumer and the layer that proved the design — while Dear ImGui's own audience is
overwhelmingly C++. The C ABI was built so every binding could sit on equal footing; this
document is the list of things standing between that intent and reality.

## Current state

| Capability | C++ | Rust | Landed in |
|---|---|---|---|
| Painting core | ✅ | ✅ | — |
| Fluent chaining | ✅ | ✅ | — |
| Build system | ✅ | ✅ | `75f3342` CMake |
| Reusable per-frame context | ✅ | ✅ | `d61ffae` `Context`/`Frame`/`Canvas` |
| Automatic host-value sampling | ✅ | ✅ | `bd15ad1` `imgui_painter_imgui.h` |
| Widget decoration | ✅ | ✅ | `938fb4f` `bfb3a1b` `943ed8f` `40fe797` |
| Palette / recipes | ✅ | ✅ | `5a52ddb` `imgui_painter_recipes.h` |
| Examples / visual demo | ✅ | ✅ | `0d73264` GLFW + OpenGL3 |
| Native tests | ◐ | ✅ | decorators covered; core geometry pending |

Good news up front: **no gap below requires changing the C++ core or the C ABI.** Everything
missing is a layer above them. The core already supports what's needed — for instance
`ip_begin` explicitly resets and reuses a context, so per-frame reuse is a wrapper concern,
not a core one.

---

## 1. Build system — the actual blocker

**Status:** there is no `CMakeLists.txt`, `Makefile`, or any other build definition. The only
way the core gets compiled today is `bindings/rust/build.rs` driving the `cc` crate.

A C++ user cannot build this library at all without hand-rolling their own compile of
`src/painter.cpp` and `capi/imgui_painter_c.cpp`. Everything else in this document is
downstream of fixing that.

**What it takes:** a `CMakeLists.txt` at the repo root exporting an `imgui_painter` target
(C++17, sources `src/painter.cpp` + `capi/imgui_painter_c.cpp`, public include dirs `capi/`
and `include/`). Static by default; `BUILD_SHARED_LIBS` respected. An installed target with a
`imgui-painterConfig.cmake` so consumers can `find_package`. `FetchContent` should also work,
since that's how much of the Dear ImGui ecosystem consumes dependencies.

Worth deciding: whether CMake becomes the source of truth and `build.rs` shells out to it, or
whether the two build definitions stay independent and CI proves they agree. Independent is
simpler and keeps the Rust crate installable without CMake on the machine — at the cost of the
source list being written twice. Recommend independent, with a CI job that builds both.

**Effort:** small. **Unblocks:** literally everything else.

---

## 2. Reusable per-frame context

**Status:** `ip::Painter`'s constructor calls `ip_ctx_create()` and its destructor calls
`ip_ctx_destroy()`. It's single-use by design ("construct one per shape/frame element and call
`draw()` once"), so a frame drawing 40 elements creates and destroys 40 native contexts.

Rust instead has a three-level chain — a long-lived `Painter` owning one context, a per-frame
`Frame`, and a per-draw-list `Canvas` — and reuses that one context all frame.

**What it takes:** a C++ equivalent of that chain. The C ABI already supports it; this is
purely a matter of adding types that don't create a context per shape. Roughly:

```cpp
ip::Context ctx;                                  // long-lived, owns one ip_ctx
auto frame = ctx.begin_frame(white_uv, scale);    // per frame
frame.shape(rect, radius)
     .fill(gradient)
     .draw(*ImGui::GetWindowDrawList());          // per element
```

The existing single-use `Painter` can stay as the convenience path for one-off shapes.

**Constraint to preserve:** the mesh returned by `ip_end` is owned by the context and is
invalidated by the next `ip_begin`. A reusable context must therefore copy each element's mesh
into the draw list before starting the next element — which is what `draw()` already does, so
the ordering is naturally correct as long as the API doesn't let a caller hold a mesh across
elements.

**Effort:** small-to-medium. **Depends on:** nothing.

---

## 3. Automatic host-value sampling

**Status:** C++ callers must pass `white_pixel_uv` and `pixel_scale` by hand. Rust's
`begin_frame()` samples both automatically — `igGetFontTexUvWhitePixel()` and
`io.DisplayFramebufferScale.x`.

Both are easy to get wrong in ways that produce *no error*: a wrong white-pixel UV silently
samples the wrong texel, and a missing pixel scale silently blurs hairlines on HiDPI.

**What it takes:** an **opt-in, ImGui-aware header** — say `include/imgui_painter_imgui.h` —
that does include `imgui.h` and offers a `begin_frame()` overload sampling both values. This
must be a *separate* header, because `include/imgui_painter.h` is deliberately ImGui-free and
that property is load-bearing (it's what lets the template `draw()` compile against a mock).

**Effort:** trivial once (2) exists. **Depends on:** 2.

---

## 4. Widget decoration — the headline feature

**Status:** absent from C++ entirely. This is the feature the library leads with — restyling a
*stock* `ImGui::Button()` with no wrapper widget — and today it exists only in Rust.

**The encouraging part:** every primitive the decorator bracket needs is public C++ Dear ImGui
API, and several are *more* direct from C++ than through Rust bindings:

| Decorator step | C++ API |
|---|---|
| Split the draw list into channels | `ImDrawList::ChannelsSplit` / `ChannelsSetCurrent` / `ChannelsMerge` |
| Suppress the widget's own chrome | `ImGui::PushStyleColor(ImGuiCol_Button, transparent)` etc. |
| Capture item state after submission | `IsItemHovered`, `IsItemActive`, `GetItemRectMin` / `GetItemRectMax` |
| Reconstruct chrome geometry | `GetStyle()`, `GetFrameHeight()`, `CalcTextSize()`, optionally `imgui_internal.h` |

A C++ decorator can take the widget as a lambda, which reads about as well as the Rust
closure form:

```cpp
bool clicked = ip::decorate_button(frame, material, []{
    return ImGui::Button("Save");
});
```

**What it takes**, per widget: the chrome-rectangle formula, the set of `ImGuiCol_` slots to
suppress, and the state→color resolution. All of that already exists, worked out and tested,
in `bindings/rust/src/item_paint.rs` — the C++ port is a translation of known-good logic
rather than a fresh derivation. Start with Button and Selectable (chrome rect == item rect),
then Checkbox and InputText (chrome rect ≠ item rect, since the label must be excluded), then
Slider/Combo/TreeNode (multi-part).

**Two things to preserve**, both of which are contracts the Rust side established:

1. **Last-item preservation.** After the bracket returns, the widget must still be ImGui's last
   item — ID, bounds, hover, active, drag/drop all intact — so callers can attach tooltips and
   context menus immediately after. Port the regression test alongside the feature.
2. **The version pin.** Reconstructed geometry tracks internal ImGui layout, so the C++ layer
   needs the same `VERIFIED_IMGUI_SYS`-style gate. A C++ decorator layer will pin to a Dear
   ImGui version the same way, and should say so as loudly.

**Architectural note:** this layer depends on Dear ImGui by definition, so it belongs in the
opt-in ImGui-aware header from (3), never in the core or the ImGui-free fluent header.

**Effort:** the largest item here, but incremental — it lands widget by widget, each
independently useful. **Depends on:** 2, 3.

---

## 5. Palette and recipes

**Status:** Rust has a 9-token `Palette`, a family of material builders (`raised_button`,
`toolbar_button`, `inset_control`, `selected_row`, `parameter_slider`, `combo_field`,
`panel`, `inset_panel`), and `apply_imgui_colors`, which maps the palette across every stock
ImGui color role.

This is almost entirely **pure data and arithmetic** — color mixing, shading, tinting — so the
C++ port is mechanical and needs no design work.

`apply_imgui_colors` is the exception worth calling out: it's the highest-value piece for a
C++ user (without it, a host maintains a second hand-written palette for everything ImGui
still draws itself, and the two drift), and it's also the piece that pins to an ImGui version
at *compile* time, because it names color roles like `ImGuiCol_NavCursor` and `ImGuiCol_TextLink`
that don't exist in older releases.

**Effort:** small. **Depends on:** 1. Can land before decorators; `apply_imgui_colors` is
useful on its own.

---

## 6. Examples and the visual demo

**Status:** `painter_demo` — the visual gate for every change — is a Rust binary using
wgpu/winit. There is no C++ example at all.

This matters beyond convenience: CONTRIBUTING requires a human visual pass at 1×/1.5×/2× for
any change touching widget chrome. A C++ decorator layer with no C++ demo has no way to run
that gate on its own rendering path.

**What it takes:** a C++ example against a conventional Dear ImGui backend pair — GLFW +
OpenGL3 is the most familiar to that audience and the easiest to build everywhere. Minimum: a
"basic" example mirroring the Rust one. Ideally, eventually, a C++ `painter_demo` covering the
same looks so the visual gate can run natively.

**Effort:** medium (mostly backend boilerplate). **Depends on:** 1.

---

## 7. Native C++ tests

**Status:** the core's ~70 tests all run through the Rust binding. The only native C++ check is
a compile-check of the fluent header against a mock draw list.

The core is C++; its tests running exclusively through a Rust harness means a C++-only
contributor cannot verify their own change.

**What it takes:** a native test target (Catch2 or doctest — both single-header and
FetchContent-friendly) covering the geometry invariants the Rust tests already assert:
tessellation, gradient modes, band clipping, shadow stacking, hairline alpha compensation,
degenerate/NaN safety.

Some duplication with the Rust tests is fine and arguably correct — they're testing the same
core through two different bindings, which is exactly the property worth protecting.

**Effort:** medium. **Depends on:** 1.

---

## Recommended order

1. **CMake build** (1) — nothing else is reachable until this exists.
2. **Reusable context** (2) + **auto host-value sampling** (3) — small, and everything after
   assumes them.
3. **Palette/recipes** (5) — mechanical, high value on its own, no dependency on decorators.
4. **A C++ example** (6) — proves 1–3 actually work for a real consumer, and gives the visual
   gate somewhere to run.
5. **Decorators** (4), widget by widget — Button and Selectable first.
6. **Native tests** (7) — can start any time after 1; do it before decorators if you want the
   geometry invariants locked down natively first.

## Decisions worth making before starting

- **Does CMake or `build.rs` own the source list?** Recommend keeping them independent with a
  CI job building both; revisit if they drift.
- **Do the C++ and Rust decorator layers share their anatomy formulas?** They can't share code
  across the FFI boundary — the formulas need ImGui's own layout state. Accept two
  implementations, and keep one document describing the anatomy so they can't drift silently.
- **Is C++ or Rust the reference binding going forward?** Today Rust is, de facto. If C++
  becomes the reference, the visual gate should eventually move to a C++ demo, and the Rust
  binding becomes a consumer of a spec rather than the definition of one.
