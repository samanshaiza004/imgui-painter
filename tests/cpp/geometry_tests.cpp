#include "imgui_painter_c.h"
#include "test_harness.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <vector>

namespace {

using ip_test::require;

constexpr ip_vec2 uv() { return {0.5f, 0.5f}; }
constexpr ip_rect rect(float min_x, float min_y, float max_x, float max_y) {
    return {{min_x, min_y}, {max_x, max_y}};
}
constexpr ip_color rgba(unsigned r, unsigned g, unsigned b,
                        unsigned a = 255u) {
    return r | (g << 8u) | (b << 16u) | (a << 24u);
}

// Per-channel near-equality: atan2-based gradients (Angular) can land a
// hair off an exact stop boundary (e.g. t == 0.49999997 instead of 0.5)
// purely from floating-point rounding in the angle math, which the
// truncating lerp in LerpColor then turns into an off-by-one channel
// value. Comparing colors exactly at a stop boundary is testing float
// rounding, not gradient correctness — this tolerance is the fix.
bool color_close(ip_color actual, ip_color expected, int tolerance) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        const int a = static_cast<int>((actual >> shift) & 0xFFu);
        const int e = static_cast<int>((expected >> shift) & 0xFFu);
        if (std::abs(a - e) > tolerance) {
            return false;
        }
    }
    return true;
}

struct Mesh {
    std::vector<ip_vertex> vertices;
    std::vector<uint16_t> indices;
};

bool same_vertex(const ip_vertex &a, const ip_vertex &b) {
    return a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.uv.x == b.uv.x &&
           a.uv.y == b.uv.y && a.col == b.col;
}

bool same_mesh(const Mesh &a, const Mesh &b) {
    return a.indices == b.indices && a.vertices.size() == b.vertices.size() &&
           std::equal(a.vertices.begin(), a.vertices.end(), b.vertices.begin(),
                      same_vertex);
}

class Session {
  public:
    Session() : ctx_(ip_ctx_create()) { require(ctx_ != nullptr, "ip_ctx_create failed"); }
    ~Session() { ip_ctx_destroy(ctx_); }
    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    void begin() { ip_begin(ctx_, uv()); }
    void set_pixel_scale(float scale) { ip_set_pixel_scale(ctx_, scale); }
    void rounded_rect(ip_rect shape, float radius) {
        ip_rounded_rect(ctx_, shape, radius);
    }
    void fill_color(ip_color color) { ip_fill_color(ctx_, color); }
    void fill_gradient(const ip_gradient &gradient) {
        ip_fill_gradient(ctx_, &gradient);
    }
    void fill_band_color(float y0, float y1, ip_color color) {
        ip_fill_band_color(ctx_, y0, y1, color);
    }
    void fill_band_gradient(float y0, float y1, const ip_gradient &gradient) {
        ip_fill_band_gradient(ctx_, y0, y1, &gradient);
    }
    void add_shadow(const ip_shadow &shadow) { ip_add_shadow(ctx_, &shadow); }
    void add_border(const ip_border &border) { ip_add_border(ctx_, &border); }
    void add_border_inset(float inset, const ip_border &border) {
        ip_add_border_inset(ctx_, inset, &border);
    }
    void line(ip_vec2 a, ip_vec2 b, float thickness, ip_color color) {
        ip_line(ctx_, a, b, thickness, color);
    }
    Mesh end() {
        const ip_mesh raw = ip_end(ctx_);
        Mesh mesh;
        if (raw.vtx_count > 0) {
            mesh.vertices.assign(raw.vtx, raw.vtx + raw.vtx_count);
        }
        if (raw.idx_count > 0) {
            mesh.indices.assign(raw.idx, raw.idx + raw.idx_count);
        }
        return mesh;
    }

  private:
    ip_ctx *ctx_;
};

template <typename Predicate>
bool all_vertices(const Mesh &mesh, Predicate predicate) {
    return std::all_of(mesh.vertices.begin(), mesh.vertices.end(), predicate);
}

