/*
    This source file is part of Rigs of Rods
    Copyright 2016-2020 Petr Ohlidal

    For more information, see http://www.rigsofrods.org/

    Rigs of Rods is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 3, as
    published by the Free Software Foundation.

    Rigs of Rods is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Rigs of Rods. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "Application.h"
#include "OgreImGui.h"

namespace RoR {
namespace GUI {

class GameSettings
{
public:
    void Draw();

    bool IsVisible() const { return m_is_visible; }
    void SetVisible(bool v);

private:
    void DrawRenderSystemSettings();
    void DrawGeneralSettings();
    void DrawGameplaySettings();
    void DrawGraphicsSettings();
    void DrawAudioSettings();
    void DrawControlSettings();
    void DrawDiagSettings();

    bool m_is_visible = false;

    Str<1000> m_buf_diag_preset_terrain;
    Str<1000> m_buf_diag_preset_vehicle;
    Str<1000> m_buf_diag_preset_veh_config;
    Str<1000> m_buf_app_extra_mod_dir;
    Str<1000> m_buf_io_outgauge_ip;
};

} // namespace GUI
} // namespace RoR
