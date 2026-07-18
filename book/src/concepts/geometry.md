# Geometry and composition

Painting is a sequence of operations against one shape. In C++, the `Painter`
constructor sets that shape; chained calls then apply fills, shadows, and
borders to it, in order.

```cpp
ip::Painter({uv.x, uv.y}, rect, radius)
    .pixel_scale(scale)
    .shadow(outer_shadow)
    .fill(surface)
    .border(outer_border)
    .draw(*ImGui::GetWindowDrawList());
```

## Shapes

The rounded rectangle passed to `ip::Painter` is the workhorse. A radius of
`0.0` gives a plain rectangle, so there is no separate shape type for it. At the
C ABI level the same operation is `ip_rounded_rect(ctx, rect, radius)`.

Tessellation is **adaptive and error-bounded**: the segment count per corner is
derived from the corner's actual radius against a flatness tolerance, rather than
being a fixed constant. A 2px corner does not get the same 8 segments as a 20px
corner. This produces smaller meshes than a fixed-count approach at equal or
better visual quality — see [Benchmarks](../appendix/benchmarks.md).

## Fills

- `.fill(color)` — flat fill.
- `.fill(gradient)` — a multi-stop gradient in one of four modes:
  `IP_GRADIENT_LINEAR`, `_RADIAL`, `_ANGULAR`, and `_DIAMOND`. Stops are
  `ip_color_stop` values whose `t` fields run from `0.0` to `1.0`.
- `.band(from_y, to_y, color)` and `.band(from_y, to_y, gradient)` — fills
  clipped to a horizontal band, still respecting the shape's rounded corners.

Bands are what make gloss and bevel effects possible without a second shape. A
gloss highlight across the top third of a button is a band, not a separate
rounded rect that you have to keep aligned with the first one.

## Shadows

`.shadow(shadow)` takes an `ip_shadow` containing offset, blur, spread, color,
and an `inset` flag.

- **Outer** shadows (`inset: false`) read as elevation.
- **Inset** shadows (`inset: true`) read as recession, and are what makes a
  pressed state look genuinely pushed in rather than merely darker.

Shadows stack. Calling `.shadow()` twice applies both, in order.

## Borders

- `.border(border)` — a border on the current shape's edge.
- `.border(distance, border)` — a border inset by `distance`, which is how you
  stack multiple visually distinct outlines.

Borders honor **hairline alpha compensation**. A border thinner than one physical
pixel cannot be drawn thinner, so instead its alpha is scaled proportionally. A
0.5px border becomes a 1px border at half alpha, which is visually what a
sub-pixel line should look like — and, importantly, it stays consistent across
display scales.

## Device pixels

```cpp
const float scale = ImGui::GetIO().DisplayFramebufferScale.x;
const float hairline = 1.0f / scale;

ip::Painter({uv.x, uv.y}, rect, radius)
    .pixel_scale(scale)
    .band(rect.min.y + hairline, rect.min.y + hairline * 2.0f, highlight)
    .draw(*ImGui::GetWindowDrawList());
```

`1.0f / scale` is one physical pixel in logical units. Use it anywhere you mean
"the thinnest crisp line", rather than hard-coding `1.0` — on a 2× display,
`1.0` logical unit is two physical pixels, and a bevel drawn that way looks
twice as heavy as intended. The Rust `Canvas` exposes the same calculation as
`device_pixel()`.

## Composition is ordered, not declarative

The full layered-chrome idiom looks like this:

```cpp
ip::Painter({uv.x, uv.y}, rect, radius)
    .pixel_scale(scale)
    .shadow(outer_shadow)                         // elevation
    .fill(surface)                                // base
    .band(top, gloss_end, gloss)                  // gloss
    .band(top, top + hairline, highlight)         // bevel
    .shadow(inset_shadow)                         // depth
    .border(outer_border)                         // outline
    .border(hairline, inner_border)               // inner outline
    .draw(*ImGui::GetWindowDrawList());
```

Every line is a draw operation whose effect depends on the ones before it. That
is the whole model. There is no state to push and pop, no cascade to resolve, and
nothing that happens implicitly between two adjacent calls — which is what makes
this readable at all compared to the equivalent hand-written geometry.
