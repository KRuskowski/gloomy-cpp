#version 450
// Dual-filter (Kawase) downsample: five taps of the larger source into a
// target half its size.

layout(location = 0) in vec2 v_uv;
layout(set = 0, binding = 0) uniform sampler2D u_source;

layout(push_constant) uniform Push {
  // Half a texel of the source.
  vec2 half_texel;
} pc;

layout(location = 0) out vec4 o_color;

void main() {
  vec2 h = pc.half_texel;
  vec4 sum = texture(u_source, v_uv) * 4.0;
  sum += texture(u_source, v_uv - h);
  sum += texture(u_source, v_uv + h);
  sum += texture(u_source, v_uv + vec2(h.x, -h.y));
  sum += texture(u_source, v_uv - vec2(h.x, -h.y));
  o_color = vec4((sum / 8.0).rgb, 1.0);
}
