#version 450 core

// glTF's metallic-roughness material, lit by a single directional light.
//
// Direct lighting only. Without an environment to reflect, a metal surface has
// nothing to show but that one light, so metal reads dark here rather than as
// chrome. That is the honest limit of one light, and the place an environment
// map would go later.

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;
in vec4 v_tangent;

out vec4 o_colour;

uniform vec3 u_camera_position;

uniform vec4 u_base_colour;
uniform float u_metallic;
uniform float u_roughness;
uniform vec3 u_emissive;
uniform float u_normal_scale;
uniform float u_occlusion_strength;

// 0 opaque, 1 masked, 2 blended. Masked throws the fragment away below the
// cutoff, which is how foliage and hair cards are drawn.
uniform int u_alpha_mode;
uniform float u_alpha_cutoff;

layout(binding = 0) uniform sampler2D u_base_colour_map;
layout(binding = 1) uniform sampler2D u_metallic_roughness_map;
layout(binding = 2) uniform sampler2D u_normal_map;
layout(binding = 3) uniform sampler2D u_occlusion_map;
layout(binding = 4) uniform sampler2D u_emissive_map;

const vec3 kLightDirection = normalize(vec3(-0.4, -1.0, -0.6));
const vec3 kLightColour = vec3(1.0);
const vec3 kAmbient = vec3(0.20, 0.21, 0.24);
const float kPi = 3.14159265359;

// How much of the surface is turned towards the halfway direction. The sharp
// peak is what makes a smooth surface show a small bright highlight and a rough
// one a wide dull sheen.
float distribution(vec3 normal, vec3 halfway, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float cosine = max(dot(normal, halfway), 0.0);
    float denominator = (cosine * cosine * (a2 - 1.0)) + 1.0;
    return a2 / max(kPi * denominator * denominator, 1e-7);
}

// How much of that is hidden behind the bumps of the surface itself, which is
// what stops a rough surface glowing at a grazing angle.
float occlusion(float cosine, float roughness) {
    float k = ((roughness + 1.0) * (roughness + 1.0)) / 8.0;
    return cosine / ((cosine * (1.0 - k)) + k);
}

vec3 fresnel(float cosine, vec3 reflectance) {
    return reflectance + ((1.0 - reflectance) * pow(clamp(1.0 - cosine, 0.0, 1.0), 5.0));
}

vec3 surface_normal() {
    vec3 normal = normalize(v_normal);
    vec3 tangent = normalize(v_tangent.xyz - (normal * dot(normal, v_tangent.xyz)));

    if (length(v_tangent.xyz) <= 0.0) {
        return normal;
    }

    // The third axis, whose direction depends on whether the UV island was
    // mirrored. That is what the w component of the tangent records.
    vec3 bitangent = cross(normal, tangent) * v_tangent.w;

    vec3 sampled = (texture(u_normal_map, v_uv).xyz * 2.0) - 1.0;
    sampled.xy *= u_normal_scale;

    return normalize(mat3(tangent, bitangent, normal) * sampled);
}

void main() {
    vec4 base = u_base_colour * texture(u_base_colour_map, v_uv);

    if (u_alpha_mode == 1 && base.a < u_alpha_cutoff) {
        discard;
    }

    // Metalness in blue and roughness in green, which is how glTF packs them.
    // Not called "packed": that is a reserved word in GLSL.
    vec3 surface = texture(u_metallic_roughness_map, v_uv).rgb;
    float metallic = clamp(u_metallic * surface.b, 0.0, 1.0);
    float roughness = clamp(u_roughness * surface.g, 0.04, 1.0);

    vec3 normal = surface_normal();

    // Two sided, so a face wound the wrong way is still lit rather than black,
    // which keeps a winding mistake visible as a shape rather than as a hole.
    vec3 view = normalize(u_camera_position - v_world_position);
    if (dot(normal, view) < 0.0) {
        normal = -normal;
    }

    vec3 light = -kLightDirection;
    vec3 halfway = normalize(view + light);

    float normal_dot_light = max(dot(normal, light), 0.0);
    float normal_dot_view = max(dot(normal, view), 1e-4);

    // Metals take their reflected colour from the surface; everything else
    // reflects a plain four percent of the light straight back.
    vec3 reflectance = mix(vec3(0.04), base.rgb, metallic);

    float d = distribution(normal, halfway, roughness);
    float g = occlusion(normal_dot_light, roughness) * occlusion(normal_dot_view, roughness);
    vec3 f = fresnel(max(dot(halfway, view), 0.0), reflectance);

    vec3 specular = (d * g * f) / max(4.0 * normal_dot_light * normal_dot_view, 1e-7);

    // What is not reflected is what gets through to be scattered, and a metal
    // scatters nothing.
    vec3 diffuse = (1.0 - f) * (1.0 - metallic) * base.rgb / kPi;

    vec3 lit = (diffuse + specular) * kLightColour * normal_dot_light;

    float shadowed = texture(u_occlusion_map, v_uv).r;
    vec3 ambient = kAmbient * base.rgb * mix(1.0, shadowed, u_occlusion_strength);

    vec3 emitted = u_emissive * texture(u_emissive_map, v_uv).rgb;

    o_colour = vec4(lit + ambient + emitted, u_alpha_mode == 2 ? base.a : 1.0);
}
