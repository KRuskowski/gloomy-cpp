#version 450
// gloomy UI vertex shader: expands one instance into a quad.
// Instance layout: include/gloomy/gfx/draw_list.h (Instance).

layout(location = 0) in vec4 a_rect;
layout(location = 1) in vec4 a_uv;
layout(location = 2) in vec4 a_radii;
layout(location = 3) in vec4 a_clip;
layout(location = 4) in uint a_color;
layout(location = 5) in uint a_color2;
layout(location = 6) in float a_stroke;
layout(location = 7) in float a_soft;
layout(location = 8) in uint a_kind;

// Shared with ui.frag.
layout(push_constant) uniform Push {
  vec2 viewport;
  // 1 when a blurred backdrop is bound for glass, else 0.
  float has_backdrop;
  float pad;
} pc;

layout(location = 0) out vec2 v_local;
layout(location = 1) flat out vec2 v_half;
layout(location = 2) out vec2 v_uv;
layout(location = 3) flat out vec4 v_color;
layout(location = 4) flat out vec4 v_color2;
layout(location = 5) flat out vec4 v_radii;
layout(location = 6) flat out vec4 v_clip;
layout(location = 7) flat out vec2 v_params;
layout(location = 8) flat out uint v_kind;
layout(location = 9) flat out vec2 v_glass;

const vec2 kCorners[6] = vec2[](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

const uint kRect = 0u;
const uint kCircle = 1u;
const uint kLine = 2u;
const uint kGlyph = 3u;
const uint kGlass = 4u;
const uint kHexagon = 5u;
const uint kTriangle = 6u;

void main() {
  vec2 corner = kCorners[gl_VertexIndex];
  uint kind = a_kind & 0xFFu;
  vec2 pos;
  v_uv = vec2(0.0);
  v_glass = vec2(0.0);
  if (kind == kLine) {
    // Quad along the line, one pixel of antialiasing margin around it.
    vec2 a = a_rect.xy;
    vec2 b = a_rect.zw;
    vec2 d = b - a;
    float len = length(d);
    vec2 dir = len > 0.0 ? d / len : vec2(1.0, 0.0);
    vec2 nrm = vec2(-dir.y, dir.x);
    vec2 half_size = vec2(len * 0.5, a_radii.x * 0.5);
    vec2 local = (corner * 2.0 - 1.0) * (half_size + 1.0);
    pos = (a + b) * 0.5 + dir * local.x + nrm * local.y;
    v_local = local;
    v_half = half_size;
  } else if (kind == kTriangle) {
    // The bounds and one antialiasing pixel; the fragment shader measures
    // from the vertices in pixels (radii: first two, uv: third).
    vec2 lo = a_rect.xy - 1.0;
    vec2 hi = a_rect.zw + 1.0;
    pos = mix(lo, hi, corner);
    v_local = pos;
    v_half = vec2(0.0);
    v_glass = a_uv.xy;
  } else if (kind == kGlyph) {
    pos = mix(a_rect.xy, a_rect.zw, corner);
    v_uv = mix(a_uv.xy, a_uv.zw, corner);
    v_local = vec2(0.0);
    v_half = vec2(0.0);
  } else {
    // Rects and circles grow by the softness plus one antialiasing pixel.
    vec2 center = (a_rect.xy + a_rect.zw) * 0.5;
    vec2 half_size = (a_rect.zw - a_rect.xy) * 0.5;
    vec2 local = (corner * 2.0 - 1.0) * (half_size + a_soft + 1.0);
    pos = center + local;
    v_local = local;
    v_half = half_size;
    if (kind == kGlass) {
      // Glass: darken, noise, rim strength and width.
      v_uv = a_uv.xy;
      v_glass = a_uv.zw;
    }
  }
  v_color = unpackUnorm4x8(a_color);
  v_color2 = unpackUnorm4x8(a_color2);
  v_radii = a_radii;
  v_clip = a_clip;
  v_params = vec2(a_stroke, a_soft);
  v_kind = a_kind;
  gl_Position = vec4(pos / pc.viewport * 2.0 - 1.0, 0.0, 1.0);
}