template <typename Predicate>
bool any_vertex(const Mesh &mesh, Predicate predicate) {
    return std::any_of(mesh.vertices.begin(), mesh.vertices.end(), predicate);
}

bool valid_indices(const Mesh &mesh) {
    return mesh.indices.size() % 3 == 0 &&
           std::all_of(mesh.indices.begin(), mesh.indices.end(),
                       [&mesh](uint16_t index) { return index < mesh.vertices.size(); });
}

ip_shadow default_shadow() {
    return {{0.0f, 0.0f}, 12.0f, 0.0f, rgba(0, 0, 0, 120), false};
}

IP_TEST_CASE(version_is_reachable_through_ffi, "version_is_reachable_through_ffi") {
    require(ip_version() == 2, "unexpected C ABI version");
}

IP_TEST_CASE(rgba_packs_r_in_the_lowest_byte, "rgba_packs_r_in_the_lowest_byte") {
    // Regression: an earlier hand-packed test literal got this backwards
    // (treated the top byte as R, ARGB-style). Pin the shared ip_color order.
    require(rgba(0xAA, 0xBB, 0xCC, 0xDD) == 0xDDCCBBAAu, "RGBA packing is wrong");
    require(rgba(0xFF, 0, 0, 0) == 0x000000FFu, "R is not the lowest byte");
    require(rgba(0, 0, 0, 0xFF) == 0xFF000000u, "alpha is not the highest byte");
}

IP_TEST_CASE(radius_zero_never_produces_nan_positions,
             "radius_zero_never_produces_nan_positions") {
    // Regression: AppendArc's segment-angle division (i / segments) was
    // 0.0 / 0.0 at radius <= 0 (segments == 0) before it was guarded,
    // producing NaN vertex positions that earlier tests didn't catch
    // because they only asserted vertex *counts*, not that the
    // positions were finite. Cover every effect at radius 0, not just
    // solid fill.
    const auto finite = [](const Mesh &mesh) {
        return all_vertices(mesh, [](const ip_vertex &v) {
            return std::isfinite(v.pos.x) && std::isfinite(v.pos.y);
        });
    };
    const ip_rect shape = rect(0, 0, 50, 50);
    {
        Session s; s.begin(); s.rounded_rect(shape, 0); s.fill_color(rgba(255,255,255));
        require(finite(s.end()), "solid fill produced a non-finite position");
    }
    {
        const ip_color_stop stops[] = {{0, rgba(255,0,0)}, {1, rgba(0,0,255)}};
        const ip_gradient gradient{IP_GRADIENT_LINEAR, {0,0}, {50,0}, stops, 2};
        Session s; s.begin(); s.rounded_rect(shape, 0); s.fill_gradient(gradient);
        require(finite(s.end()), "gradient produced a non-finite position");
    }
    {
        const ip_shadow shadow{{0,0}, 8, 4, rgba(0,0,0,120), false};
        Session s; s.begin(); s.rounded_rect(shape, 0); s.add_shadow(shadow);
        require(finite(s.end()), "shadow produced a non-finite position");
    }
    {
        const ip_border border{2, rgba(255,255,255)};
        Session s; s.begin(); s.rounded_rect(shape, 0); s.add_border(border);
        require(finite(s.end()), "border produced a non-finite position");
    }
}

IP_TEST_CASE(solid_fill_produces_a_closed_triangle_fan,
             "solid_fill_produces_a_closed_triangle_fan") {
    const ip_color color = rgba(255,255,255);
    Session s; s.begin(); s.rounded_rect(rect(0,0,100,50), 8); s.fill_color(color);
    const Mesh mesh = s.end();
    require(!mesh.vertices.empty(), "solid fill is empty");
    require(valid_indices(mesh), "solid fill has invalid triangle indices");
    require(all_vertices(mesh, [](const ip_vertex &v) {
        return v.uv.x == uv().x && v.uv.y == uv().y && v.col == color;
    }), "solid fill vertex attributes are wrong");
}

