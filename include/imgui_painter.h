/* A header-only fluent wrapper over capi/imgui_painter_c.h — the design
 * doc's `Painter(rect).fill(...).shadow(...).border(...).draw()` chaining,
 * for a direct C++ consumer (the Rust adapter, bindings/rust, is phase 1's
 * actual proof; this is phase 2's binding for C++ callers of the same
 * core).
 *
 * `draw()` is a template method parameterized on the draw-list type, not a
 * concrete `ImDrawList&` — that's what keeps this header at zero Dear ImGui
 * dependency even though it's the "fluent C++" entry point: it calls
 * dl.PrimReserve/PrimWriteVtx/PrimWriteIdx generically, which resolves
 * against a real ImDrawList (those are its actual public C++ methods — the
 * same ones bindings/rust's adapter.rs calls through cimgui's C wrappers)
 * *and* against a duck-typed mock with matching method signatures, such as
 * bindings/rust/tests/fluent_header_mock.cpp's compile-check. Two-phase
 * template lookup means `dl.PrimWriteVtx(...)`'s argument types aren't
 * checked until `DrawList` is a concrete type at the call site, so the
 * brace-init vertex/uv arguments below correctly construct whichever
 * concrete vec2 type that `DrawList`'s real method actually expects (real
 * `ImVec2`, or the mock's own).
 */
#ifndef IMGUI_PAINTER_H
#define IMGUI_PAINTER_H

#include "imgui_painter_c.h"

#include <cassert>
#include <cmath>
#include <cstdint>

namespace ip {

class Context;
class Frame;

template <typename DrawList>
class Canvas;

/* RAII fluent wrapper over one ip_ctx paint session. Single-use: construct
 * one per shape/frame element and call draw() once, mirroring how
 * bindings/rust's Session is meant to be reused via begin() per frame
 * rather than how a Painter object itself is meant to be reused — this
 * type doesn't expose begin() at all, so there's no reuse footgun to
 * document. Not copyable (owns a unique ip_ctx*); movable would be easy to
 * add but has no caller yet (see CLAUDE.md's "no speculative API surface").
 */
class Painter {
public:
    /* `white_pixel_uv` is host-specific (the UV of the host's font atlas's
     * solid-white texel) and must be supplied by the caller — defaulting it
     * would silently sample the wrong texel with no visible error, so it's
     * a required constructor argument rather than a chained setter. */
    Painter(ip_vec2 white_pixel_uv, ip_rect rect, float radius = 0.0f) {
        ctx_ = ip_ctx_create();
        ip_begin(ctx_, white_pixel_uv);
        ip_rounded_rect(ctx_, rect, radius);
    }

    ~Painter() { ip_ctx_destroy(ctx_); }

    Painter(const Painter &) = delete;
    Painter &operator=(const Painter &) = delete;

    Painter &fill(ip_color color) {
        ip_fill_color(ctx_, color);
        return *this;
    }

    Painter &fill(const ip_gradient &gradient) {
        ip_fill_gradient(ctx_, &gradient);
        return *this;
    }

    Painter &band(float y0, float y1, ip_color color) {
        ip_fill_band_color(ctx_, y0, y1, color);
        return *this;
    }

    Painter &band(float y0, float y1, const ip_gradient &gradient) {
        ip_fill_band_gradient(ctx_, y0, y1, &gradient);
        return *this;
    }

    Painter &pixel_scale(float scale) {
        ip_set_pixel_scale(ctx_, scale);
        return *this;
    }

    Painter &shadow(const ip_shadow &s) {
        ip_add_shadow(ctx_, &s);
        return *this;
    }

    Painter &border(const ip_border &b) {
        ip_add_border(ctx_, &b);
        return *this;
    }

    Painter &border(float inset, const ip_border &b) {
        ip_add_border_inset(ctx_, inset, &b);
        return *this;
    }

    Painter &line(ip_vec2 a, ip_vec2 b, float thickness, ip_color color) {
        ip_line(ctx_, a, b, thickness, color);
        return *this;
    }

    /* Copies the session's mesh into `dl` through its own PrimReserve/
     * PrimWriteVtx/PrimWriteIdx — never by touching internal buffers
     * directly (see the README's core/adapter rationale, the same one
     * bindings/rust's adapter.rs follows). */
    template <typename DrawList>
    void draw(DrawList &dl) {
        const ip_mesh mesh = ip_end(ctx_);
        if (mesh.vtx_count <= 0 || mesh.idx_count <= 0) {
            return;
        }
        dl.PrimReserve(mesh.idx_count, mesh.vtx_count);
        /* Indices are session-local (0-based); the draw list's vertex
         * buffer is shared across everything drawn this frame, so rebase
         * against its current write offset before writing ours in — same
         * rebasing bindings/rust's adapter.rs does against
         * `_VtxCurrentIdx`. */
        const uint16_t base = static_cast<uint16_t>(dl._VtxCurrentIdx);
        for (int32_t i = 0; i < mesh.idx_count; ++i) {
            dl.PrimWriteIdx(static_cast<uint16_t>(base + mesh.idx[i]));
        }
        for (int32_t i = 0; i < mesh.vtx_count; ++i) {
            const ip_vertex &v = mesh.vtx[i];
            dl.PrimWriteVtx({v.pos.x, v.pos.y}, {v.uv.x, v.uv.y}, v.col);
        }
    }

private:
    ip_ctx *ctx_;
};

/* Long-lived owner of one reusable ip_ctx. begin_frame() borrows that
 * context until the returned Frame is destroyed; debug builds catch
 * overlapping frames, which C++ cannot reject through a borrow checker. */
class Context {
public:
    Context() : ctx_(ip_ctx_create()), frame_active_(false) {}

