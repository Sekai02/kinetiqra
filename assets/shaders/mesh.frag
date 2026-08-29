#version 450 core

// A single directional light. Not a material system, just enough shading to
// read the shape: with per-corner normals the faces come out flat and the edges
// hard, and if the bake ever merged corners it should not, this is where it
// would show up as a rounded box.

in vec3 v_world_position;
in vec3 v_normal;
in vec2 v_uv;

out vec4 o_colour;

uniform vec3 u_camera_position;

const vec3 kLightDirection = normalize(vec3(-0.4, -1.0, -0.6));
const vec3 kBaseColour = vec3(0.72, 0.73, 0.76);
const vec3 kAmbient = vec3(0.20, 0.21, 0.24);

void main() {
    vec3 normal = normalize(v_normal);

    // Two-sided, so a face wound the wrong way is still lit rather than black,
    // which keeps a winding mistake visible as a shape rather than a hole.
    vec3 view_direction = normalize(u_camera_position - v_world_position);
    if (dot(normal, view_direction) < 0.0) {
        normal = -normal;
    }

    float diffuse = max(dot(normal, -kLightDirection), 0.0);

    vec3 colour = kBaseColour * (kAmbient + vec3(diffuse) * 0.85);

    o_colour = vec4(colour, 1.0);
}
