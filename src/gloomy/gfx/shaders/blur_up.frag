#version 450
// Dual-filter (Kawase) upsample: eight taps of the smaller source into a
// target twice its size.

layout(location = 0) in vec2 v_uv;
layout(set = 0, binding = 0) uniform sampler2D u_source;

layout(push_constant) uniform Push {
  // Half a texel of the source.
  vec2 half_texel;
} pc;

layout(location = 0) out vec4 o_color;

void main() {
  vec2 h = pc.half_texel;
  vec4 sum = texture(u_source, v_uv + vec2(-h.x * 2.0, 0.0));
  sum += texture(u_source, v_uv + vec2(-h.x, h.y)) * 2.0;
  sum += texture(u_source, v_uv + vec2(0.0, h.y * 2.0));
  sum += texture(u_source, v_uv + vec2(h.x, h.y)) * 2.0;
  sum += texture(u_source, v_uv + vec2(h.x * 2.0, 0.0));
  sum += texture(u_source, v_uv + vec2(h.x, -h.y)) * 2.0;
  sum += texture(u_source, v_uv + vec2(0.0, -h.y * 2.0));
  sum += texture(u_source, v_uv + vec2(-h.x, -h.y)) * 2.0;
  o_color = vec4((sum / 12.0).rgb, 1.0);
}
