# Widget anatomy

The single source of truth for how imgui-painter reconstructs stock Dear ImGui widget geometry.

## Why this document exists

The decorator layer exists twice — once in `bindings/rust/src/item_paint.rs`, once in
`include/imgui_painter_decorators.h`. They **cannot share code**: the formulas below read Dear
ImGui's own layout state (`GetCursorScreenPos`, `CalcItemWidth`, `GetFrameHeight`,
`GetTreeNodeToLabelSpacing`, `GetStyle().GrabMinSize`), which lives on whichever side of the FFI
boundary owns the ImGui context. `docs/cpp-parity.md` accepts that duplication and asks for one
document describing the anatomy so the two implementations cannot drift silently. This is it.

**If you change a formula, change it here first**, then in both bindings, then rerun the visual
gate at 1×/1.5×/2× on both rendering paths.

## The version pin, and why it is strict

Everything below reproduces **Dear ImGui 1.91.9b** (`IMGUI_VERSION_NUM == 19191`) layout through
public API. None of it is an upstream contract. Dear ImGui is free to move a widget's internal
geometry in a patch release without notice, and when it does, the painted chrome silently stops
lining up with the widget it is supposed to be replacing.

The two bindings guard this differently, and the difference is deliberate:

| | Guard | Behaviour on mismatch |
|---|---|---|
| Rust | `debug_assert!` on `igGetVersion()` | Panics in debug, **compiled out in release** — degrades to silently wrong geometry |
| C++ | `static_assert(IMGUI_VERSION_NUM == 19191)` | Fails to compile, in every build mode |

The C++ side takes an exact equality, not a `>=` floor, and offers
`IMGUI_PAINTER_ALLOW_UNVERIFIED_IMGUI` as a named opt-out for someone who has rerun the visual
gate themselves. Note this is stricter than the ImGui-aware *host-value* header
(`imgui_painter_imgui.h`), which only needs a `>=` floor — `GetFontTexUvWhitePixel` and
`DisplayFramebufferScale` are stable, long-standing APIs, whereas chrome geometry is not.

## The bracket

Every decorator except Combo runs this sequence. **Each step's position is load-bearing.**

1. Assert the ImGui version.
2. Grab the window draw list.
3. **Capture chrome geometry — before the widget submits.** It reads `GetCursorScreenPos()`,
   which is only correct pre-submission. Widgets whose chrome is the item rect skip this.
4. `ChannelsSplit(dl, 3)`, then `ChannelsSetCurrent(dl, 1)`.
5. Push transparent style colours for this widget's suppression set.
6. **Run the widget.** It lands on channel 1.
7. Capture item state: `GetItemRectMin/Max`, `IsItemHovered(0)`, `IsItemActive`,
   `IsItemFocused`, `GetStyle().Alpha`.
8. Pop the style colours.
9. `ChannelsSetCurrent(dl, 0)`, open a canvas, paint, close the canvas (submitting).
10. `ChannelsMerge()`.

Chrome paints on channel **0** and the widget on channel **1**, so ImGui's own text and glyphs
draw *over* the painted chrome. Channel 2 is allocated but unused; both bindings keep it that way
so the split count matches.

Both guards are RAII (Rust `Drop`, C++ destructors) because ImGui asserts hard if a channel split
is left stranded. Rust unwinds on panic; C++ unwinds on exception.

### Combo does not use this bracket

Combo reimplements it because two orderings differ, and both matter:

- **Style colours pop *before* the popup contents run.** Otherwise every widget inside the
  dropdown inherits imgui-painter's transparent `FrameBg`/`Button` and renders invisibly.
- **Item state is captured *after* `EndCombo`**, which is what restores the parent window's
  backed-up `LastItemData`. Capturing earlier reads the popup's state, not the combo's.

## Contracts that hold across all widgets

- **Last-item preservation.** After a decorator returns, the wrapped widget is still ImGui's last
  item — ID, rect, hovered, active, and drag/drop eligibility intact — so callers can attach a
  tooltip or context menu immediately after. This holds mechanically because nothing after the
  widget submits another item; it breaks the moment someone adds a `Dummy()`, `InvisibleButton()`,
  or a cursor move plus item inside the bracket. CONTRIBUTING names this a public compatibility
  contract, and both bindings carry an executable regression test for it.
- **Style alpha is applied exactly once**, multiplied into every painted colour (fill, border,
  shadow). This is how decorated widgets respect `BeginDisabled()`.
- **Corner radius is clamped** to half the painted rect's height.
- **Containment tripwire.** Every resolved chrome rect is asserted to sit inside the item rect,
  with `EPSILON = 0.5`. This is the cheap early warning that an ImGui bump moved the layout out
  from under a formula.

## Per-widget anatomy

`cursor` below means `GetCursorScreenPos()` sampled **before** the widget submits.
`item` means the post-submission `GetItemRectMin/Max`.

### Button

| | |
|---|---|
| Chrome rect | `item` |
| Suppresses | `Button`, `ButtonHovered`, `ButtonActive` |
| States | `active → active` · `hovered → hover` · else `base` |

### Selectable

| | |
|---|---|
| Chrome rect | `item` |
| Suppresses | `Header`, `HeaderHovered`, `HeaderActive` |

Selectable is the one widget whose fill is not a plain slot lookup, because persistent selection
and momentary press are different things:

| Visual state | Condition (first match wins) | Fill |
|---|---|---|
| Pressed + selected | `active && selected` | `shade(fill.active, 0.12)` |
| Pressed | `active` | `fill.active` |
| Selected | `selected` | `fill.active` |
| Hovered | `hovered` | `fill.hover` |
| Idle | — | `fill.base` |

The `StateColors` triple is deliberately small. Idle selection reuses `active`, then darkens only
while pressed, so a persistently selected row does not lose click feedback.

