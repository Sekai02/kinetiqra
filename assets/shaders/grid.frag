#version 450 core

// This writes display colours directly, with no tone mapping and no encode.
// That is deliberate: the grid and the selection are interface drawn into the
// world, not surfaces being lit, so there is no linear light here to convert
// back from. Correcting them to match the mesh shader would wash them out.

// An infinite ground plane, drawn by intersecting each pixel's view ray with
// Y = 0 rather than by storing any geometry. That is what lets it reach the
// horizon: there is no mesh to run out of.
//
// The engine works in metres and Y is up, so one fine cell is one metre and the
// plane is the world's floor. See docs/INVARIANTS.md.

in vec2 v_ndc;

out vec4 o_colour;

uniform mat4 u_view_projection;
uniform mat4 u_inverse_view_projection;
uniform vec3 u_camera_position;
uniform float u_far_plane;

const float kFineSpacing = 1.0;    // metres
const float kCoarseSpacing = 10.0; // metres

const vec3 kFineColour = vec3(0.30, 0.31, 0.34);
const vec3 kCoarseColour = vec3(0.42, 0.43, 0.47);
const vec3 kAxisXColour = vec3(0.79, 0.32, 0.31);
const vec3 kAxisZColour = vec3(0.24, 0.45, 0.79);

// Coverage of the nearest grid line, antialiased by the rate at which the
// coordinate changes between neighbouring pixels. Without that derivative the
// lines alias into moire the moment the camera tilts.
float line_coverage(vec2 plane_position, float spacing) {
    vec2 scaled = plane_position / spacing;
    vec2 width = fwidth(scaled);
    vec2 distance_to_line = abs(fract(scaled - 0.5) - 0.5) / max(width, vec2(1e-6));
    return 1.0 - min(min(distance_to_line.x, distance_to_line.y), 1.0);
}

// Coverage of a single line along one axis, used for the coloured world axes.
float axis_coverage(float coordinate) {
    float width = fwidth(coordinate);
    return 1.0 - min(abs(coordinate) / max(width, 1e-6), 1.0);
}

vec3 unproject(vec2 ndc, float depth) {
    vec4 point = u_inverse_view_projection * vec4(ndc, depth, 1.0);
    return point.xyz / point.w;
}

void main() {
    // Two points on this pixel's ray, taken from the near and far planes.
    vec3 near_point = unproject(v_ndc, -1.0);
    vec3 far_point = unproject(v_ndc, 1.0);
    vec3 direction = far_point - near_point;

    // Where the ray meets Y = 0. Rays travelling away from the plane never do.
    float t = -near_point.y / direction.y;
    if (t <= 0.0 || abs(direction.y) < 1e-6) {
        discard;
    }

    vec3 world = near_point + direction * t;

    float fine = line_coverage(world.xz, kFineSpacing);
    float coarse = line_coverage(world.xz, kCoarseSpacing);
    float axis_x = axis_coverage(world.z);  // the X axis runs along z = 0
    float axis_z = axis_coverage(world.x);

    // Fade the fine grid out before it turns into noise in the distance, and
    // fade everything as it approaches the far plane so there is no hard edge.
    float distance_to_camera = length(world - u_camera_position);
    float fine_fade = 1.0 - smoothstep(kCoarseSpacing * 4.0, kCoarseSpacing * 12.0,
                                       distance_to_camera);
    float horizon_fade = 1.0 - smoothstep(u_far_plane * 0.25, u_far_plane * 0.8,
                                          distance_to_camera);

    float alpha = max(coarse, fine * fine_fade);
    vec3 colour = mix(kFineColour, kCoarseColour, step(0.001, coarse));

    if (axis_x > 0.0) {
        colour = kAxisXColour;
        alpha = max(alpha, axis_x);
    }
    if (axis_z > 0.0) {
        colour = kAxisZColour;
        alpha = max(alpha, axis_z);
    }

    alpha *= horizon_fade;
    if (alpha <= 0.001) {
        discard;
    }

    o_colour = vec4(colour, alpha);

    // The plane sits in the world, so it has to occlude and be occluded like
    // anything else. Depth is written by hand because the geometry drawn is a
    // fullscreen triangle, not the plane itself.
    vec4 clip = u_view_projection * vec4(world, 1.0);
    gl_FragDepth = (clip.z / clip.w) * 0.5 + 0.5;
}