IP_TEST_CASE(no_shape_means_no_output, "no_shape_means_no_output") {
    Session s; s.begin(); s.fill_color(rgba(255,255,255)); const Mesh mesh = s.end();
    require(mesh.vertices.empty() && mesh.indices.empty(), "fill without a shape emitted output");
}

IP_TEST_CASE(begin_clears_the_previous_session, "begin_clears_the_previous_session") {
    Session s; s.begin(); s.rounded_rect(rect(0,0,10,10), 0); s.fill_color(rgba(255,0,0));
    require(!s.end().vertices.empty(), "first session emitted no output");
    s.begin();
    require(s.end().vertices.empty(), "begin did not clear the previous session");
}

IP_TEST_CASE(zero_radius_still_fills_a_plain_rect, "zero_radius_still_fills_a_plain_rect") {
    Session s; s.begin(); s.rounded_rect(rect(0,0,20,10), 0); s.fill_color(rgba(255,255,255));
    const Mesh mesh = s.end();
    require(mesh.vertices.size() == 5, "plain rect is not 4 outline vertices plus 1 centroid");
    require(mesh.indices.size() == 12, "plain rect is not four triangles");
}

Mesh rounded_fill(float radius, float size = 200.0f) {
    Session s; s.begin(); s.rounded_rect(rect(0,0,size,size), radius); s.fill_color(rgba(255,255,255));
    return s.end();
}

IP_TEST_CASE(larger_radius_tessellates_more_vertices_than_smaller,
             "larger_radius_tessellates_more_vertices_than_smaller") {
    require(rounded_fill(64).vertices.size() > rounded_fill(2).vertices.size(),
            "larger radius did not add tessellation vertices");
}

IP_TEST_CASE(segment_count_grows_sublinearly_with_radius,
             "segment_count_grows_sublinearly_with_radius") {
    // Pin the error-bounded, roughly sqrt(radius), segment formula against a
    // regression to linear growth. These radii avoid the floor/ceiling clamps.
    const Mesh small = rounded_fill(20, 600);
    const Mesh large = rounded_fill(200, 600);
    require(large.vertices.size() > small.vertices.size(), "segment count did not grow");
    const double ratio = static_cast<double>(large.vertices.size()) / small.vertices.size();
    require(ratio < 5.0, "10x radius produced at least 5x the vertices");
}

IP_TEST_CASE(linear_gradient_endpoints_hit_exact_stop_colors,
             "linear_gradient_endpoints_hit_exact_stop_colors") {
    const ip_color first = rgba(0x11,0x11,0x11,0x11), last = rgba(0x99,0x99,0x99,0x99);
    const ip_color_stop stops[] = {{0,first},{1,last}};
    const ip_gradient gradient{IP_GRADIENT_LINEAR,{0,25},{100,25},stops,2};
    Session s; s.begin(); s.rounded_rect(rect(0,0,100,50),0); s.fill_gradient(gradient);
    const Mesh mesh=s.end(); bool left=false,right=false;
    for (const auto &v: mesh.vertices) {
        if (v.pos.x==0) { left=true; require(v.col==first,"left endpoint color is wrong"); }
        if (v.pos.x==100) { right=true; require(v.col==last,"right endpoint color is wrong"); }
    }
    require(left && right, "gradient endpoint vertices are missing");
}

void require_degenerate_gradient(ip_gradient_mode mode) {
    const ip_color first=rgba(0xAA,0xAA,0xAA,0xAA);
    const ip_color_stop stops[]={{0,first},{1,rgba(0xBB,0xBB,0xBB,0xBB)}};
    const ip_gradient gradient{mode,{20,20},{20,20},stops,2};
    Session s; s.begin(); s.rounded_rect(rect(0,0,40,40),4); s.fill_gradient(gradient);
    const Mesh mesh=s.end();
    require(!mesh.vertices.empty(), "degenerate gradient emitted no geometry");
    require(all_vertices(mesh,[](const ip_vertex &v){return v.col==first;}),
            "degenerate gradient did not use first stop");
}

