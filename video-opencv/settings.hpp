#pragma once
#include "options/value.hpp"
using namespace options;

// Numeric values are stored in user profiles ("exposure-preset"): do not reorder.
// Enumerators avoid the names `near`/`far`, which <windows.h> defines as macros.
enum class exposure_preset : int {
    range_near, range_far, ignored, range_medium,
    automatic, // the camera's own auto exposure
    DEFAULT = automatic,
};

struct dshow_camera_settings final {
    bundle b = make_bundle("video-camera");
    value<exposure_preset> exposure{b, "exposure-preset", exposure_preset::DEFAULT};
};
