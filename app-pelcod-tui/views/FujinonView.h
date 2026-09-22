#pragma once

/// @file FujinonView.h
/// @brief TUI interactive view for Fujinon SX800 / SX801 optical surveillance camera.

#include "Canvas.h"
#include "FujinonSX800Device.h"
#include "FujinonTypes.h"
#include "Terminal.h"

#include <cstdint>
#include <string>

namespace PelcoDTui {

/// @class FujinonView
/// @brief Interactive TUI panel for configuring Fujinon SX800 optical, day/night, and image features.
class FujinonView {
public:
    FujinonView() = default;
    ~FujinonView() = default;

    void render(Canvas& canvas, int startY, int width, int height, const PelcoD::FujinonStatus& status);
    bool handleInput(const InputEvent& event, PelcoD::FujinonSX800Device& device);

private:
    int m_selectedIndex { 0 };

    // Local state tracking
    int m_oisIndex { 0 }; // 0: Auto, 1: OIS, 2: EIS, 3: Off
    bool m_vlcFilter { false };
    int m_defogIndex { 0 }; // 0: Off, 1: L1, 2: L2, 3: L3
    int m_heatHazeIndex { 0 }; // 0: Off, 1: L1, 2: L2
    int m_wdrIndex { 0 }; // 0: Off, 1: L1, 2: L2, 3: L3
    bool m_antialiasing { false };

    int m_brightnessFine { 50 }; // 1..100
    int m_contrastFine { 50 }; // 1..100
    int m_saturationFine { 50 }; // 1..100
    int m_sharpnessFine { 50 }; // 1..100
    int m_wbShiftRedFine { 50 }; // 1..100
    int m_wbShiftBlueFine { 50 }; // 1..100

    int m_dayNightExIndex { 0 }; // 0: Auto, 1: Auto+Sched, 2: Sched, 3: Day, 4: Night
    int m_dayToNightThreshold { 60 }; // 0..255
    int m_nightToDayThreshold { 100 }; // 0..255
    int m_zoomSpeedEx { 1 }; // 1..8
    int m_focusSpeedEx { 3 }; // 1..5
    int m_digitalZoomMode { 0 }; // 0: Off, 1: Digital Zoom, 2: Crop Mode

    std::string m_lastAction { "Ready" };
};

} // namespace PelcoDTui
