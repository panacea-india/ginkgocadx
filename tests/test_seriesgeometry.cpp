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

// How a series' slices are sorted and checked before being stacked into a
// volume. The slices are described in code; no DICOM files are needed.

#include <gtest/gtest.h>

#include <main/volume/seriesgeometry.h>

#include <cmath>
#include <string>
#include <vector>

using GNC::GCS::BuildSeriesGeometry;
using GNC::GCS::SeriesGeometry;
using GNC::GCS::SliceInfo;

namespace
{

const double Pi = 3.14159265358979323846;

void Cross(const double a[3], const double b[3], double out[3])
{
        out[0] = a[1] * b[2] - a[2] * b[1];
        out[1] = a[2] * b[0] - a[0] * b[2];
        out[2] = a[0] * b[1] - a[1] * b[0];
}

// A regular stack: slice k sits at start + k * step * n, where n = r x c
std::vector<SliceInfo> Stack(const double r[3], const double c[3], double step, int count, const double start[3])
{
        double n[3];
        Cross(r, c, n);
        std::vector<SliceInfo> slices(count);
        for (int k = 0; k < count; ++k) {
                SliceInfo& s = slices[k];
                s.path = "s" + std::to_string(k);
                for (int i = 0; i < 3; ++i) {
                        s.position[i] = start[i] + k * step * n[i];
                        s.orientation[i] = r[i];
                        s.orientation[3 + i] = c[i];
                }
                s.pixelSpacing[0] = s.pixelSpacing[1] = 0.8;
                s.rows = s.columns = 128;
        }
        return slices;
}

const double Zero[3] = { 0, 0, 0 };
const double X[3] = { 1, 0, 0 };
const double Y[3] = { 0, 1, 0 };
const double NegZ[3] = { 0, 0, -1 };

std::vector<SliceInfo> Axial(int count = 8)
{
        return Stack(X, Y, 2.5, count, Zero);
}

void ExpectRefused(const std::vector<SliceInfo>& slices, const std::string& messagePart)
{
        SeriesGeometry g;
        const std::string message = BuildSeriesGeometry(slices, g);
        EXPECT_NE(message.find(messagePart), std::string::npos) << "message was: \"" << message << "\"";
}

}

