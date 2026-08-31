#version 450 core

// What a rough surface facing this direction receives from the whole sky.
//
// This is the ambient term, and it replaces the single constant the mesh shader
// used to add everywhere. A constant says the light is the same from every
// direction, which is why a model lit that way looks flat and pasted on. This
// says where the light actually comes from, so the top of a surface picks up
// the sky and the underside picks up the ground.
//
// Every direction of the hemisphere around the normal is sampled and weighted
// by how much of it the surface faces. Small enough to be brute force: the
// result is 32 pixels a face and it is computed once, at startup.

in vec3 v_direction;

out vec4 o_colour;

layout(binding = 0) uniform samplerCube u_sky;

const float kPi = 3.14159265359;
const float kStep = 0.025;

void main() {
    vec3 normal = normalize(v_direction);

    // Any two directions square to the normal and to each other. Which two does
    // not matter, since the whole hemisphere is walked.
    vec3 up = abs(normal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 right = normalize(cross(up, normal));
    up = cross(normal, right);

    vec3 total = vec3(0.0);
    float samples = 0.0;

    for (float around = 0.0; around < 2.0 * kPi; around += kStep) {
        for (float away = 0.0; away < 0.5 * kPi; away += kStep) {
            vec3 local = vec3(sin(away) * cos(around), sin(away) * sin(around), cos(away));
            vec3 direction = (right * local.x) + (up * local.y) + (normal * local.z);

            // cos weights how much the surface faces this direction, sin is the
            // shrinking area of the ring at this angle from the pole.
            total += texture(u_sky, direction).rgb * cos(away) * sin(away);
            samples += 1.0;
        }
    }

    o_colour = vec4(kPi * total / max(samples, 1.0), 1.0);
}