    ~Context() { ip_ctx_destroy(ctx_); }

    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;

    Frame begin_frame(ip_vec2 white_pixel_uv, float pixel_scale = 1.0f);

private:
    friend class Frame;

    template <typename DrawList>
    friend class Canvas;

    ip_ctx *ctx_;
    bool frame_active_;
};

/* One imgui frame's painting scope. It owns no context and performs no
 * submission work; each Canvas resets, accumulates, and submits its own
 * mesh before the next Canvas may reuse the context. */
class Frame {
public:
    ~Frame() {
        assert(!canvas_active_);
        context_->frame_active_ = false;
    }

    template <typename DrawList>
    Canvas<DrawList> canvas(DrawList &dl);

private:
    friend class Context;

    template <typename DrawList>
    friend class Canvas;

    Frame(Context &context, ip_vec2 white_pixel_uv, float pixel_scale)
        : context_(&context), white_pixel_uv_(white_pixel_uv), pixel_scale_(pixel_scale),
          canvas_active_(false) {}

    Frame(const Frame &) = delete;
    Frame &operator=(const Frame &) = delete;

    Context *context_;
    ip_vec2 white_pixel_uv_;
    float pixel_scale_;
    bool canvas_active_;
};

/* Per-draw-list accumulator borrowed from a Frame. Its mesh never escapes:
 * destruction ends the session, copies the mesh into the draw list once,
 * then releases the Frame for sequential reuse by another Canvas. */
template <typename DrawList>
class Canvas {
public:
    Canvas &rounded_rect(ip_rect rect, float radius) {
        ip_rounded_rect(ctx(), rect, radius);
        return *this;
    }

    Canvas &fill(ip_color color) {
        ip_fill_color(ctx(), color);
        return *this;
    }

    Canvas &fill(const ip_gradient &gradient) {
        ip_fill_gradient(ctx(), &gradient);
        return *this;
    }

    Canvas &band(float y0, float y1, ip_color color) {
        ip_fill_band_color(ctx(), y0, y1, color);
        return *this;
    }

    Canvas &band(float y0, float y1, const ip_gradient &gradient) {
        ip_fill_band_gradient(ctx(), y0, y1, &gradient);
        return *this;
    }

    Canvas &shadow(const ip_shadow &s) {
        ip_add_shadow(ctx(), &s);
        return *this;
    }

    Canvas &border(const ip_border &b) {
        ip_add_border(ctx(), &b);
        return *this;
    }

    Canvas &border(float inset, const ip_border &b) {
        ip_add_border_inset(ctx(), inset, &b);
        return *this;
    }

    Canvas &line(ip_vec2 a, ip_vec2 b, float thickness, ip_color color) {
        ip_line(ctx(), a, b, thickness, color);
        return *this;
    }

    float device_pixel() const { return 1.0f / frame_->pixel_scale_; }

    ~Canvas() {
        const ip_mesh mesh = ip_end(ctx());
        if (mesh.vtx_count > 0 && mesh.idx_count > 0) {
            draw_list_->PrimReserve(mesh.idx_count, mesh.vtx_count);
            /* PrimWriteVtx advances _VtxCurrentIdx, so capture the shared
             * draw-list base and write all rebased indices first. */
            const uint16_t base = static_cast<uint16_t>(draw_list_->_VtxCurrentIdx);
            for (int32_t i = 0; i < mesh.idx_count; ++i) {
                draw_list_->PrimWriteIdx(static_cast<uint16_t>(base + mesh.idx[i]));
            }
            for (int32_t i = 0; i < mesh.vtx_count; ++i) {
                const ip_vertex &v = mesh.vtx[i];
                draw_list_->PrimWriteVtx({v.pos.x, v.pos.y}, {v.uv.x, v.uv.y}, v.col);
            }
        }
        frame_->canvas_active_ = false;
    }

private:
    friend class Frame;

    Canvas(Frame &frame, DrawList &draw_list) : frame_(&frame), draw_list_(&draw_list) {}

    Canvas(const Canvas &) = delete;
    Canvas &operator=(const Canvas &) = delete;

    ip_ctx *ctx() const { return frame_->context_->ctx_; }

    Frame *frame_;
    DrawList *draw_list_;
};

inline Frame Context::begin_frame(ip_vec2 white_pixel_uv, float pixel_scale) {
    assert(!frame_active_);
    frame_active_ = true;
    const float guarded_scale =
        std::isfinite(pixel_scale) && pixel_scale > 0.0f ? pixel_scale : 1.0f;
    return Frame(*this, white_pixel_uv, guarded_scale);
}

template <typename DrawList>
Canvas<DrawList> Frame::canvas(DrawList &dl) {
    assert(!canvas_active_);
    canvas_active_ = true;
    ip_begin(context_->ctx_, white_pixel_uv_);
    ip_set_pixel_scale(context_->ctx_, pixel_scale_);
    return Canvas<DrawList>(*this, dl);
}

} // namespace ip

#endif /* IMGUI_PAINTER_H */