IP_TEST_CASE(degenerate_gradient_axis_falls_back_to_first_stop,
             "degenerate_gradient_axis_falls_back_to_first_stop") {
    require_degenerate_gradient(IP_GRADIENT_LINEAR);
    require_degenerate_gradient(IP_GRADIENT_RADIAL);
}

IP_TEST_CASE(degenerate_axis_falls_back_to_first_stop_for_angular_and_diamond,
             "degenerate_axis_falls_back_to_first_stop_for_angular_and_diamond") {
    require_degenerate_gradient(IP_GRADIENT_ANGULAR);
    require_degenerate_gradient(IP_GRADIENT_DIAMOND);
}

Mesh angular_mesh() {
    const ip_color_stop stops[]={{0,rgba(255,0,0)},{0.5f,rgba(0,255,0)},{1,rgba(0,0,255)}};
    const ip_gradient gradient{IP_GRADIENT_ANGULAR,{0,0},{1,0},stops,3};
    Session s; s.begin(); s.rounded_rect(rect(-50,-50,50,50),0); s.fill_gradient(gradient);
    return s.end();
}

IP_TEST_CASE(angular_gradient_sweeps_from_the_to_direction,
             "angular_gradient_sweeps_from_the_to_direction") {
    const Mesh mesh=angular_mesh();
    require(any_vertex(mesh,[](const ip_vertex &v){return v.pos.x==50 && v.pos.y==0 && v.col==rgba(255,0,0);}),
            "angular sweep does not start in the to direction");
    require(any_vertex(mesh,[](const ip_vertex &v){return v.pos.x==-50 && v.pos.y==0 && color_close(v.col,rgba(0,255,0),1);}),
            "angular sweep midpoint color is wrong");
}

IP_TEST_CASE(angular_gradient_seam_is_a_hard_wrap_not_a_blend,
             "angular_gradient_seam_is_a_hard_wrap_not_a_blend") {
    const ip_color_stop stops[]={{0,rgba(255,0,0)},{1,rgba(0,0,255)}};
    const ip_gradient gradient{IP_GRADIENT_ANGULAR,{0,0},{1,0},stops,2};
    Session s; s.begin(); s.rounded_rect(rect(-50,-50,50,50),0); s.fill_gradient(gradient);
    const Mesh mesh=s.end();
    require(any_vertex(mesh,[](const ip_vertex &v){return v.pos.x==50 && v.pos.y==0 && v.col==rgba(255,0,0);}),
            "angular seam start was blended instead of wrapped");
}

IP_TEST_CASE(diamond_gradient_uses_per_axis_max_norm,
             "diamond_gradient_uses_per_axis_max_norm") {
    const ip_color red=rgba(255,0,0), blue=rgba(0,0,255);
    const ip_color_stop stops[]={{0,red},{1,blue}};
    const ip_gradient gradient{IP_GRADIENT_DIAMOND,{0,0},{10,20},stops,2};
    Session s; s.begin(); s.rounded_rect(rect(0,0,10,20),0); s.fill_gradient(gradient);
    const Mesh mesh=s.end();
    auto at=[&mesh](float x,float y,ip_color c){return any_vertex(mesh,[=](const ip_vertex &v){return v.pos.x==x&&v.pos.y==y&&v.col==c;});};
    require(at(0,0,red), "diamond origin color is wrong");
    require(at(10,0,blue), "diamond x-axis maximum is wrong");
    require(at(0,20,blue), "diamond y-axis maximum is wrong");
}

IP_TEST_CASE(degenerate_zero_size_rect_does_not_panic,
             "degenerate_zero_size_rect_does_not_panic") {
    const ip_color_stop stops[]={{0,rgba(255,255,255)},{1,rgba(255,0,0,0)}};
    const ip_gradient gradient{IP_GRADIENT_RADIAL,{5,5},{25,5},stops,2};
    Session s; s.begin(); s.rounded_rect(rect(5,5,5,5),10); s.fill_gradient(gradient);
    require(valid_indices(s.end()), "zero-size rect produced invalid indices");
}

