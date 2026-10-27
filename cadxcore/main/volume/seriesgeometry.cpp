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
#include "seriesgeometry.h"

#include <algorithm>
#include <cmath>

namespace
{

const double OrientationTolerance = 1e-3;
const double SamePositionTolerance = 1e-3;  // mm
const double SpacingRelativeTolerance = 0.01;
const double SpacingAbsoluteTolerance = 0.01;  // mm
const double TiltParallelLimit = 0.999;

double Dot(const double a[3], const double b[3])
{
        return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Cross(const double a[3], const double b[3], double out[3])
{
        out[0] = a[1] * b[2] - a[2] * b[1];
        out[1] = a[2] * b[0] - a[0] * b[2];
        out[2] = a[0] * b[1] - a[1] * b[0];
}

struct Placed {
        double distance;  // position along the slice normal
        size_t index;
};

bool ByDistance(const Placed& a, const Placed& b)
{
        return a.distance < b.distance;
}

}

std::string GNC::GCS::BuildSeriesGeometry(const std::vector<SliceInfo>& slices, SeriesGeometry& out)
{
        if (slices.size() < 3) {
                return "A volume needs at least 3 slices.";
        }

        const SliceInfo& first = slices[0];
        for (size_t i = 0; i < slices.size(); ++i) {
                const SliceInfo& s = slices[i];
                if (s.frames != 1) {
                        return "Multi-frame images aren't supported yet.";
                }
                if (s.samplesPerPixel != 1) {
                        return "Only greyscale images are supported.";
                }
                if (s.rows <= 0 || s.columns <= 0 || s.rows != first.rows || s.columns != first.columns) {
                        return "The slices differ in size.";
                }
                if (s.pixelSpacing[0] <= 0.0 || s.pixelSpacing[1] <= 0.0 ||
                    std::fabs(s.pixelSpacing[0] - first.pixelSpacing[0]) > 1e-6 ||
                    std::fabs(s.pixelSpacing[1] - first.pixelSpacing[1]) > 1e-6) {
                        return "The slices differ in pixel spacing, or it is missing.";
                }
                for (int k = 0; k < 6; ++k) {
                        if (std::fabs(s.orientation[k] - first.orientation[k]) > OrientationTolerance) {
                                return "The slices differ in orientation.";
                        }
                }
        }

        const double* r = first.orientation;
        const double* c = first.orientation + 3;
        if (std::fabs(Dot(r, r) - 1.0) > 1e-2 || std::fabs(Dot(c, c) - 1.0) > 1e-2 || std::fabs(Dot(r, c)) > 1e-2) {
                return "The image orientation is invalid.";
        }
        double n[3];
        Cross(r, c, n);

        std::vector<Placed> placed(slices.size());
        for (size_t i = 0; i < slices.size(); ++i) {
                placed[i].distance = Dot(slices[i].position, n);
                placed[i].index = i;
        }
        std::stable_sort(placed.begin(), placed.end(), ByDistance);

        for (size_t i = 1; i < placed.size(); ++i) {
                if (placed[i].distance - placed[i - 1].distance < SamePositionTolerance) {
                        return "Two slices are at the same position (a duplicate, or several phases in one series).";
                }
        }

        // Gantry tilt: the stack runs along a line that isn't the slice normal
        const double* p0 = slices[placed.front().index].position;
        const double* pn = slices[placed.back().index].position;
        const double axis[3] = { pn[0] - p0[0], pn[1] - p0[1], pn[2] - p0[2] };
        const double axisLength = std::sqrt(Dot(axis, axis));
        if (std::fabs(Dot(axis, n)) / axisLength < TiltParallelLimit) {
                return "Gantry-tilted series aren't supported yet.";
        }

        const double meanGap = (placed.back().distance - placed.front().distance) / double(placed.size() - 1);
        const double allowed = std::max(SpacingRelativeTolerance * meanGap, SpacingAbsoluteTolerance);
        for (size_t i = 1; i < placed.size(); ++i) {
                const double gap = placed[i].distance - placed[i - 1].distance;
                if (std::fabs(gap - meanGap) > allowed) {
                        return "The slices aren't evenly spaced (a slice may be missing).";
                }
        }

        out.sortedPaths.clear();
        for (size_t i = 0; i < placed.size(); ++i) {
                out.sortedPaths.push_back(slices[placed[i].index].path);
        }
        // DICOM lists the spacing between rows first, then between columns
        out.spacing[0] = first.pixelSpacing[1];
        out.spacing[1] = first.pixelSpacing[0];
        out.spacing[2] = meanGap;
        for (int k = 0; k < 3; ++k) {
                out.origin[k] = p0[k];
                out.direction[3 * k + 0] = r[k];
                out.direction[3 * k + 1] = c[k];
                out.direction[3 * k + 2] = n[k];
        }
        return std::string();
}
