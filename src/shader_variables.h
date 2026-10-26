#pragma once

typedef struct ShaderVariables {
    // float array with 3 elements is the same as vec3 in ImGui.
    float sun_direction[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float sun_intensity = 1.0;
    float sun_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
} ShaderVariables;