IP_TEST_CASE(empty_stops_is_a_no_op, "empty_stops_is_a_no_op") {
    const ip_gradient gradient{IP_GRADIENT_LINEAR,{0,0},{10,0},nullptr,0};
    Session s; s.begin(); s.rounded_rect(rect(0,0,10,10),0); s.fill_gradient(gradient);
    const Mesh mesh=s.end(); require(mesh.vertices.empty()&&mesh.indices.empty(),"empty stops emitted output");
}

IP_TEST_CASE(single_stop_gradient_fills_solid, "single_stop_gradient_fills_solid") {
    const ip_color color=rgba(0x42,0x42,0x42,0x42); const ip_color_stop stop{0,color};
    const ip_gradient gradient{IP_GRADIENT_RADIAL,{5,5},{5,5},&stop,1};
    Session s; s.begin(); s.rounded_rect(rect(0,0,10,10),0); s.fill_gradient(gradient);
    const Mesh mesh=s.end(); require(!mesh.vertices.empty(),"single stop emitted no output");
    require(all_vertices(mesh,[](const ip_vertex &v){return v.col==color;}),"single stop was not solid");
}

IP_TEST_CASE(gradient_fill_tessellates_more_vertices_than_solid_fill,
             "gradient_fill_tessellates_more_vertices_than_solid_fill") {
    const Mesh solid=rounded_fill(8,100);
    const ip_color_stop stops[]={{0,rgba(255,0,0)},{1,rgba(0,0,255)}};
    const ip_gradient gradient{IP_GRADIENT_LINEAR,{0,0},{100,0},stops,2};
    Session s; s.begin(); s.rounded_rect(rect(0,0,100,100),8); s.fill_gradient(gradient);
    require(s.end().vertices.size()>solid.vertices.size(),"gradient edge subdivision is missing");
}

IP_TEST_CASE(radial_gradient_center_and_far_corner_hit_stop_colors,
             "radial_gradient_center_and_far_corner_hit_stop_colors") {
    const ip_color first=rgba(0x11,0x11,0x11,0x11),last=rgba(0x99,0x99,0x99,0x99);
    const ip_color_stop stops[]={{0,first},{1,last}};
    const ip_gradient gradient{IP_GRADIENT_RADIAL,{50,50},{50,0},stops,2};
    Session s; s.begin(); s.rounded_rect(rect(0,0,100,100),0); s.fill_gradient(gradient);
    const Mesh mesh=s.end(); require(!mesh.vertices.empty(),"radial gradient is empty");
    require(mesh.vertices[0].pos.x==50&&mesh.vertices[0].pos.y==50&&mesh.vertices[0].col==first,
            "radial center did not hit first stop");
    for(const auto &v:mesh.vertices) if(v.pos.x==0||v.pos.x==100) require(v.col==last,"radial corner did not hit last stop");
}

Mesh band_mesh(float y0,float y1) {
    Session s; s.begin(); s.rounded_rect(rect(10,20,90,80),12); s.fill_band_color(y0,y1,rgba(255,255,255)); return s.end();
}

IP_TEST_CASE(band_fill_clips_vertices_and_accepts_inverted_endpoints,
             "band_fill_clips_vertices_and_accepts_inverted_endpoints") {
    const Mesh forward=band_mesh(35,55), inverted=band_mesh(55,35);
    require(!forward.vertices.empty(),"band is empty");
    require(all_vertices(forward,[](const ip_vertex &v){return v.pos.y>=34.999f&&v.pos.y<=55.001f;}),"band escaped interval");
    require(same_mesh(forward,inverted),"inverted band endpoints changed the mesh");
}

std::array<float,4> bounds(const Mesh &mesh) {
    std::array<float,4> result={std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()};
    for(const auto &v:mesh.vertices){result[0]=std::min(result[0],v.pos.x);result[1]=std::min(result[1],v.pos.y);result[2]=std::max(result[2],v.pos.x);result[3]=std::max(result[3],v.pos.y);} return result;
}

