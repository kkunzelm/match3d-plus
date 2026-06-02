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

#include "AppSettings.h"

void AppSettings::load(QSettings& s) {
    s.beginGroup("GlobalParameters");
    clipMin    = s.value("clipMin",    clipMin).toFloat();
    clipMax    = s.value("clipMax",    clipMax).toFloat();
    clipGrad   = s.value("clipGrad",   clipGrad).toFloat();
    clipDz     = s.value("clipDz",     clipDz).toFloat();
    viewExp    = s.value("viewExp",    viewExp).toFloat();
    matchQuant = s.value("matchQuant", matchQuant).toInt();
    scaleX     = s.value("scaleX",     scaleX).toFloat();
    scaleY     = s.value("scaleY",     scaleY).toFloat();
    scaleZ     = s.value("scaleZ",     scaleZ).toFloat();
    fpFitpixel = s.value("fpFitpixel", fpFitpixel).toInt();
    fpHoehe    = s.value("fpHoehe",    fpHoehe).toFloat();
    s.endGroup();

    s.beginGroup("STLImport");
    stlResolution = s.value("resolution", stlResolution).toFloat();
    s.endGroup();
}

void AppSettings::save(QSettings& s) const {
    s.beginGroup("GlobalParameters");
    s.setValue("clipMin",    clipMin);
    s.setValue("clipMax",    clipMax);
    s.setValue("clipGrad",   clipGrad);
    s.setValue("clipDz",     clipDz);
    s.setValue("viewExp",    viewExp);
    s.setValue("matchQuant", matchQuant);
    s.setValue("scaleX",     scaleX);
    s.setValue("scaleY",     scaleY);
    s.setValue("scaleZ",     scaleZ);
    s.setValue("fpFitpixel", fpFitpixel);
    s.setValue("fpHoehe",    fpHoehe);
    s.endGroup();

    s.beginGroup("STLImport");
    s.setValue("resolution", stlResolution);
    s.endGroup();
}
