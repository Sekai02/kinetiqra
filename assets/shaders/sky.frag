#version 450 core

// The world the model is standing in, as far as its reflections are concerned.
//
// A gradient rather than a picture: bright above, dimmer towards the horizon,
// and dark and slightly warm below, which is what both a room and an overcast
// day look like to a reflective surface. Nothing is loaded and nothing is
// licensed, and the passes that consume this do not care where it came from, so
// a real panorama can replace it later without touching them.
//
// There is no sun in it. The scene already has a directional light, and putting
// one here as well would count the same light twice: the highlight would be too
// hot and the metal too bright.

in vec3 v_direction;

out vec4 o_colour;

const vec3 kZenith = vec3(0.42, 0.52, 0.72);
const vec3 kHorizon = vec3(0.62, 0.64, 0.68);
const vec3 kGround = vec3(0.19, 0.17, 0.15);

void main() {
    float height = normalize(v_direction).y;

    // Values above one on purpose. A sky is a light source, and one that never
    // exceeded what a screen can show would leave nothing for the tone curve to
    // roll off and no brightness to reflect.
    vec3 sky = mix(kHorizon, kZenith, pow(clamp(height, 0.0, 1.0), 0.5)) * 1.6;
    vec3 ground = mix(kHorizon * 0.6, kGround, pow(clamp(-height, 0.0, 1.0), 0.35));

    o_colour = vec4(height > 0.0 ? sky : ground, 1.0);
}