IP_TEST_CASE(band_outside_shape_is_empty_and_full_height_matches_fill_bounds,
             "band_outside_shape_is_empty_and_full_height_matches_fill_bounds") {
    require(band_mesh(100,120).vertices.empty(),"outside band emitted geometry");
    Session s; s.begin(); s.rounded_rect(rect(10,20,90,80),12); s.fill_color(rgba(255,255,255)); const Mesh plain=s.end();
    require(bounds(plain)==bounds(band_mesh(20,80)),"full-height band bounds differ from fill");
}

IP_TEST_CASE(gradient_band_clips_to_the_requested_interval,
             "gradient_band_clips_to_the_requested_interval") {
    const ip_color_stop stops[]={{0,rgba(255,0,0)},{1,rgba(0,0,255)}};
    const ip_gradient gradient{IP_GRADIENT_LINEAR,{0,8},{40,8},stops,2};
    Session s; s.begin(); s.rounded_rect(rect(0,0,40,40),4); s.fill_band_gradient(8,12,gradient);
    const Mesh mesh=s.end(); require(!mesh.vertices.empty(),"gradient band is empty");
    require(all_vertices(mesh,[](const ip_vertex &v){return v.pos.y>=7.999f&&v.pos.y<=12.001f;}),"gradient band escaped interval");
}

IP_TEST_CASE(inset_shadow_stays_inside_and_hard_band_reaches_only_spread,
             "inset_shadow_stays_inside_and_hard_band_reaches_only_spread") {
    const ip_rect shape=rect(10,20,90,80); constexpr float spread=6;
    const ip_shadow shadow{{0,0},0,spread,rgba(0,0,0),true};
    Session s; s.begin(); s.rounded_rect(shape,0); s.add_shadow(shadow); const Mesh mesh=s.end();
    require(!mesh.vertices.empty(),"inset shadow is empty");
    require(all_vertices(mesh,[shape](const ip_vertex &v){return v.pos.x>=shape.min.x-.5f&&v.pos.x<=shape.max.x+.5f&&v.pos.y>=shape.min.y-.5f&&v.pos.y<=shape.max.y+.5f;}),"inset shadow escaped shape");
    require(all_vertices(mesh,[shape](const ip_vertex &v){const float d=std::min({v.pos.x-shape.min.x,shape.max.x-v.pos.x,v.pos.y-shape.min.y,shape.max.y-v.pos.y});return d<=6.001f;}),"hard inset shadow exceeded spread");
}

IP_TEST_CASE(blurred_offset_inset_shadow_stays_inside_shape,
             "blurred_offset_inset_shadow_stays_inside_shape") {
    const ip_rect shape=rect(10,20,90,80); const ip_shadow shadow{{5,-3},12,2,rgba(0,0,0),true};
    Session s; s.begin(); s.rounded_rect(shape,8); s.add_shadow(shadow); const Mesh mesh=s.end();
    require(!mesh.vertices.empty(),"blurred inset shadow is empty");
    require(all_vertices(mesh,[shape](const ip_vertex &v){return v.pos.x>=shape.min.x-.5f&&v.pos.x<=shape.max.x+.5f&&v.pos.y>=shape.min.y-.5f&&v.pos.y<=shape.max.y+.5f;}),"blurred inset shadow escaped shape");
}

IP_TEST_CASE(inset_shadow_without_shape_emits_nothing,
             "inset_shadow_without_shape_emits_nothing") {
    ip_shadow shadow=default_shadow(); shadow.inset=true; Session s; s.begin(); s.add_shadow(shadow);
    require(s.end().vertices.empty(),"inset shadow without shape emitted geometry");
}

Mesh shadow_mesh(float blur, ip_vec2 offset={0,0}, float spread=4) {
    const ip_shadow shadow{offset,blur,spread,rgba(0,0,0),false};
    Session s; s.begin(); s.rounded_rect(rect(0,0,50,50),blur==0?0:4); s.add_shadow(shadow); return s.end();
}

