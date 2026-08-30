#version 450 core

// Positions only. The overlay says where something is, not what it looks like,
// so it needs none of the attributes the mesh shader reads.
layout(location = 0) in vec3 a_position;

uniform mat4 u_model;
uniform mat4 u_view_projection;
uniform float u_point_size;

void main() {
    gl_Position = u_view_projection * u_model * vec4(a_position, 1.0);
    gl_PointSize = u_point_size;
}
