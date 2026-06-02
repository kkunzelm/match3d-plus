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

#include <QDialog>
#include "../AppSettings.h"

class QLineEdit;

class GlobalParametersDialog : public QDialog {
    Q_OBJECT
public:
    explicit GlobalParametersDialog(AppSettings& settings, QWidget* parent = nullptr);

private slots:
    void onUpdate();
    void onRevert();

private:
    void populateFields();
    void applyToSettings();

    AppSettings& settings_;
    AppSettings  saved_;   // snapshot for Revert

    QLineEdit* leClipMin_       = nullptr;
    QLineEdit* leClipMax_       = nullptr;
    QLineEdit* leClipGrad_      = nullptr;
    QLineEdit* leClipDz_        = nullptr;
    QLineEdit* leViewExp_       = nullptr;
    QLineEdit* leMatchQuant_    = nullptr;
    QLineEdit* leScaleX_        = nullptr;
    QLineEdit* leScaleY_        = nullptr;
    QLineEdit* leScaleZ_        = nullptr;
    QLineEdit* leFpFitpixel_    = nullptr;
    QLineEdit* leFpHoehe_       = nullptr;
    QLineEdit* leStlResolution_ = nullptr;
};
