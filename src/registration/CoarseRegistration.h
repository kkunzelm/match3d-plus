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

#include "../io/ViffReader.h"
#include "../RoiMask.h"
#include "Transformation3D.h"

#include <CCGeom.h>
#include <PointCloud.h>

#include <vector>

class CoarseRegistration {
public:
    // Compute rigid transform from N>=3 corresponding image points.
    // dataPoints: pixel coords (col, row) in the data image.
    // refPoints:  pixel coords (col, row) in the reference image.
    // Converts to 3D world coords using each image's pixel size and depth.
    static bool fromPoints(
        const std::vector<std::pair<int,int>>& dataPts,
        const ViffImage& dataImg,
        const std::vector<std::pair<int,int>>& refPts,
        const ViffImage& refImg,
        Transformation3D& result,
        QString& errorMsg);

    // Compute translation from center-of-mass difference.
    static bool fromCOM(
        const ViffImage& dataImg, const RoiMask* dataRoi,
        const ViffImage& refImg,  const RoiMask* refRoi,
        Transformation3D& result,
        QString& errorMsg);

private:
    static CCVector3 pixelToWorld(int col, int row, const ViffImage& img);
};
