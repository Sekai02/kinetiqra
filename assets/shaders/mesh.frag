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

// The world around the model: what it receives from every direction, and what
// it reflects at each roughness. These replace the constant ambient term that
// used to be added everywhere, which said the light was the same from all sides
// and made everything look flat and pasted on.
layout(binding = 5) uniform samplerCube u_irradiance;
layout(binding = 6) uniform samplerCube u_reflection;

uniform float u_reflection_levels;

const vec3 kLightDirection = normalize(vec3(-0.4, -1.0, -0.6));
const vec3 kLightColour = vec3(1.0);
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

// The filmic curve from ACES, in the fitted form that is five multiplications
// rather than a matrix and a spline.
//
// Lighting produces values that run past one wherever something is bright, and
// a display cannot show those. Clipping them turns every highlight into a flat
// white patch; this rolls them off instead, which is what makes a bright scene
// read as bright rather than as burnt.
vec3 tone_map(vec3 colour) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((colour * ((a * colour) + b)) / ((colour * ((c * colour) + d)) + e), 0.0, 1.0);
}

// Linear light to what a display expects.
//
// Textures are straightened on the way in and every calculation above happens
// in linear light, which is the only space in which adding two lights together
// means anything. The screen does not work that way, so the result has to be
// bent back at the very end. Without this the whole image is shown darker than
// it is, which is not a subtle effect.
vec3 to_display(vec3 colour) {
    bvec3 small = lessThanEqual(colour, vec3(0.0031308));
    vec3 low = colour * 12.92;
    vec3 high = (1.055 * pow(colour, vec3(1.0 / 2.4))) - 0.055;
    return mix(high, low, vec3(small));
}

// Fresnel again, but for a whole environment rather than one direction. A rough
// surface averages over so many directions that the sharp rise at a grazing
// angle is worn down, and using the sharp one here makes rough metal glow at
// its edges.
vec3 fresnel_rough(float cosine, vec3 reflectance, float roughness) {
    vec3 ceiling = max(vec3(1.0 - roughness), reflectance);
    return reflectance + ((ceiling - reflectance) * pow(clamp(1.0 - cosine, 0.0, 1.0), 5.0));
}

// How much of a reflection survives, as a scale and an offset on the surface's
// reflectance. Karis' fit to the integral that is usually baked into a texture.
vec2 environment_brdf(float cosine, float roughness) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);

    vec4 r = (roughness * c0) + c1;
    float a004 = (min(r.x * r.x, exp2(-9.28 * cosine)) * r.x) + r.y;

    return (vec2(-1.04, 1.04) * a004) + r.zw;
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

    // What the surroundings contribute. This is the whole point of an
    // environment: a metal shows almost nothing of its own colour, so without
    // this a metal is black however bright the scene is.
    vec3 from_environment = fresnel_rough(normal_dot_view, reflectance, roughness);

    vec3 scattered = texture(u_irradiance, normal).rgb * base.rgb;
    scattered *= (1.0 - from_environment) * (1.0 - metallic);

    // Rougher surfaces read from a blurrier level of the same map, which is
    // what the levels were prefiltered for.
    vec3 mirrored = reflect(-view, normal);
    vec3 reflected = textureLod(u_reflection, mirrored,
                                roughness * (u_reflection_levels - 1.0)).rgb;

    // The split sum approximation, in the fitted form that needs no lookup
    // table. Close enough that the difference does not show under one light,
    // and it saves a texture and a pass to build it.
    vec2 fit = environment_brdf(normal_dot_view, roughness);
    reflected *= (reflectance * fit.x) + fit.y;

    float shadowed = texture(u_occlusion_map, v_uv).r;
    vec3 ambient =
        (scattered + reflected) * mix(1.0, shadowed, u_occlusion_strength);

    vec3 emitted = u_emissive * texture(u_emissive_map, v_uv).rgb;

    vec3 colour = to_display(tone_map(lit + ambient + emitted));

    o_colour = vec4(colour, u_alpha_mode == 2 ? base.a : 1.0);
}