### Checkbox

| | |
|---|---|
| Chrome rect | `GetFrameHeight()` square at `cursor` — **excludes the label** |
| Suppresses | `FrameBg`, `FrameBgHovered`, `FrameBgActive` |
| States | standard (`active`/`hovered`/`base`) |

### InputText

| | |
|---|---|
| Chrome rect | `CalcItemWidth()` × `GetFrameHeight()` at `cursor` — **excludes the label** |
| Suppresses | `FrameBg`, `FrameBgHovered`, `FrameBgActive` |
| States | standard |

Single-line only. The caller must submit exactly one single-line `InputText`.

### Slider (horizontal, linear, f32)

| | |
|---|---|
| Frame rect | `CalcItemWidth()` × `GetFrameHeight()` at `cursor` |
| Suppresses | `FrameBg`, `FrameBgHovered`, `FrameBgActive`, `SliderGrab`, `SliderGrabActive` |

```
GRAB_PADDING  = 2.0                                   // logical, never DPI-scaled
slider_size   = max(frame_width - GRAB_PADDING*2, 0)
grab_size     = min(max(GrabMinSize, 0), slider_size)
usable_size   = max(slider_size - grab_size, 0)
usable_min    = frame.min.x + GRAB_PADDING + grab_size/2
grab_center   = usable_min + usable_size * normalized_linear(value, min, max)

grab   = [grab_center ± grab_size/2] × [min(frame.min.y+PAD, frame.max.y),
                                        max(frame.max.y-PAD, frame.min.y)]

device_pixel = framebuffer_scale > 0 && finite ? 1/framebuffer_scale : 1
track_height = min(max(frame_height * 0.25, device_pixel * 2), frame_height)
track_y      = (frame.min.y + frame.max.y) / 2
track        = [min(frame.min.x+PAD, frame.max.x),
                max(frame.max.x-PAD, frame.min.x)] × [track_y ± track_height/2]
fill         = track, with max.x = clamp(grab_center, track.min.x, track.max.x)
```

Three things here look like noise and are not:

- **`GRAB_PADDING` is logical.** It is never multiplied by the framebuffer scale. Both bindings
  pin this by asserting the grab rect is *identical* at 1×, 1.5×, and 2×.
- **The track minimum height is the only DPI-aware term** in the whole formula. Pinned by: a
  100×4 frame with `GrabMinSize = 2` gives track height exactly `2.0` at 1× and `1.0` at 2×.
- **The value is read after the widget runs.** Using the pre-widget value makes the grab lag the
  cursor by one frame during a drag.

`normalized_linear` clamps to `[0,1]` and returns `0.0` for non-finite inputs or `min == max`.
Reversed ranges (`min > max`) fall out of the arithmetic correctly and are tested.

Paint order is **track → fill → grab**, all three resolving to the *same* slot. The fill is
skipped entirely when its width is not `> 0`.

| Visual state | Condition | Slot |
|---|---|---|
| Adjusting | `active` | `Active` |
| Focused | `focused` | `Hover` |
| Hovered | `hovered` | `Hover` |
| Idle | — | `Base` |

Focused and Hovered intentionally share the `Hover` slot. There is no visual tell for this in a
static screenshot, so it carries an explicit test.

### Combo

| | |
|---|---|
| Frame rect | `CalcItemWidth()` × `GetFrameHeight()` at `cursor` |
| Suppresses | `FrameBg`, `FrameBgHovered`, `FrameBgActive`, `Button`, `ButtonHovered`, `ButtonActive` |

```
arrow_width = min(max(frame_height, 0), max(frame_width, 0))   // square, clamped to width
split       = frame.max.x - arrow_width
preview     = [frame.min.x, split]     × frame.y-range
arrow       = [split, frame.max.x]     × frame.y-range
```

Paints frame then arrow region, same slot. ImGui keeps the preview text, arrow glyph, label,
popup contents, and navigation.

| Visual state | Condition (first match wins) | Slot |
|---|---|---|
| Open | popup open | `Active` |
| Pressed | `active` | `Active` |
| Focused | `focused` | `Hover` |
| Hovered | `hovered` | `Hover` |
| Idle | — | `Base` |

### TreeNode

| | |
|---|---|
| Row rect | `item` |
| Suppresses | `Header`, `HeaderHovered`, `HeaderActive` |

```
disclosure = leaf ? none
                  : [row.min.x, min(row.min.x + max(GetTreeNodeToLabelSpacing(), 0), row.max.x)]
                    × row.y-range
```

A leaf has **no** disclosure rect at all. Parent channels merge before children are drawn, so the
caller draws children after the decorator returns.

Node flags must match what the caller declares: non-leaves need
`SpanAvailWidth | OpenOnArrow`; leaves additionally need `Leaf | NoTreePushOnOpen`.

| Visual state | Condition (first match wins) | Slot |
|---|---|---|
| Pressed | `active` | `Active` |
| Selected | `selected` | `Active` |
| Focused | `focused` | `Hover` |
| Hovered | `hovered` | `Hover` |
| **Open** | `open` | **`Base`** |
| Idle | — | `Base` |

**`Open` ranking below Hovered/Focused, and painting like Idle, is deliberate.** Openness is
already communicated by the disclosure arrow; letting it outrank hover made expanded rows feel
inert. It stays a *named* state, rather than collapsing into Idle, so a future distinct open
treatment needs no re-plumbing. Both bindings assert the `Open → Base` mapping explicitly so it
survives refactoring.

## What ImGui always keeps

imgui-painter never takes over layout, input, navigation, text, glyphs, popup or tree state, or a
widget's return value. It suppresses the specific `ImGuiCol_` roles it replaces and paints
underneath. Anything not listed as a chrome part above is still drawn by Dear ImGui.
