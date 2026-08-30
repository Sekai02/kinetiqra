#pragma once

// ImGuizmo names ImGui's types without including it, so imgui.h has to come
// first or the header does not compile.
//
// Include order here is deliberate and cannot be expressed where it is needed:
// the project sorts its includes alphabetically, and by that rule ImGuizmo.h
// lands before imgui.h. This file is the one place that order is stated, so
// nothing else has to remember it.
//
// clang-format off
#include <imgui.h>

#include <ImGuizmo.h>
// clang-format on
