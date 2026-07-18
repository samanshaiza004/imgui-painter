# Painting sessions

Every painted element follows the same native lifecycle:

```
ip_ctx_create
   └─ ip_begin          open one element and reset its mesh and pixel scale
        ├─ shape and paint operations
        └─ ip_end       return the completed mesh
   └─ ip_ctx_destroy
```

The context owns the mesh buffers. `ip_end` returns an `ip_mesh` by value, but
its vertex and index pointers remain owned by the context and are valid only
until the next `ip_begin` or `ip_ctx_destroy`. A binding therefore copies that
mesh into its host draw list before opening another session.

`ip_begin(ctx, white_pixel_uv)` also resets pixel scale to `1.0`, so direct C ABI
callers must call `ip_set_pixel_scale(ctx, scale)` **after** every begin.

## C++: one context per shape

The C++ fluent wrapper makes one painting session an expression:

```cpp
const ImVec2 uv = ImGui::GetFontTexUvWhitePixel();

ip::Painter({uv.x, uv.y}, rect, radius)
    .pixel_scale(ImGui::GetIO().DisplayFramebufferScale.x)
    .shadow(outer_shadow)
    .fill(surface)
    .border(outer_border)
    .draw(*ImGui::GetWindowDrawList());
```

`ip::Painter` is single-use and non-copyable. Its constructor creates a native
context and calls `ip_begin`; its destructor destroys that context. Create one
instance per shape, and supply both host values yourself: the white-pixel UV in
the constructor and framebuffer scale through `.pixel_scale()`.

That ownership is simple, but it means C++ creates and destroys a context for
every painted shape. A reusable C++ context is an explicit
[parity gap](https://github.com/samanshaiza004/imgui-painter/blob/main/docs/cpp-parity.md).

## Rust: Painter, Frame, Canvas

The Rust binding reuses one native context for the whole frame through a
three-level ownership chain:

```
Painter   long-lived    owns the native painter context
   └─ Frame   per frame    samples host values
        └─ Canvas  per draw list   the thing you actually paint with
```

```rust
let mut frame = painter.begin_frame();
let draw_list = imgui::sys::igGetWindowDrawList();
let mut canvas = frame.canvas(draw_list);
```

Create `Painter` once and keep it for the life of the application. `begin_frame()`
samples the current white-pixel UV and framebuffer scale automatically. A
`Canvas` then binds that frame to one specific `ImDrawList`; painting into a
different window means getting a different `Canvas`.

`Frame` is borrowed mutably by decorators, which is why hosts with deep call
trees end up threading `&mut Frame` through their draw functions.

`canvas.device_pixel()` returns the size of one physical pixel in logical units —
use it for hairlines and bevels rather than hard-coding `1.0`.

## Draw order is submission order

Immediate mode has no z-index. Whatever you submit first is behind whatever you
submit next.

For a panel with widgets on it, that means:

1. Paint the panel surface with a `Canvas`.
2. *Then* submit the widgets.

Hosts building nested surfaces (a window containing panes containing rows) end up
painting parent rectangles first, then submitting transparent child regions. That
ordering requirement is real and is the main structural thing to plan for when
adopting the toolkit.

## Zero allocations in the steady state

This guarantee is specific to the Rust binding. Once running, the Rust side of a
frame allocates nothing. Meshes are written into reused buffers and copied into
the draw list; there is no per-frame `Vec` churn behind the painting API.

This is enforced, not aspirational: `bindings/rust/tests/zero_alloc.rs` installs a
process-wide counting global allocator and asserts a zero steady-state
allocation count per frame. If a change introduces a per-frame allocation, that
test fails.

The caveats are documented in the test itself — it measures the painting path's
steady state, not application code that happens to allocate its own labels.
