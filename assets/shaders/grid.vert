#version 450 core

// A fullscreen triangle generated from the vertex index alone, so the draw call
// needs no vertex buffer. Three vertices covering the screen beat two triangles
// because the diagonal seam disappears along with the shared edge.

out vec2 v_ndc;

void main() {
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    v_ndc = positions[gl_VertexID];
    gl_Position = vec4(v_ndc, 0.0, 1.0);
}
