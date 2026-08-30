#version 450 core

// One flat colour. This draws the selection, and a selection that were shaded
// would be telling the user about the light rather than about what is selected.

out vec4 o_colour;

uniform vec4 u_colour;

void main() {
    o_colour = u_colour;
}
