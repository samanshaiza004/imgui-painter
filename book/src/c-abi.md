# The C ABI

`capi/imgui_painter_c.h` is the surface every language binding compiles against. The C++
fluent header wraps it, and so does the Rust binding — neither is privileged.

Because the core links no Dear ImGui and no cimgui, a binding needs only a C FFI and a way to
hand the resulting mesh to whatever draw list its host uses.

## A complete session

```c
ip_ctx *ctx = ip_ctx_create();

ip_begin(ctx, white_pixel_uv);     /* resets the mesh AND the pixel scale */
ip_set_pixel_scale(ctx, 2.0f);     /* so this must come after ip_begin */

ip_rounded_rect(ctx, rect, 5.0f);
ip_add_shadow(ctx, &shadow);       /* before the fill, so it lands behind */
ip_fill_gradient(ctx, &gradient);
ip_add_border(ctx, &border);

const ip_mesh mesh = ip_end(ctx);
/* copy mesh.vtx / mesh.idx into your host draw list */

ip_ctx_destroy(ctx);
```

Three things that are easy to get wrong:

1. **`ip_begin` takes the white-pixel UV.** Every emitted vertex samples it — a solid-white
   texel in the host's font atlas — so flat and gradient fills need no separate untextured
   draw path. Supplying the wrong UV samples the wrong texel *with no visible error*.
2. **`ip_begin` resets the pixel scale to `1.0`.** Call `ip_set_pixel_scale` after it, not
   before, or hairlines silently blur on HiDPI.
3. **The mesh is borrowed.** `mesh.vtx` and `mesh.idx` are owned by the `ip_ctx` and stay
   valid only until the next `ip_begin` or `ip_ctx_destroy`. Copy out anything you keep.

A context is reusable: call `ip_begin` again for the next element rather than creating a
second context.

## Functions

| | |
|---|---|
| `ip_version` | ABI version integer |
| `ip_ctx_create` / `ip_ctx_destroy` | context lifetime |
| `ip_begin` / `ip_end` | open a session, return an `ip_mesh` |
| `ip_set_pixel_scale` | device pixel ratio for hairlines |
| `ip_rounded_rect` | set the shape (radius ≤ 0 is a plain rect) |
| `ip_line` | a standalone segment, independent of the current shape |
| `ip_fill_color`, `ip_fill_gradient` | fills |
| `ip_fill_band_color`, `ip_fill_band_gradient` | fills clipped to a horizontal band |
| `ip_add_shadow` | outer and inset shadows, stackable |
| `ip_add_border`, `ip_add_border_inset` | borders |

## Types

`ip_vec2`, `ip_rect`, `ip_color` (packed RGBA, R in the lowest byte — the same packing as
`ImU32`), `ip_color_stop`, `ip_gradient`, `ip_gradient_mode`, `ip_shadow`, `ip_border`,
`ip_vertex`, `ip_mesh`. All plain value types the library owns.

`ip_vertex` is laid out as `pos, uv, col` — matching `ImDrawVert` exactly, so an ImGui
adapter's copy is a straight field copy. That's a convenience for that adapter, not a
dependency: the header still declares its own type rather than including an ImGui header.

Gradient modes are `IP_GRADIENT_LINEAR`, `_RADIAL`, `_ANGULAR`, and `_DIAMOND`. Stop `t`
values are expected in ascending order; an unsorted array is unspecified behavior.

## The C++ fluent wrapper

`include/imgui_painter.h` wraps the above in a chainable type, and stays ImGui-free itself:

```cpp
ip::Painter({uv.x, uv.y}, rect, 5.0f)
    .pixel_scale(scale)
    .shadow(shadow)
    .fill(gradient)
    .border(border)
    .draw(*ImGui::GetWindowDrawList());
```

`draw()` is a **template** on the draw-list type rather than taking a concrete `ImDrawList&`.
It calls `PrimReserve`/`PrimWriteVtx`/`PrimWriteIdx` generically, which resolves against a real
`ImDrawList` *and* against a duck-typed mock — `bindings/rust/tests/fluent_header_mock.cpp` is
exactly that, so header rot is caught by CI rather than by a downstream consumer.

Two-phase template lookup is what makes this work: the argument types of `dl.PrimWriteVtx(...)`
aren't checked until `DrawList` is concrete at the call site, so the brace-initialized vertex
and UV arguments construct whichever vec2 type that draw list's real method expects.

`ip::Painter` owns one `ip_ctx` and is single-use — one instance per shape. See the
[parity plan](https://github.com/samanshaiza004/imgui-painter/blob/main/docs/cpp-parity.md)
for the reusable per-frame context that will replace it on hot paths.

## Writing a new binding

The contract to uphold:

1. **One context per long-lived painter**, reused via `ip_begin` per element. Creating one per
   shape works but wastes native allocations.
2. **Sample the host's white-pixel UV and framebuffer scale.** There is no ImGui to ask from
   inside the core, so the binding must pass both in.
3. **Copy the mesh through the host draw list's public API.** For Dear ImGui that means
   `PrimReserve` / `PrimWriteVtx` / `PrimWriteIdx` — never a direct write into internal
   buffers. See [Architecture](concepts/architecture.md) for why that isn't negotiable.
4. **Rebase indices.** They're session-local and 0-based; the draw list's vertex buffer is
   shared across the frame, so offset them by its current write index.
5. **Treat `ip_mesh` as borrowed** until the next `ip_begin`.

Widget decoration is deliberately **not** part of the C ABI. It depends on reading Dear ImGui's
internal item and layout state, so it is inherently language- and version-specific; each
binding implements it against its own ImGui bindings, or omits it and exposes only painting.
