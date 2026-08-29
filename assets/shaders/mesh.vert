#version 450 core

// Attribute locations match the order geom::bake interleaves them.
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;

out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;

uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform mat4 u_normal_matrix;

void main() {
    vec4 world = u_model * vec4(a_position, 1.0);

    v_world_position = world.xyz;
    v_normal = mat3(u_normal_matrix) * a_normal;
    v_uv = a_uv;

    gl_Position = u_view_projection * world;
}
