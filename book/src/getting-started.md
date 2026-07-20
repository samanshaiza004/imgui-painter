# Getting started

This page covers C++. For the Rust binding, see [the Rust overview](rust/index.md); both
bindings expose the same feature set.

## Requirements

- A **C++17** compiler.
- Dear ImGui already integrated into your application.

## Adding it to your build

The library builds with CMake (3.16+). Fetch it:

```cmake
include(FetchContent)
FetchContent_Declare(imgui-painter
    GIT_REPOSITORY https://github.com/samanshaiza004/imgui-painter.git
    GIT_TAG        v0.1.1
)
FetchContent_MakeAvailable(imgui-painter)
target_link_libraries(your_app PRIVATE imgui_painter::imgui_painter)
```

or install it and use `find_package(imgui-painter REQUIRED)` against the same target. Then:

```cpp
#include "imgui_painter.h"
```

If you would rather not use CMake, adding `capi/imgui_painter_c.cpp` and `src/painter.cpp` to
your own build with `capi/` and `include/` on the include path still works.

The core compiles standalone — it includes no ImGui header — so it does not care which Dear
ImGui version, backend, or build flags your application uses. That changes only if you opt into
`imgui_painter_imgui.h` or `imgui_painter_decorators.h`, which do include `imgui.h`.

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

Include `imgui_painter_decorators.h` and wrap a stock widget in a lambda:

```cpp
#include "imgui_painter_decorators.h"

ip::Context ctx;                                  // long-lived
// ...per frame:
auto frame = ip::begin_frame(ctx);
const ip::Material material = ip::raised_button(palette);

bool clicked = ip::decorate_button(frame, material, [] {
    return ImGui::Button("Save");
});
```

That is a real `ImGui::Button()`. It keeps its ID, layout, input handling, keyboard navigation,
and return value — imgui-painter suppresses only the `ImGuiCol_` roles it replaces and paints
behind it. All seven decorators (Button, Selectable, Checkbox, InputText, Slider, Combo,
TreeNode) follow the same shape.

The geometry these reconstruct is **not** an upstream Dear ImGui contract, so the C++ header
pins to an exact version with a `static_assert`. See
[the compatibility contract](decorators/contract.md) and
[widget anatomy](https://github.com/samanshaiza004/imgui-painter/blob/main/docs/widget-anatomy.md).

Painting panels, strips, wells, and backgrounds behind stock widgets works the same way — paint
the surface first, then submit the widgets that sit on it.

## Performance note

`ip::Painter` creates and destroys one native context per instance, so a frame with many painted
elements creates many. That is fine at panel-and-strip scale and wasteful per-row in a long list.

Use `ip::Context` instead when painting more than a couple of elements per frame: it owns one
native context, hands out a `Frame` per frame and a `Canvas` per draw list, and a `Canvas`
accumulates every shape drawn on it and submits once when it goes out of scope.
