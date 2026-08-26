# Project layout

The project is divided into eight modules: `core`, `math`, `geom`, `scene`,
`anim`, `io`, `render` and `app`. Those eight names are used throughout, as the
directory under `src/`, as the `area:` label on issues and pull requests, and as
the optional scope in a pull request title.

```
src/core/     Handles, arenas, command stack, undo, logging
src/math/     Transforms, AABB, ray, TRS decomposition
src/geom/     Editable mesh, attribute system, mesh algorithms
src/scene/    Node hierarchy, skeleton, skin, materials, selection
src/anim/     Clips, channels, samplers, interpolation, pose evaluation
src/io/       glTF import/export and the native .kqr format
src/render/   OpenGL renderer, render meshes, passes, shaders
src/app/      Editor shell, panels, tools, gizmos
tests/        Mirrors the module layout
```

Supporting directories: `cmake/` for the build helpers, `assets/` for shaders
and fonts loaded at runtime, `external/` for dependencies vcpkg does not cover,
and `docs/` for this documentation.

## Module shape

Every module has the same structure:

```
src/<module>/
  include/kinetiqra/<module>/   public headers, visible to dependents
  src/                          private headers and translation units
```

Only `include/` is on the public include path, so a header that is not there is
physically unreachable from another module. Includes read as
`#include <kinetiqra/geom/EditMesh.hpp>`.

## Dependency direction

Dependencies flow strictly downward, in the order listed above, and are declared
in each module's `CMakeLists.txt`:

```cmake
kinetiqra_add_module(geom DEPENDS kinetiqra::core kinetiqra::math)
```

The linker enforces it: if `geom` reaches into `render`, it does not compile.
Modules with no translation units yet are INTERFACE targets and become STATIC
with their first `.cpp`.

This page says where code lives. [INVARIANTS.md](INVARIANTS.md) says what the
code itself must respect: coordinate conventions, how mesh elements are
referenced and where their attributes live, and how the scene may be changed.
