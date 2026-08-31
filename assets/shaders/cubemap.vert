#version 450 core

// One triangle covering a face of a cubemap, with the direction that face looks
// in worked out per pixel.
//
// No geometry is bound. The vertex is built from its own index, the same trick
// the grid uses, so drawing a face is one call and no buffer.

out vec3 v_direction;

// The three axes of the face being drawn: where its right, its up and its
// forward point in the world. The caller sets them once per face.
uniform vec3 u_right;
uniform vec3 u_up;
uniform vec3 u_forward;

void main() {
    // A triangle twice the size of the screen rather than two making a quad:
    // no seam down the diagonal, and one primitive instead of two.
    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vec2 position = (corner * 2.0) - 1.0;

    v_direction = normalize(u_forward + (u_right * position.x) + (u_up * position.y));

    gl_Position = vec4(position, 0.0, 1.0);
}