IP_TEST_CASE(hard_edged_shadow_is_a_single_uniform_ring,
             "hard_edged_shadow_is_a_single_uniform_ring") {
    const Mesh mesh=shadow_mesh(0,{0,0},10); require(!mesh.vertices.empty(),"hard shadow is empty");
    require(all_vertices(mesh,[](const ip_vertex &v){return v.col==rgba(0,0,0);}),"hard shadow alpha is not uniform");
}

IP_TEST_CASE(blurred_shadow_produces_more_vertices_and_varied_alpha_than_hard_edged,
             "blurred_shadow_produces_more_vertices_and_varied_alpha_than_hard_edged") {
    const Mesh hard=shadow_mesh(0), blurred=shadow_mesh(20);
    require(blurred.vertices.size()>hard.vertices.size(),"blurred shadow did not add rings");
    std::set<unsigned> alphas; for(const auto &v:blurred.vertices) alphas.insert(v.col>>24);
    require(alphas.size()>1,"blurred shadow has uniform alpha");
}

IP_TEST_CASE(shadow_offset_translates_the_ring, "shadow_offset_translates_the_ring") {
    const Mesh mesh=shadow_mesh(0,{10,20},0);
    require(any_vertex(mesh,[](const ip_vertex &v){return v.pos.x==10&&v.pos.y==20;}),"offset corner is missing");
    require(!any_vertex(mesh,[](const ip_vertex &v){return v.pos.x==0&&v.pos.y==0;}),"untranslated corner remains");
}

IP_TEST_CASE(stacked_shadows_accumulate_more_vertices_than_one,
             "stacked_shadows_accumulate_more_vertices_than_one") {
    auto mesh_with=[](int calls){Session s;s.begin();s.rounded_rect(rect(0,0,50,50),4);const ip_shadow sh=default_shadow();for(int i=0;i<calls;++i)s.add_shadow(sh);return s.end();};
    require(mesh_with(2).vertices.size()>mesh_with(1).vertices.size(),"stacked shadows did not accumulate");
}

IP_TEST_CASE(border_is_a_hollow_ring_with_exact_vertex_and_index_counts,
             "border_is_a_hollow_ring_with_exact_vertex_and_index_counts") {
    const ip_border border{2,rgba(255,255,255)}; Session s;s.begin();s.rounded_rect(rect(0,0,50,50),0);s.add_border(border);const Mesh mesh=s.end();
    require(mesh.vertices.size()==8,"plain border is not 8 vertices");require(mesh.indices.size()==24,"plain border is not 24 indices");require(valid_indices(mesh),"border indices are invalid");
}

Mesh hairline_mesh(float scale) {
    const ip_border border{.5f,rgba(255,255,255)}; Session s;s.begin();s.set_pixel_scale(scale);s.rounded_rect(rect(0,0,50,50),0);s.add_border(border);return s.end();
}

IP_TEST_CASE(hairline_border_scales_alpha_instead_of_geometry,
             "hairline_border_scales_alpha_instead_of_geometry") {
    const Mesh mesh=hairline_mesh(1);require(!mesh.vertices.empty(),"hairline border is empty");
    require(all_vertices(mesh,[](const ip_vertex &v){return v.col==rgba(255,255,255,0x7F);}),"hairline border did not scale alpha to 0x7F");
}

IP_TEST_CASE(pixel_scale_makes_half_logical_pixel_a_crisp_device_pixel,
             "pixel_scale_makes_half_logical_pixel_a_crisp_device_pixel") {
    require(all_vertices(hairline_mesh(2),[](const ip_vertex &v){return v.col==rgba(255,255,255);}),"scale 2 hairline is not opaque");
    require(all_vertices(hairline_mesh(1),[](const ip_vertex &v){return v.col==rgba(255,255,255,0x7F);}),"scale 1 hairline alpha is wrong");
}

