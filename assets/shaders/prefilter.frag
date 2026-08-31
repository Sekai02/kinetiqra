#version 450 core

// The sky as a given roughness would reflect it.
//
// A mirror reflects one direction, and a rough surface reflects a cone that
// widens as it roughens. Rather than gathering that cone while drawing, which
// would be hundreds of samples a pixel, it is gathered once here into the mips
// of a cubemap: sharp at the top level and progressively smeared further down.
// Drawing then picks a level from the roughness and gets a blur that was paid
// for at startup.
//
// The directions sampled come from the same GGX distribution the direct light
// uses, so the reflection and the highlight agree about what the surface is.

in vec3 v_direction;

out vec4 o_colour;

layout(binding = 0) uniform samplerCube u_sky;

uniform float u_roughness;

const float kPi = 3.14159265359;
const uint kSamples = 128u;

// A low discrepancy sequence: points that spread evenly instead of clumping the
// way random ones do, which is what lets this look smooth at 128 samples rather
// than at thousands.
float radical_inverse(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec3 importance_sample(vec2 random, vec3 normal, float roughness) {
    float a = roughness * roughness;

    float phi = 2.0 * kPi * random.x;
    float cos_theta = sqrt((1.0 - random.y) / (1.0 + (((a * a) - 1.0) * random.y)));
    float sin_theta = sqrt(1.0 - (cos_theta * cos_theta));

    vec3 local = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos_theta);

    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    vec3 forward = cross(normal, right);

    return normalize((right * local.x) + (forward * local.y) + (normal * local.z));
}

void main() {
    vec3 normal = normalize(v_direction);

    // Assuming the surface is being looked at head on. It is what every engine
    // does here, and the cost is that a reflection seen at a grazing angle is
    // rounder than it should be.
    vec3 view = normal;

    vec3 total = vec3(0.0);
    float weight = 0.0;

    for (uint i = 0u; i < kSamples; ++i) {
        vec2 random = vec2(float(i) / float(kSamples), radical_inverse(i));
        vec3 halfway = importance_sample(random, normal, u_roughness);
        vec3 light = normalize((2.0 * dot(view, halfway) * halfway) - view);

        float facing = dot(normal, light);
        if (facing > 0.0) {
            total += texture(u_sky, light).rgb * facing;
            weight += facing;
        }
    }

    o_colour = vec4(total / max(weight, 0.001), 1.0);
}
