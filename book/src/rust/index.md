# The Rust binding

`bindings/rust` is imgui-painter's most complete binding today. Alongside the painting core it
carries two layers that do not exist in C++ yet:

- **Widget decoration** — restyling a stock `ImGui::Button`, `Checkbox`, `Slider`, and friends.
- **Recipes and palettes** — turning a handful of color tokens into materials and painted
  surfaces.

That is a historical accident rather than a design preference: Rust was the first consumer and
the layer that proved the design. What C++ needs to catch up is written up in the
[parity plan](https://github.com/samanshaiza004/imgui-painter/blob/main/docs/cpp-parity.md).

## Setup

The crate is not on crates.io, so depend on it by git:

```toml
[dependencies]
imgui-painter = { git = "https://github.com/samanshaiza004/imgui-painter", rev = "..." }
```

If you use the decorators, your **workspace root** must also pin the imgui-rs fork providing
the supported Dear ImGui version:

```toml
[patch.crates-io]
imgui = { git = "https://github.com/samanshaiza004/imgui-rs", rev = "7a89260c79ad1f9d4bfe81d6ca1b76ad38a6b3e3" }
imgui-sys = { git = "https://github.com/samanshaiza004/imgui-rs", rev = "7a89260c79ad1f9d4bfe81d6ca1b76ad38a6b3e3" }
```

Cargo ignores `[patch]` declared by a dependency, so imgui-painter cannot apply this for you.
See [the compatibility contract](../decorators/contract.md) for why the pin exists.

Then confirm you have exactly one ImGui:

```sh
cargo tree -d | grep imgui-sys
```

Two `imgui-sys` builds means two Dear ImGui instances — garbage rendering or a crash. It is
the most important thing to check when integrating.

## The ownership chain

Unlike the C++ `ip::Painter`, which owns one native context per shape, the Rust binding reuses
a single context all frame:

```
Painter   long-lived    owns the native painter context
   └─ Frame   per frame    samples white-pixel UV + framebuffer scale
        └─ Canvas  per draw list   the thing you actually paint with
```

```rust
let mut frame = painter.begin_frame();
unsafe {
    let dl = imgui::sys::igGetWindowDrawList();
    let mut canvas = frame.canvas(dl);
    canvas.rounded_rect(rect, 3.0);
    canvas.fill_color(rgba(40, 44, 52, 255));
}
```

`begin_frame()` samples `igGetFontTexUvWhitePixel()` and `io.DisplayFramebufferScale.x`
automatically — the two values a C++ caller currently has to supply by hand.

`canvas.device_pixel()` gives one physical pixel in logical units; use it for hairlines rather
than hard-coding `1.0`.

## Decorating a widget

```rust
use imgui_painter::{decorate_button, rgba, Border, Material, StateColors};

let material = Material {
    radius: 5.0,
    fill: StateColors {
        base: rgba(45, 108, 223, 255),
        hover: rgba(62, 128, 240, 255),
        active: rgba(35, 88, 190, 255),
    },
    border: Border { thickness: 1.0, color: rgba(255, 255, 255, 48) },
    shadow: None,
};

let clicked = unsafe {
    decorate_button(&mut frame, &material, || ui.button("Save"))
};
```

The call returns whatever the closure returns, so normal `if ui.button(..)` control flow still
works.

### Why `unsafe`

The decorators call into `imgui-sys` and require:

1. a current ImGui frame and window, and
2. a closure that submits **exactly one** stock widget item.

Submitting zero or two items breaks the channel-split and chrome-capture logic. That is a real
precondition, not a formality, which is why it is `unsafe` rather than fallible.

## Zero allocations in the steady state

Once running, the Rust side of a frame allocates nothing — meshes are written into reused
buffers. This is enforced by `bindings/rust/tests/zero_alloc.rs`, which installs a
process-wide counting allocator and asserts a zero steady-state per-frame count.

## Next

- [How decoration works](../decorators/index.md) — the bracket, and what it replaces.
- [The compatibility contract](../decorators/contract.md) — the last-item guarantee and the
  ImGui version pin.
- [Widget notes](../decorators/widgets.md) — per-widget behavior and gotchas.
- [Recipes and palettes](../recipes.md).