IP_TEST_CASE(inset_borders_stack_on_distinct_outlines,
             "inset_borders_stack_on_distinct_outlines") {
    const ip_color outer=rgba(0x11,0x22,0x33),inner=rgba(0xDD,0xEE,0xFF);const ip_border a{1,outer},b{1,inner};
    Session s;s.begin();s.rounded_rect(rect(0,0,50,30),4);s.add_border(a);s.add_border_inset(1,b);const Mesh mesh=s.end();
    auto min_x=[&mesh](ip_color c){float x=std::numeric_limits<float>::infinity();for(const auto &v:mesh.vertices)if(v.col==c)x=std::min(x,v.pos.x);return x;};
    require(std::abs(min_x(outer))<.001f,"outer border outline moved");require(std::abs(min_x(inner)-1)<.001f,"inset border is not on distinct outline");
}

IP_TEST_CASE(invalid_border_geometry_is_ignored, "invalid_border_geometry_is_ignored") {
    const ip_border valid{1,rgba(255,255,255)},nan_border{std::numeric_limits<float>::quiet_NaN(),rgba(255,255,255)};
    Session s;s.begin();s.rounded_rect(rect(0,0,50,30),4);s.add_border_inset(-1,valid);s.add_border(nan_border);
    require(s.end().vertices.empty(),"invalid border geometry emitted output");
}

IP_TEST_CASE(begin_resets_pixel_scale, "begin_resets_pixel_scale") {
    const ip_border border{.5f,rgba(255,255,255)};Session s;s.begin();s.set_pixel_scale(2);s.begin();s.rounded_rect(rect(0,0,50,50),0);s.add_border(border);
    require(all_vertices(s.end(),[](const ip_vertex &v){return v.col==rgba(255,255,255,0x7F);}),"begin did not reset pixel scale to 1");
}

IP_TEST_CASE(border_thicker_than_shape_does_not_panic,
             "border_thicker_than_shape_does_not_panic") {
    const ip_border border{100,rgba(255,255,255)};Session s;s.begin();s.rounded_rect(rect(0,0,10,10),2);s.add_border(border);
    require(valid_indices(s.end()),"over-thick border produced invalid indices");
}

IP_TEST_CASE(no_shape_means_no_shadow_or_border_output,
             "no_shape_means_no_shadow_or_border_output") {
    const ip_shadow shadow=default_shadow();const ip_border border{1,rgba(255,255,255)};Session s;s.begin();s.add_shadow(shadow);s.add_border(border);const Mesh mesh=s.end();
    require(mesh.vertices.empty()&&mesh.indices.empty(),"shadow/border without shape emitted output");
}

IP_TEST_CASE(line_produces_a_one_pixel_wide_quad_with_the_expected_span,
             "line_produces_a_one_pixel_wide_quad_with_the_expected_span") {
    const ip_color color=rgba(255,0,0);Session s;s.begin();s.line({10,5},{10,25},1,color);const Mesh mesh=s.end();
    require(mesh.vertices.size()==4,"line is not one quad");require(mesh.indices.size()==6,"line is not two triangles");
    require(all_vertices(mesh,[](const ip_vertex &v){return (v.pos.x==9.5f||v.pos.x==10.5f)&&(v.pos.y==5||v.pos.y==25)&&v.col==color;}),"line span, width, or color is wrong");
}

IP_TEST_CASE(multiple_shapes_accumulate_into_one_mesh,
             "multiple_shapes_accumulate_into_one_mesh") {
    const ip_color color=rgba(0x20,0x40,0x60);Session s;s.begin();s.rounded_rect(rect(0,0,20,10),0);s.fill_color(color);s.rounded_rect(rect(30,0,50,10),0);s.fill_color(color);s.line({0,0},{0,10},1,color);const Mesh mesh=s.end();
    require(mesh.vertices.size()==14,"two rects plus line are not 14 vertices");require(mesh.indices.size()==30,"two rects plus line are not 30 indices");
}

} // namespace

int main(int argc, char **argv) { return ip_test::run(argc, argv); }
