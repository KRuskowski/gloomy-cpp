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
layout(location = 9) flat in vec2 v_glass;

layout(set = 0, binding = 0) uniform sampler2DArray u_atlas;
// The host scene, blurred (BackdropBlur), for glass; a 1x1 placeholder
// when there is none.
layout(set = 1, binding = 0) uniform sampler2D u_backdrop;

// Shared with ui.vert.
layout(push_constant) uniform Push {
  vec2 viewport;
  float has_backdrop;
  float pad;
} pc;

layout(location = 0) out vec4 o_color;

const uint kRect = 0u;
const uint kCircle = 1u;
const uint kLine = 2u;
const uint kGlyph = 3u;
const uint kGlass = 4u;
const uint kHexagon = 5u;

// Rounded box; radii are top-left, top-right, bottom-right, bottom-left
// with y pointing down.
float RoundedBox(vec2 p, vec2 b, vec4 r) {
  float rr = p.x < 0.0 ? (p.y < 0.0 ? r.x : r.w) : (p.y < 0.0 ? r.y : r.z);
  rr = min(rr, min(b.x, b.y));
  vec2 q = abs(p) - b + rr;
  return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - rr;
}

// Regular hexagon with inradius r, flat top and bottom (vertices at
// x = +-r / cos(30 deg)).
float Hexagon(vec2 p, float r) {
  const vec3 k = vec3(-0.866025404, 0.5, 0.577350269);
  p = abs(p);
  p -= 2.0 * min(dot(k.xy, p), 0.0) * k.xy;
  p -= vec2(clamp(p.x, -k.z * r, k.z * r), r);
  return length(p) * sign(p.y);
}

// Coverage of a pixel at signed distance d from the edge.
float Coverage(float d, float soft) {
  if (soft > 0.0) {
    return 1.0 - smoothstep(-soft, soft, d);
  }
  return clamp(0.5 - d, 0.0, 1.0);
}

// A hash in [-0.5, 0.5) per pixel, for dithering.
float Dither(vec2 p) {
  return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
}

// Frosted glass at signed distance d inside its box: the blurred backdrop,
// darkened, under the tint, with a light rim strongest along the top.
vec4 Glass(float d) {
  if (pc.has_backdrop < 0.5) {
    return v_color2;
  }
  float darken = v_uv.x;
  float noise = v_uv.y;
  float rim = v_glass.x;
  float rim_width = max(v_glass.y, 0.5);
  vec3 back = texture(u_backdrop, gl_FragCoord.xy / pc.viewport).rgb;
  vec3 rgb = mix(back * (1.0 - darken), v_color.rgb, v_color.a);
  float edge = 1.0 - smoothstep(0.0, rim_width, -d);
  float top = clamp((v_local.y + v_half.y) / max(2.0 * v_half.y, 1.0), 0.0,
                    1.0);
  rgb += vec3(rim * edge * mix(1.0, 0.3, top));
  rgb += vec3(Dither(gl_FragCoord.xy) * noise / 255.0);
  return vec4(clamp(rgb, 0.0, 1.0), 1.0);
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
    } else if (kind == kHexagon) {
      // v_half.x is the circumradius; pointy-top turns it a quarter.
      vec2 q = v_radii.x > 0.5 ? v_local.yx : v_local;
      d = Hexagon(q, v_half.x * 0.866025404);
    } else if (kind == kLine) {
      d = RoundedBox(v_local, v_half, vec4(0.0));
    } else if (kind == kGlass) {
      d = RoundedBox(v_local, v_half, v_radii);
      color = Glass(d);
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
