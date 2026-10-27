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

// LoadSeriesVolume on the phantom series (tests/phantom.h): every voxel of the
// loaded volume, placed with the geometry that comes back, must be what the
// phantom has at that point in patient space. That covers slice order, axis
// directions, spacing and Hounsfield units in one go.

#include <gtest/gtest.h>

#include "phantom.h"

#include <api/controllers/icontroladorcarga.h>
#include <main/volume/seriesvolume.h>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <list>
#include <string>
#include <vector>

using GNC::GCS::SeriesGeometry;

namespace
{

std::vector<std::string> FilesIn(const std::string& dir)
{
        std::vector<std::string> names;
        DIR* d = opendir(dir.c_str());
        if (d != NULL) {
                while (dirent* entry = readdir(d)) {
                        const std::string name = entry->d_name;
                        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".dcm") == 0) {
                                names.push_back(dir + "/" + name);
                        }
                }
                closedir(d);
        }
        std::sort(names.begin(), names.end());
        return names;
}

class SeriesVolume : public ::testing::Test
{
protected:
        static void SetUpTestSuite()
        {
                char pattern[] = "/tmp/ginkgo-phantom-XXXXXX";
                ASSERT_TRUE(mkdtemp(pattern) != NULL);
                root = pattern;
                for (int i = 0; i < phantom::VariantCount; ++i) {
                        const phantom::Variant v = static_cast<phantom::Variant>(i);
                        const std::string dir = root + "/" + phantom::Name(v);
                        ASSERT_EQ(mkdir(dir.c_str(), 0755), 0);
                        ASSERT_TRUE(phantom::Write(v, dir));
                }
        }

        static void TearDownTestSuite()
        {
                for (int i = 0; i < phantom::VariantCount; ++i) {
                        const std::string dir = root + "/" + phantom::Name(static_cast<phantom::Variant>(i));
                        const std::vector<std::string> files = FilesIn(dir);
                        for (size_t f = 0; f < files.size(); ++f) {
                                unlink(files[f].c_str());
                        }
                        rmdir(dir.c_str());
                }
                rmdir(root.c_str());
        }

        static std::list<std::string> Paths(phantom::Variant v)
        {
                const std::vector<std::string> files = FilesIn(root + "/" + phantom::Name(v));
                return std::list<std::string>(files.begin(), files.end());
        }

        // Compares every voxel with the phantom at the voxel's patient position
        static void ExpectVolumeMatchesPhantom(phantom::Variant v, int columns, int rows, int slices)
        {
                vtkSmartPointer<vtkImageData> volume;
                SeriesGeometry g;
                ASSERT_NO_THROW(GNC::GCS::LoadSeriesVolume(NULL, Paths(v), volume, g));
                ASSERT_TRUE(volume != NULL);

                int dims[3];
                volume->GetDimensions(dims);
                ASSERT_EQ(dims[0], columns);
                ASSERT_EQ(dims[1], rows);
                ASSERT_EQ(dims[2], slices);
                EXPECT_NEAR(g.spacing[0], 0.8, 1e-6);
                EXPECT_NEAR(g.spacing[1], 0.8, 1e-6);
                EXPECT_NEAR(g.spacing[2], 2.0, 1e-6);
                double spacing[3];
                volume->GetSpacing(spacing);
                for (int i = 0; i < 3; ++i) {
                        EXPECT_NEAR(spacing[i], g.spacing[i], 1e-9);
                }
                double origin[3];
                volume->GetOrigin(origin);
                EXPECT_EQ(origin[0] + origin[1] + origin[2], 0.0);

                int wrong = 0;
                for (int k = 0; k < dims[2]; ++k) {
                        for (int j = 0; j < dims[1]; ++j) {
                                for (int i = 0; i < dims[0]; ++i) {
                                        double p[3];
                                        for (int a = 0; a < 3; ++a) {
                                                p[a] = g.origin[a]
                                                       + i * g.spacing[0] * g.direction[3 * a + 0]
                                                       + j * g.spacing[1] * g.direction[3 * a + 1]
                                                       + k * g.spacing[2] * g.direction[3 * a + 2];
                                        }
                                        if (std::fabs(volume->GetScalarComponentAsDouble(i, j, k, 0) - phantom::HU(p)) > 0.5) {
                                                ++wrong;
                                        }
                                }
                        }
                }
                EXPECT_EQ(wrong, 0) << "voxels that differ from the phantom";
        }

        static std::string root;
};

std::string SeriesVolume::root;

}

TEST_F(SeriesVolume, Axial)
{
        ExpectVolumeMatchesPhantom(phantom::Axial, 128, 128, 64);
}

TEST_F(SeriesVolume, FilesInReverseInstanceOrderGiveTheSameVolume)
{
        ExpectVolumeMatchesPhantom(phantom::AxialReversed, 128, 128, 64);
}

TEST_F(SeriesVolume, FilesGivenInAnyOrderGiveTheSameVolume)
{
        std::list<std::string> paths = Paths(phantom::Axial);
        std::vector<std::string> shuffled(paths.begin(), paths.end());
        std::random_shuffle(shuffled.begin(), shuffled.end());

        vtkSmartPointer<vtkImageData> volume;
        SeriesGeometry g;
        GNC::GCS::LoadSeriesVolume(NULL, std::list<std::string>(shuffled.begin(), shuffled.end()), volume, g);
        EXPECT_EQ(g.sortedPaths, std::vector<std::string>(paths.begin(), paths.end()));
}

TEST_F(SeriesVolume, Coronal)
{
        ExpectVolumeMatchesPhantom(phantom::Coronal, 128, 160, 52);
}

TEST_F(SeriesVolume, Oblique)
{
        ExpectVolumeMatchesPhantom(phantom::Oblique20, 128, 128, 64);
}

TEST_F(SeriesVolume, SeriesWithAMissingSliceIsRefusedWithAMessage)
{
        vtkSmartPointer<vtkImageData> volume;
        SeriesGeometry g;
        try {
                GNC::GCS::LoadSeriesVolume(NULL, Paths(phantom::Gap), volume, g);
                FAIL() << "expected an exception";
        } catch (const GNC::GCS::ControladorCargaException& e) {
                EXPECT_NE(e.GetCause().find("evenly spaced"), std::string::npos) << e.GetCause();
        }
}
