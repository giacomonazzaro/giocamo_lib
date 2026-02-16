#version 330

in vec2 fragTexCoord;
// in vec4 fragColor;

uniform float u_time;
uniform vec2 u_resolution;

out vec4 fragColor;

// Inigo Quilez (iquilezles.org/articles/palettes)
vec3 palette_iq(float t, vec3 a, vec3 b, vec3 c, vec3 d)
{
    return a + b * cos(2.0 * 3.1416 * (c * t + d));
}

vec3 palette(float t, vec3 a, vec3 b, vec3 c)
{
    float wa = max(-t*t*t, 0.0);
    float wb = max(t * (t-1.0) * 2.0, 0.0);
    float wc = max(t*t*t, 0.0);
    float w = wa + wb + wc;
    return (wa * a + wb * b + wc * c) / w;
}

// Example presets:
// Rainbow:   palette(t, vec3(0.5), vec3(0.5), vec3(1.0), vec3(0.0, 0.33, 0.67))
// Warm:      palette(t, vec3(0.5), vec3(0.5), vec3(1.0), vec3(0.0, 0.1, 0.2))
// Cool:      palette(t, vec3(0.5), vec3(0.5), vec3(1.0, 1.0, 0.5), vec3(0.8, 0.9, 0.3))

float eval_v(vec2 uv, float t) {
    // Layered sine waves
    // t = 0.0;
    float v1 = sin(uv.x * 3.0 + t) * cos(uv.y * 2.0 - t * 0.7);
    float v2 = sin(uv.y * 4.0 - t * 0.5) * cos(uv.x * 3.0 - t * 1.1);
    float v3 = sin((uv.y) * 2.5 - t * 0.8);
    float v = (v1 + v2 + v3) / 3.0;
    return v;
}

vec3 combine(vec3 a, vec3 b) {
    vec3 s = a + b;
    return max(a, b);
}


vec2 hash_22(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}


float voronoi(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    float min_dist = 1.0;
    for (int y = -1; y <= 1; y++)
    {
        for (int x = -1; x <= 1; x++)
        {
            vec2 neighbor = vec2(float(x), float(y));
            vec2 point = hash_22(i + neighbor);
            vec2 diff = neighbor + point - f;
            float dist = length(diff);
            min_dist = min(min_dist, dist);
        }
    }
    return min_dist;
}


float remap(float value, float in_min, float in_max, float out_min, float out_max)
{
    return out_min + (out_max - out_min) * (value - in_min) / (in_max - in_min);
}

float hash_21(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}


float value_noise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash_21(i);
    float b = hash_21(i + vec2(1.0, 0.0));
    float c = hash_21(i + vec2(0.0, 1.0));
    float d = hash_21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    for (int i = 0; i < 6; i++)
    {
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

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution; 
    float t = u_time * 0.5;
    // uv *= 2.0;

    float rt = t * 0.3;
    float blue = eval_v(rotate(uv, rt), t*1.0);
    float orange = eval_v(rotate(uv * 1.1, rt + 0.1), t*1.0);

    vec3 a = blue * vec3(0.1, 0.1, 0.8);
    vec3 b = orange * vec3(0.8, 0.3, 0.1);
    float vor = (1.0 - voronoi(uv * 10.0 + t));
    vor = remap(vor, 0.0, 1.0, 0.5, 1.0);
    // b *= vor;    
    
    float noise = fbm(uv + t * 0.5);
    //a *= noise;
    
    vec3 col = combine(a, b);
    col = max(col, 0.1 * fbm(uv + t * 0.1));
    float dd = vor;
    //col = vec3();
    //col *= vor;
    //col = vec3(vor);
    // Dark shifting color palette
    // vec3 col;
    // col.r = 0.08 + 0.04 * sin(v * 3.14);
    // col.g = 0.09 + 0.04 * sin(v * 3.14 + 0.5);
    // col.b = 0.13 + 0.06 * sin(v * 3.14 + 1.0);

    fragColor = vec4(col, 1.0);
}