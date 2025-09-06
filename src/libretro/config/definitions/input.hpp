/*
    Copyright 2023 Jesse Talavera-Greenberg

    melonDS DS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS DS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS DS. If not, see http://www.gnu.org/licenses/.
*/

#ifndef MELONDS_DS_INPUT_HPP
#define MELONDS_DS_INPUT_HPP

#include <libretro.h>

#include "../constants.hpp"

namespace MelonDsDs::config::definitions {
    constexpr retro_core_option_v2_definition TouchMode {
        retro_core_option_v2_definition {
            config::input::TOUCH_MODE,
            "Touch Mode",
            nullptr,
            "Determines how the console's touch screen is emulated.\n"
            "\n"
            "Joystick: Use a joystick to control the cursor. "
            "Recommended if you don't have a mouse or a real touch screen available.\n"
            "Pointer: Use your mouse or touch screen to control the cursor.\n"
            "Auto: Use either Joystick or Pointer, depending on which you last touched.\n"
            "\n"
            "If unsure, set to Auto.",
            nullptr,
            config::input::CATEGORY,
            {
                {MelonDsDs::config::values::JOYSTICK, "Joystick"},
                {MelonDsDs::config::values::TOUCH, "Pointer"},
                {MelonDsDs::config::values::AUTO, "Auto"},
                {nullptr, nullptr},
            },
            MelonDsDs::config::values::AUTO
        },
    };

    constexpr retro_core_option_v2_definition JoystickCursorDeadzone {
        config::input::JOYSTICK_CURSOR_DEADZONE,
        "Joystick Cursor Deadzone",
        nullptr,
        "If the joystick is within this deadzone the cursor will not move.",
        nullptr,
        config::input::CATEGORY,
        {
            {"0", "0%"},
            {"5", "5%"},
            {"10", "10%"},
            {"15", "15%"},
            {"20", "20%"},
            {"25", "25%"},
            {"30", "30%"},
            {"35", "35%"},
            {nullptr, nullptr},
        },
        "5"
    };

    constexpr retro_core_option_v2_definition JoystickCursorMaxSpeed {
        config::input::JOYSTICK_CURSOR_MAXSPEED,
        "Joystick Cursor Max Speed",
        nullptr,
        "Set the max speed for the joystick cursor.",
        nullptr,
        config::input::CATEGORY,
        {
            {"1", "1"},
            {"2", "2"},
            {"3", "3"},
            {"4", "4"},
            {"5", "5"},
            {"6", "6"},
            {"7", "7"},
            {"8", "8"},
            {"9", "9"},
            {nullptr, nullptr},
        },
        "3"
    };
    constexpr retro_core_option_v2_definition JoystickCursorResponse {
        config::input::JOYSTICK_CURSOR_RESPONSE,
        "Joystick Cursor Response",
        nullptr,
        "Set the response curve for the joystick cursor.\n"
        "Linear is a response curve where the cursor speed is 1:1 with the joystick input.\n"
        "Quadratic is a response curve that reduces the sensitivity near the joystick center for finer control, but increases the sensitivity near the edges",
        nullptr,
        config::input::CATEGORY,
        {
            {"100", "Linear"},
            {"200", "Quadratic"},
            {nullptr, nullptr},
        },
        "200"
    };

    constexpr retro_core_option_v2_definition JoystickCursorSpeedup {
        config::input::JOYSTICK_CURSOR_SPEEDUP,
        "Joystick Cursor Multiplier",
        nullptr,
        "Set the multiplier for the joystick cursor when the speedup/slowdown pointer button (L2 by default) is held",
        nullptr,
        config::input::CATEGORY,
        {
            {"33", "33%"},
            {"50", "50%"},
            {"66", "66%"},
            {"150", "150%"},
            {"200", "200%"},
            {"250", "250%"},
            {"300", "300%"},
            {nullptr, nullptr},
        },
        "200"
    };    

    constexpr std::initializer_list<retro_core_option_v2_definition> InputOptionDefinitions {
        TouchMode,
        JoystickCursorDeadzone,
        JoystickCursorMaxSpeed,
        JoystickCursorResponse,
        JoystickCursorSpeedup,
    };    
}
#endif //MELONDS_DS_INPUT_HPP
