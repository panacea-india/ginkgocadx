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

// A synthetic CT of known content, written as DICOM series in several
// orientations. Used by the volume loading tests and, through make_phantom,
// to check the 3D views by eye.
//
// Everything is defined in DICOM patient space (LPS: +x is the patient's
// left, +y posterior, +z towards the head; millimetres), around the origin:
//  - background -1000 HU, a 60 mm sphere of 40 HU at the centre
//  - a 1000 HU rod on the patient's left (x = 40), running along z
//  - a 1000 HU cube at the anterior edge (y = -45)
//  - a 1000 HU ball at the head end (z = 50)
// so a flipped or swapped axis in a view is obvious.

#include <string>

namespace phantom
{

enum Variant {
        Axial,          // 128 x 128 x 64, 0.8 x 0.8 x 2 mm, instance numbers in stacking order
        AxialReversed,  // the same slices, instance numbers reversed
        Coronal,        // 128 x 160 pixels, 52 slices along y
        Oblique20,      // axial rotated 20 degrees about x
        Gap,            // axial with one slice missing; refused by the volume code
        VariantCount
};

const char* Name(Variant v);

/** The phantom's value in Hounsfield units at a patient position. */
double HU(const double p[3]);

/** Writes the series as IM0001.dcm, IM0002.dcm, ... (instance order) into \a dir, which must exist. */
bool Write(Variant v, const std::string& dir);

}
