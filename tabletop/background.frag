#version 330

in vec2 fragTexCoord;
// in vec4 fragColor;

uniform float u_time;
uniform vec2  u_resolution;
uniform float u_turn;   // 0.0 = your turn, 1.0 = opponent's turn.
uniform vec2  u_mouse;  // (0,0) to (1,1) across the screen.

out vec4 fragColor;

// Inigo Quilez (iquilezles.org/articles/palettes)
vec3 palette_iq(float t, vec3 a, vec3 b, vec3 c, vec3 d) {
  return a + b * cos(2.0 * 3.1416 * (c * t + d));
}

vec3 palette(float t, vec3 a, vec3 b, vec3 c) {
  float wa = max(-t * t * t, 0.0);
  float wb = max(t * (t - 1.0) * 2.0, 0.0);
  float wc = max(t * t * t, 0.0);
  float w  = wa + wb + wc;
  return (wa * a + wb * b + wc * c) / w;
}

// Example presets:
// Rainbow:   palette(t, vec3(0.5), vec3(0.5), vec3(1.0), vec3(0.0, 0.33, 0.67))
// Warm:      palette(t, vec3(0.5), vec3(0.5), vec3(1.0), vec3(0.0, 0.1, 0.2))
// Cool:      palette(t, vec3(0.5), vec3(0.5), vec3(1.0, 1.0, 0.5), vec3(0.8,
// 0.9, 0.3))

float eval_v(vec2 uv, float t) {
  // Layered sine waves
  // t = 0.0;
  float v1 = sin(uv.x * 3.0 + t) * cos(uv.y * 2.0 - t * 0.7);
  float v2 = sin(uv.y * 4.0 - t * 0.5) * cos(uv.x * 3.0 - t * 1.1);
  float v3 = sin((uv.y) * 2.5 - t * 0.8);
  float v  = (v1 + v2 + v3) / 3.0;
  return v;
}

vec3 combine(vec3 a, vec3 b) {
  // return a+b
  return max(a, b);
}

vec2 hash_22(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.xx + p3.yz) * p3.zy);
}

float voronoi(vec2 p) {
  vec2  i        = floor(p);
  vec2  f        = fract(p);
  float min_dist = 1.0;
  for (int y = -1; y <= 1; y++) {
    for (int x = -1; x <= 1; x++) {
      vec2  neighbor = vec2(float(x), float(y));
      vec2  point    = hash_22(i + neighbor);
      vec2  diff     = neighbor + point - f;
      float dist     = length(diff);
      min_dist       = min(min_dist, dist);
    }
  }
  return min_dist;
}

float remap(
  float value, float in_min, float in_max, float out_min, float out_max
) {
  return out_min + (out_max - out_min) * (value - in_min) / (in_max - in_min);
}

float hash_21(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float value_noise(vec2 p) {
  vec2 i  = floor(p);
  vec2 f  = fract(p);
  f       = f * f * (3.0 - 2.0 * f);
  float a = hash_21(i);
  float b = hash_21(i + vec2(1.0, 0.0));
  float c = hash_21(i + vec2(0.0, 1.0));
  float d = hash_21(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
  float value     = 0.0;
  float amplitude = 0.5;
  float frequency = 1.0;
  for (int i = 0; i < 6; i++) {
    value += amplitude * value_noise(p * frequency);
    frequency *= 2.0;
    amplitude *= 0.5;
  }
  return value;
}

vec2 rotate(vec2 x, float angle) {
  float ct = cos(angle), st = sin(angle);
  return mat2(ct, -st, st, ct) * x;
}

// Turbulence: layered sine-wave displacements with golden-ratio rotation.
// TURB_NUM, TURB_AMP, TURB_SPEED, TURB_FREQ, TURB_EXP – see Constants.
#define TURB_NUM 10.0
#define TURB_AMP 0.7
#define TURB_SPEED 0.3
#define TURB_FREQ 2.0
#define TURB_EXP 1.4

const mat2 GOLDEN_ROT2 =
  mat2(-0.73736887808, 0.67549029426, -0.67549029426, -0.73736887808);

vec2 turbulence(vec2 pos, float time, float amplitude) {
  float freq = TURB_FREQ;
  mat2  rot  = GOLDEN_ROT2;
  for (float i = 0.0; i < TURB_NUM; i++) {
    float phase = freq * (pos * rot).y + TURB_SPEED * time + i;
    pos += amplitude * rot[0] * sin(phase) / freq;
    rot *= GOLDEN_ROT2;
    freq *= TURB_EXP;
  }
  return pos;
}

void main() {
  vec2  uv   = gl_FragCoord.xy / u_resolution.x;
  float t    = u_time * 0.1;
  vec2 mouse = 2 * vec2(u_mouse.x, u_resolution.y - u_mouse.y) / u_resolution.x;
  // uv *= 2.0;
  float dist = length(uv - mouse);
  // dist = -dist;
  // dist = remap(dist, -0.7, 0.0, 0.0, 1.0);
  // dist = clamp(dist, 0.0, 1.0);
  // dist *= dist;

  dist *= 2.0;
  dist = 1 / (1 + dist * dist);
  //   fragColor = vec4(vec3(dist), 1.0);
  //   return;
  uv = turbulence(uv, u_time, TURB_AMP * dist);

  float rt     = t * 0.3;
  float value0 = eval_v(rotate(uv, rt), t * 1.0);
  float value1 = eval_v(rotate(uv * 1.1, rt + 0.1), t * 1.0);

  vec3 blue   = vec3(0.1, 0.1, 0.8);
  vec3 orange = vec3(0.8, 0.3, 0.1);
  orange      = mix(orange, orange.yzx, u_turn);
  blue        = mix(blue, blue.yzx, u_turn);
  // orange = orange.yzx;
  // blue = blue.yzx;
  // xyz
  // xzy
  // yxz nice
  // yzx nice
  // zxy
  // zyx

  vec3 a = value0 * blue;
  vec3 b = value1 * orange;
  //   float vor = (1.0 - voronoi(uv * 10.0 + t));
  //   vor       = remap(vor, 0.0, 1.0, 0.5, 1.0);
  // b *= vor;

  //   float noise = fbm(uv + t * 0.5);
  // a *= noise;

  vec3 col = combine(a, b);
  // col = max(col, 0.1 * fbm(uv + t * 0.1));

  // Smoothly invert colors when the turn passes to the opponent.
  // col = mix(col, 1.0 - col, u_turn);

  fragColor = vec4(col, 1.0);
}