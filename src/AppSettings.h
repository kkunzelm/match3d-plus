/*
 * Match3D+ - Dental surface comparison software
 * Copyright (C) 2026 Karl-Heinz Kunzelmann
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <QSettings>

// Global parameters matching Gloger's match3d "Global Parameters" dialog.
// Single instance held by MainWindow, passed by pointer where needed.
struct AppSettings {
    float clipMin      = 0.0f;
    float clipMax      = 14000.0f;
    float clipGrad     = 4.0f;
    float clipDz       = 200.0f;
    float viewExp      = 1.5f;
    int   matchQuant   = 5;
    float scaleX       = 1.0f;
    float scaleY       = 1.0f;
    float scaleZ       = 1.0f;
    int   fpFitpixel   = 10;
    float fpHoehe      = 2000.0f;

    void load(QSettings& s);
    void save(QSettings& s) const;
};
