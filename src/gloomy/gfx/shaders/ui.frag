#version 450
// gloomy UI fragment shader: signed distance shapes and atlas glyphs,
// clipped per instance, premultiplied alpha output.

layout(location = 0) in vec2 v_local;
layout(location = 1) flat in vec2 v_half;
layout(location = 2) in vec2 v_uv;
layout(location = 3) flat in vec4 v_color;
layout(location = 4) flat in vec4 v_color2;
layout(location = 5) flat in vec4 v_radii;
layout(location = 6) flat in vec4 v_clip;
layout(location = 7) flat in vec2 v_params;
layout(location = 8) flat in uint v_kind;

layout(set = 0, binding = 0) uniform sampler2DArray u_atlas;

layout(location = 0) out vec4 o_color;

const uint kRect = 0u;
const uint kCircle = 1u;
const uint kLine = 2u;
const uint kGlyph = 3u;

// Rounded box; radii are top-left, top-right, bottom-right, bottom-left
// with y pointing down.
float RoundedBox(vec2 p, vec2 b, vec4 r) {
  float rr = p.x < 0.0 ? (p.y < 0.0 ? r.x : r.w) : (p.y < 0.0 ? r.y : r.z);
  rr = min(rr, min(b.x, b.y));
  vec2 q = abs(p) - b + rr;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - rr;
}

// Coverage of a pixel at signed distance d from the edge.
float Coverage(float d, float soft) {
  if (soft > 0.0) {
    return 1.0 - smoothstep(-soft, soft, d);
  }
  return clamp(0.5 - d, 0.0, 1.0);
}

void main() {
  uint kind = v_kind & 0xFFu;
  float stroke = v_params.x;
  float soft = v_params.y;
  vec4 color = v_color;
  float alpha;
  if (kind == kGlyph) {
    float page = float(v_kind >> 8u);
    alpha = texture(u_atlas, vec3(v_uv, page)).r;
  } else {
    float d;
    if (kind == kCircle) {
      d = length(v_local) - v_half.x;
    } else if (kind == kLine) {
      d = RoundedBox(v_local, v_half, vec4(0.0));
    } else {
      d = RoundedBox(v_local, v_half, v_radii);
      float t = clamp(v_local.y / max(2.0 * v_half.y, 1e-4) + 0.5, 0.0, 1.0);
      color = mix(v_color, v_color2, t);
    }
    alpha = Coverage(d, soft);
    if (stroke > 0.0) {
      alpha -= Coverage(d + stroke, soft);
    }
  }
  // Pixel-area coverage of the clip rectangle, so fractional clips
  // antialias too.
  vec2 p = gl_FragCoord.xy;
  vec2 lo = max(p - 0.5, v_clip.xy);
  vec2 hi = min(p + 0.5, v_clip.zw);
  vec2 cov = clamp(hi - lo, 0.0, 1.0);
  alpha *= cov.x * cov.y * color.a;
  if (alpha <= 0.0) {
    discard;
  }
  o_color = vec4(color.rgb * alpha, alpha);
}
