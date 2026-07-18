# Getting started

This page covers C++. For the Rust binding — which is where widget decoration currently
lives — see [the Rust overview](rust/index.md).

## Requirements

- A **C++17** compiler.
- Dear ImGui already integrated into your application.

## Adding it to your build

There is no build system in the repo yet (it's the first item on the
[parity plan](https://github.com/samanshaiza004/imgui-painter/blob/main/docs/cpp-parity.md)),
so add the two core sources to your own build:

```
capi/imgui_painter_c.cpp
src/painter.cpp
```

with `capi/` and `include/` on your include path. Then:

```cpp
#include "imgui_painter.h"
```

The core compiles standalone — it includes no ImGui header — so it does not care which Dear
ImGui version, backend, or build flags your application uses.

## Your first painted shape

```cpp
ImDrawList* dl = ImGui::GetWindowDrawList();
const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();
const ImVec2 p  = ImGui::GetCursorScreenPos();

const ip_rect rect = {{p.x, p.y}, {p.x + 200.0f, p.y + 32.0f}};
const ip_border border = {1.0f, IM_COL32(0, 0, 0, 90)};

ip::Painter({uv.x, uv.y}, rect, 5.0f)
    .pixel_scale(ImGui::GetIO().DisplayFramebufferScale.x)
    .fill(IM_COL32(48, 52, 62, 255))
    .border(border)
    .draw(*dl);

ImGui::Dummy({200.0f, 32.0f});   // reserve the layout space you painted into
```

`ip_color` uses the same packing as `ImU32`, so `IM_COL32` values pass straight through.

### The two host values

Both must be supplied by you, and both fail *silently* if you get them wrong:

| Value | Source | Failure mode if wrong |
|---|---|---|
| `white_pixel_uv` | `ImGui::GetFontTexUvWhitePixel()` | Samples the wrong texel in your atlas — no error, just wrong output |
| `pixel_scale` | `ImGui::GetIO().DisplayFramebufferScale.x` | Sub-pixel hairlines blur on HiDPI displays |

Order matters: the `Painter` constructor calls `ip_begin`, which **resets the pixel scale to
1.0**. So `.pixel_scale()` has to come after construction — which the chained form above
naturally does.

## Composing something richer

The point of the library is that layers stack in call order:

```cpp
const float scale = ImGui::GetIO().DisplayFramebufferScale.x;
const float hairline = 1.0f / scale;

ip::Painter painter({uv.x, uv.y}, rect, 5.0f);
painter.pixel_scale(scale)
    .shadow(drop)                              // elevation, behind everything
    .fill(surface)                             // multi-stop base gradient
    .band(rect.min.y, gloss_end, gloss)        // translucent gloss over the top
    .band(rect.min.y, rect.min.y + hairline,   // one-device-pixel bevel
          IM_COL32(255, 255, 255, 40))
    .shadow(inset)                             // recessed depth
    .border(outer)
    .border(hairline, inner);                  // a second, inset outline
painter.draw(*dl);
```

Read that top to bottom and you have the paint order. There is no z-index, no stylesheet, and
nothing implicit between two adjacent calls.

## Gradients

```cpp
const ip_color_stop stops[] = {
    {0.0f, IM_COL32(72, 78, 92, 255)},
    {0.5f, IM_COL32(58, 63, 75, 255)},
    {1.0f, IM_COL32(44, 48, 58, 255)},
};

const ip_gradient surface = {
    IP_GRADIENT_LINEAR,
    {rect.min.x, rect.min.y},    // from
    {rect.min.x, rect.max.y},    // to — a vertical axis
    stops,
    3,
};
```

Stop `t` values must ascend. Modes are `IP_GRADIENT_LINEAR`, `_RADIAL`, `_ANGULAR`, and
`_DIAMOND`; for radial, `from` is the center and `to` sets the radius.

## Decorating stock widgets

Not available from C++ yet. The layer that restyles a live `ImGui::Button()` — suppressing its
chrome, splitting the draw list, and painting behind it — currently exists only in the
[Rust binding](rust/index.md).

Everything needed to build it *is* public Dear ImGui C++ API (`ImDrawList::ChannelsSplit`,
`PushStyleColor`, `IsItemHovered`/`IsItemActive`, `GetItemRectMin`/`Max`), so this is work not
yet done rather than a technical obstacle. The plan is in
[docs/cpp-parity.md](https://github.com/samanshaiza004/imgui-painter/blob/main/docs/cpp-parity.md).

In the meantime, painting panels, strips, wells, and backgrounds behind stock widgets works
today — paint the surface first, then submit the widgets that sit on it.

## Performance note

`ip::Painter` creates and destroys one native context per instance, so a frame with many
painted elements creates many. That is fine at panel-and-strip scale and wasteful per-row in a
long list. A reusable per-frame context is the second item on the parity plan.
