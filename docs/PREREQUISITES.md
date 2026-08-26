# Prerequisites

kinetiqra targets C++20 and OpenGL 4.5 core. Nothing in the project is tied to a
particular operating system.

## What the project needs

You need CMake 3.25 or newer, Ninja, and a compiler with C++20 support. GCC 13
and Clang 18 are both known to work. To run the editor you also need a GPU
driver providing OpenGL 4.5 core with direct state access.

Two more sets of packages are needed by vcpkg rather than by kinetiqra itself,
which makes them easy to overlook. The autotools family (`autoconf`,
`autoconf-archive`, `automake` and `libtool`) is used by some ports, and without
it the dependency install fails partway through with an error that does not
obviously point at the cause. `curl`, `zip`, `unzip` and `tar` are used by
vcpkg's bootstrap.

`clang-format` is not required to build, but is expected before pushing; see
[CODESTYLE.md](CODESTYLE.md).

On Linux you will also need the X11 and OpenGL development headers, which GLFW
links against.

## Installing them

The commands below are for **Ubuntu 24.04**, which is what the maintainer
develops on. They are an example, not a requirement. It is simply the one
platform where the steps have been written down and confirmed to work.

```sh
sudo apt install cmake ninja-build pkg-config build-essential \
  curl zip unzip tar \
  autoconf autoconf-archive automake libtool \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxi-dev libxext-dev
```

## vcpkg

The library dependencies come from `vcpkg.json` in manifest mode. Install vcpkg
once:

```sh
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg      # add this to your shell profile
```

The presets read `VCPKG_ROOT` and will not configure without it.

Some vcpkg ports build with autotools, which mishandles spaces in paths, so the
presets keep its install tree in `~/.cache/vcpkg-installed/kinetiqra` rather
than inside `build/`. That covers the dependencies, but a checkout under a path
containing a space may still trip other tooling, so a space-free path is safer.

## Other operating systems

The project should build anywhere its dependencies do: vcpkg, CMake and the
presets work the same on Windows, macOS and other Linux distributions, and only
the system package step differs. What is missing is documentation, not support,
since nobody has written those steps down yet. If you get stuck, are unsure
which packages to install, or already have it building elsewhere, open an issue.
That is how this page gets filled in.
