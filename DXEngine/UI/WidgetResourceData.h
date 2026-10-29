#pragma once

#include <dxwidgets/core/resourcedata.h>

// Whether a window system started: with none (no display, no SDL video) only
// a dedicated server, which opens no window, runs on.
bool InitWidgetResources(const std::string& theme);
void DeinitWidgetResources();
