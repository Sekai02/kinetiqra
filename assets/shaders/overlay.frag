#version 450 core

// This writes display colours directly, with no tone mapping and no encode.
// That is deliberate: the grid and the selection are interface drawn into the
// world, not surfaces being lit, so there is no linear light here to convert
// back from. Correcting them to match the mesh shader would wash them out.

// One flat colour. This draws the selection, and a selection that were shaded
// would be telling the user about the light rather than about what is selected.

out vec4 o_colour;

uniform vec4 u_colour;

void main() {
    o_colour = u_colour;
}
