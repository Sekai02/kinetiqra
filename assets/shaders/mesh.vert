#version 450 core

// Attribute locations match the order geom::bake interleaves them.
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_tangent;

out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;
out vec4 v_tangent;

uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform mat4 u_normal_matrix;

void main() {
    vec4 world = u_model * vec4(a_position, 1.0);

    v_world_position = world.xyz;
    v_normal = mat3(u_normal_matrix) * a_normal;

    // The tangent lies in the surface, so it is carried by the model matrix
    // rather than by the normal matrix, which is for directions perpendicular
    // to it. The handedness in w belongs to the UVs and is passed straight on.
    v_tangent = vec4(mat3(u_model) * a_tangent.xyz, a_tangent.w);
    v_uv = a_uv;

    gl_Position = u_view_projection * world;
}
