#version 450 core

// Linear blend skinning: each vertex is placed by up to four joints, weighted.
//
// The joint matrices arrive already composed on the CPU as "where the joint is
// now" times "the inverse of where it was when the mesh was bound", so in the
// bind pose every one of them is the identity and the vertex does not move.

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_joints;
layout(location = 4) in vec4 a_weights;

out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_uv;

uniform mat4 u_view_projection;

// A uniform block rather than a uniform array on purpose. GL 4.5 guarantees
// only 1024 vertex uniform components, which is 64 matrices, and a humanoid
// skeleton gets close to that; a uniform block is guaranteed 16 KB, which is
// 256 matrices.
const int kMaxJoints = 256;

layout(std140, binding = 0) uniform Joints {
    mat4 u_joints[kMaxJoints];
};

void main() {
    ivec4 joints = ivec4(a_joints);

    // A vertex with no weights at all would collapse to the origin, so it is
    // left where it was modelled instead.
    float total = a_weights.x + a_weights.y + a_weights.z + a_weights.w;

    mat4 skin = mat4(1.0);
    if (total > 0.0) {
        skin = a_weights.x * u_joints[joints.x] +
               a_weights.y * u_joints[joints.y] +
               a_weights.z * u_joints[joints.z] +
               a_weights.w * u_joints[joints.w];
        skin /= total;
    }

    vec4 world = skin * vec4(a_position, 1.0);

    v_world_position = world.xyz;

    // The upper 3x3 of the blend, which is right for the rotations a skeleton
    // applies. A non-uniform scale on a joint would need the inverse transpose,
    // and that is not what rigs do.
    v_normal = mat3(skin) * a_normal;
    v_uv = a_uv;

    gl_Position = u_view_projection * world;
}
