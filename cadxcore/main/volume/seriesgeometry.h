/*
 * This file is part of Ginkgo CADx
 *
 * Ginkgo CADx is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser Public License
 * along with Ginkgo CADx; if not, see <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

// Works out how the slices of a series stack into a volume: their order,
// spacing and placement in DICOM patient space (LPS: +x is the patient's left,
// +y posterior, +z towards the head). Plain code, no VTK or wx.

#include <api/api.h>

#include <string>
#include <vector>

namespace GNC
{
namespace GCS
{

/** What the geometry needs from one slice's DICOM tags. */
struct SliceInfo {
        std::string path;
        double position[3];      // Image Position (Patient), 0020,0032
        double orientation[6];   // Image Orientation (Patient), 0020,0037: row direction, then column direction
        double pixelSpacing[2];  // Pixel Spacing, 0028,0030: spacing between rows, then between columns
        int rows;                // 0028,0010
        int columns;             // 0028,0011
        int frames;              // Number of Frames, 0028,0008 (1 if absent)
        int samplesPerPixel;     // 0028,0002

        SliceInfo() : rows(0), columns(0), frames(1), samplesPerPixel(1)
        {
                for (int i = 0; i < 3; ++i) {
                        position[i] = 0.0;
                }
                for (int i = 0; i < 6; ++i) {
                        orientation[i] = 0.0;
                }
                pixelSpacing[0] = pixelSpacing[1] = 0.0;
        }
};

struct SeriesGeometry {
        std::vector<std::string> sortedPaths;  // slices in stacking order
        double spacing[3];                     // along x (between columns), y (between rows), z (between slices), mm
        double origin[3];                      // patient position of the first sorted slice's first pixel
        double direction[9];                   // row-major 3x3; its columns are the row, column and normal directions
};

/**
 * Sorts the slices by position along the slice normal and checks that they
 * form a regular volume: at least 3 single-frame, single-sample slices of the
 * same size, pixel spacing and orientation, evenly spaced, with no gantry tilt.
 * Returns an empty string on success, otherwise a message for the user.
 */
EXTAPI std::string BuildSeriesGeometry(const std::vector<SliceInfo>& slices, SeriesGeometry& out);

}
}
