# Introduction

imgui-painter is a rendering and styling toolkit for [Dear ImGui](https://github.com/ocornut/imgui),
written in C++. It aims to make high-quality visuals as easy as `PushStyleColor`, without
replacing ImGui's widget, layout, or input systems.

## The problem

Dear ImGui's style system covers colors, rounding, spacing, and border sizes. Past that,
styling means writing `ImDrawList` geometry by hand.

Hand-written geometry does not compose. Consider one moderately rich button: a drop shadow, a
multi-stop base gradient, a translucent gloss band across the top third, a one-physical-pixel
bevel highlight, an inset shadow so the pressed state reads as recessed, and two borders of
different colors. Each of those is easy alone. Together they become an ordering problem, a set
of magic numbers, and a block of code that has to be rewritten — and re-debugged — for the
next widget.

Then the display scale changes and the hairlines go blurry.

## The approach

imgui-painter keeps the composition explicit and ordered, without introducing a styling
language:

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

Operations stack in the order you write them. There is no cascade, no selector engine, and no
inheritance to reason about — the reason that snippet is readable is precisely that it is not
a stylesheet.

## Non-goals

Being clear about what this is not is most of what keeps it small:

- **Not a widget library.** It adds no widgets. It restyles the ones ImGui has.
- **Not a design system or theme engine.** It has no opinion about your palette beyond an
  optional recipe helper.
- **Not CSS.** No cascade, no selectors, no stylesheet parsing.
- **It does not own layout, input, navigation, text, or popup/tree state.** ImGui keeps all of
  it. imgui-painter never renders text; hosts style typography through stock ImGui style APIs.

## How it is layered

The bottom layer is a C++ core with **zero** Dear ImGui dependency: pure math in, a generic
vertex/index mesh out. Above it sit a C ABI and, on top of that, per-language bindings — the
header-only C++ fluent wrapper, and the Rust binding.

See [Architecture](concepts/architecture.md) for why that split exists and what it buys.

## Decorating stock widgets

The most distinctive thing the toolkit does is restyle a **stock** widget in place:

```rust
// Rust binding
decorate_button(&mut frame, &material, || ui.button("Save"));
```

That is a real `ImGui::Button()`. It keeps its ID, layout, input handling, keyboard
navigation, and return value; imgui-painter suppresses only the chrome it replaces and paints
behind it.

> **This layer exists only in the Rust binding today.** The C++ side has the painting core and
> the fluent wrapper, but no decorators yet. Everything required to build them is public Dear
> ImGui C++ API, so it is pending work rather than a technical obstacle — see the
> [parity plan](https://github.com/samanshaiza004/imgui-painter/blob/main/docs/cpp-parity.md).
>
> The Rust binding got ahead because it was the first consumer and the layer that proved the
> design, not because the design favors it. The C ABI exists so every binding can sit on equal
> footing.

## Where to go next

- [Getting started](getting-started.md) — C++ setup and your first painted shape.
- [Architecture](concepts/architecture.md) — the core/binding split and its rationale.
- [The C ABI](c-abi.md) — the surface every binding compiles against.
- [The Rust binding](rust/index.md) — decorators, recipes, and the compatibility contract.