TEST(SeriesGeometry, AxialSeriesGivenInShuffledOrderComesBackSorted)
{
        std::vector<SliceInfo> slices = Axial();
        const int order[8] = { 5, 2, 7, 0, 3, 6, 1, 4 };
        std::vector<SliceInfo> shuffled;
        for (int i = 0; i < 8; ++i) {
                shuffled.push_back(slices[order[i]]);
        }

        SeriesGeometry g;
        ASSERT_EQ(BuildSeriesGeometry(shuffled, g), "");
        ASSERT_EQ(g.sortedPaths.size(), 8u);
        for (int k = 0; k < 8; ++k) {
                EXPECT_EQ(g.sortedPaths[k], "s" + std::to_string(k));
        }
        EXPECT_NEAR(g.spacing[0], 0.8, 1e-9);
        EXPECT_NEAR(g.spacing[1], 0.8, 1e-9);
        EXPECT_NEAR(g.spacing[2], 2.5, 1e-9);
        EXPECT_NEAR(g.origin[2], 0.0, 1e-9);
        const double identity[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
        for (int i = 0; i < 9; ++i) {
                EXPECT_NEAR(g.direction[i], identity[i], 1e-9) << "element " << i;
        }
}

TEST(SeriesGeometry, StackingDirectionDoesNotMatter)
{
        // The same slices numbered feet-first (z rising) and head-first (z falling):
        // either way the result is ordered along +n, starting from the first slice
        const double start[3] = { 0, 0, 100 };
        std::vector<SliceInfo> feetFirst = Stack(X, Y, 2.5, 8, start);
        std::vector<SliceInfo> headFirst(feetFirst.rbegin(), feetFirst.rend());

        SeriesGeometry a, b;
        ASSERT_EQ(BuildSeriesGeometry(feetFirst, a), "");
        ASSERT_EQ(BuildSeriesGeometry(headFirst, b), "");
        EXPECT_EQ(a.sortedPaths, b.sortedPaths);
        EXPECT_EQ(a.sortedPaths.front(), "s0");
        EXPECT_NEAR(a.origin[2], 100.0, 1e-9);
        EXPECT_NEAR(b.origin[2], 100.0, 1e-9);
        EXPECT_NEAR(b.spacing[2], 2.5, 1e-9);
}

TEST(SeriesGeometry, FlippedImageHasNormalTowardsFeet)
{
        const double flippedColumns[3] = { 0, -1, 0 };
        SeriesGeometry g;
        ASSERT_EQ(BuildSeriesGeometry(Stack(X, flippedColumns, 2.5, 8, Zero), g), "");
        EXPECT_NEAR(g.direction[8], -1.0, 1e-9);
}

TEST(SeriesGeometry, CoronalSeries)
{
        SeriesGeometry g;
        ASSERT_EQ(BuildSeriesGeometry(Stack(X, NegZ, 3.0, 10, Zero), g), "");
        // r = +x, c = -z, so n = r x c = +y
        EXPECT_NEAR(g.direction[2], 0.0, 1e-9);
        EXPECT_NEAR(g.direction[5], 1.0, 1e-9);
        EXPECT_NEAR(g.direction[8], 0.0, 1e-9);
        EXPECT_NEAR(g.spacing[2], 3.0, 1e-9);
        EXPECT_EQ(g.sortedPaths.front(), "s0");
        EXPECT_EQ(g.sortedPaths.back(), "s9");
}

TEST(SeriesGeometry, ObliqueSeries)
{
        // Rotated 20 degrees about x
        const double a = 20.0 * Pi / 180.0;
        const double c[3] = { 0, std::cos(a), std::sin(a) };
        SeriesGeometry g;
        ASSERT_EQ(BuildSeriesGeometry(Stack(X, c, 2.0, 12, Zero), g), "");
        EXPECT_NEAR(g.direction[2], 0.0, 1e-9);
        EXPECT_NEAR(g.direction[5], -std::sin(a), 1e-9);
        EXPECT_NEAR(g.direction[8], std::cos(a), 1e-9);
        EXPECT_NEAR(g.spacing[2], 2.0, 1e-9);
}

TEST(SeriesGeometry, NonSquarePixelsKeepTheirAxes)
{
        std::vector<SliceInfo> slices = Axial();
        for (size_t i = 0; i < slices.size(); ++i) {
                slices[i].pixelSpacing[0] = 0.5;  // between rows
                slices[i].pixelSpacing[1] = 0.8;  // between columns
        }
        SeriesGeometry g;
        ASSERT_EQ(BuildSeriesGeometry(slices, g), "");
        EXPECT_NEAR(g.spacing[0], 0.8, 1e-9);  // x runs along a row, so it is the column spacing
        EXPECT_NEAR(g.spacing[1], 0.5, 1e-9);
}

TEST(SeriesGeometry, MissingSliceIsRefused)
{
        std::vector<SliceInfo> slices = Axial(7);
        slices.erase(slices.begin() + 3);
        ExpectRefused(slices, "evenly spaced");
}

TEST(SeriesGeometry, DuplicatePositionIsRefused)
{
        std::vector<SliceInfo> slices = Axial();
        slices[4].position[2] = slices[3].position[2];
        ExpectRefused(slices, "same position");
}

TEST(SeriesGeometry, MixedOrientationIsRefused)
{
        std::vector<SliceInfo> slices = Axial();
        slices[5].orientation[1] = 0.05;
        ExpectRefused(slices, "orientation");
}

TEST(SeriesGeometry, GantryTiltIsRefused)
{
        // Normal along z, but the stack advances along a line tilted 30 degrees from it
        std::vector<SliceInfo> slices = Axial();
        for (size_t k = 0; k < slices.size(); ++k) {
                slices[k].position[1] = k * 2.5 * 0.5;
                slices[k].position[2] = k * 2.5 * std::sqrt(0.75);
        }
        ExpectRefused(slices, "tilt");
}

TEST(SeriesGeometry, MultiFrameSliceIsRefused)
{
        std::vector<SliceInfo> slices = Axial();
        slices[2].frames = 20;
        ExpectRefused(slices, "Multi-frame");
}

TEST(SeriesGeometry, ColourSliceIsRefused)
{
        std::vector<SliceInfo> slices = Axial();
        slices[0].samplesPerPixel = 3;
        ExpectRefused(slices, "greyscale");
}

TEST(SeriesGeometry, TooFewSlicesAreRefused)
{
        ExpectRefused(Axial(2), "at least 3");
}

TEST(SeriesGeometry, SlicesOfDifferentSizeAreRefused)
{
        std::vector<SliceInfo> slices = Axial();
        slices[6].columns = 256;
        ExpectRefused(slices, "size");
}

TEST(SeriesGeometry, MissingPixelSpacingIsRefused)
{
        std::vector<SliceInfo> slices = Axial();
        for (size_t i = 0; i < slices.size(); ++i) {
                slices[i].pixelSpacing[0] = slices[i].pixelSpacing[1] = 0.0;
        }
        ExpectRefused(slices, "pixel spacing");
}
