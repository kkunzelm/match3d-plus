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
