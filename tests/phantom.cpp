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
#include "phantom.h"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dctk.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace
{

const double Pi = 3.14159265358979323846;
const double PixelSize = 0.8;       // mm
const double Intercept = -1024.0;   // stored value = HU - Intercept

std::string NewUid()
{
        char uid[100];
        dcmGenerateUniqueIdentifier(uid, SITE_INSTANCE_UID_ROOT);
        return uid;
}

// One study and frame of reference for everything written by this process
const std::string& StudyUid()
{
        static const std::string uid = NewUid();
        return uid;
}

const std::string& FrameOfReferenceUid()
{
        static const std::string uid = NewUid();
        return uid;
}

struct Grid {
        int rows, columns, slices;
        double r[3], c[3], n[3];  // row direction, column direction, slice normal
        double first[3];          // position of the first pixel of slice 0
        double step;              // mm between slices, along n
};

Grid MakeGrid(phantom::Variant v)
{
        Grid g;
        g.step = 2.0;
        switch (v) {
        case phantom::Coronal:
                g.rows = 160;
                g.columns = 128;
                g.slices = 52;
                g.r[0] = 1; g.r[1] = 0; g.r[2] = 0;
                g.c[0] = 0; g.c[1] = 0; g.c[2] = -1;
                g.n[0] = 0; g.n[1] = 1; g.n[2] = 0;
                g.first[0] = -63.5 * PixelSize;
                g.first[1] = -51.0;
                g.first[2] = 63.6;
                break;
        case phantom::Oblique20: {
                const double a = 20.0 * Pi / 180.0;
                g.rows = g.columns = 128;
                g.slices = 64;
                g.r[0] = 1; g.r[1] = 0; g.r[2] = 0;
                g.c[0] = 0; g.c[1] = std::cos(a); g.c[2] = std::sin(a);
                g.n[0] = 0; g.n[1] = -std::sin(a); g.n[2] = std::cos(a);
                for (int i = 0; i < 3; ++i) {
                        g.first[i] = -31.5 * g.step * g.n[i] - 63.5 * PixelSize * (g.r[i] + g.c[i]);
                }
                break;
        }
        default:  // the axial variants
                g.rows = g.columns = 128;
                g.slices = 64;
                g.r[0] = 1; g.r[1] = 0; g.r[2] = 0;
                g.c[0] = 0; g.c[1] = 1; g.c[2] = 0;
                g.n[0] = 0; g.n[1] = 0; g.n[2] = 1;
                g.first[0] = g.first[1] = -63.5 * PixelSize;
                g.first[2] = -31.5 * g.step;
                break;
        }
        return g;
}

bool Put(DcmDataset* ds, const DcmTagKey& tag, const std::string& value)
{
        return ds->putAndInsertString(tag, value.c_str()).good();
}

std::string Numbers(const double* values, int count)
{
        std::string text;
        char buffer[40];
        for (int i = 0; i < count; ++i) {
                std::snprintf(buffer, sizeof buffer, "%s%.6f", i ? "\\" : "", values[i]);
                text += buffer;
        }
        return text;
}

}

const char* phantom::Name(Variant v)
{
        switch (v) {
        case Axial:
                return "axial";
        case AxialReversed:
                return "axial-reversed";
        case Coronal:
                return "coronal";
        case Oblique20:
                return "oblique20";
        case Gap:
                return "gap";
        default:
                return "";
        }
}

double phantom::HU(const double p[3])
{
        const double x = p[0], y = p[1], z = p[2];
        if (x * x + y * y + (z - 50.0) * (z - 50.0) <= 25.0) {
                return 1000.0;  // ball at the head end
        }
        if (std::fabs(x) <= 4.0 && std::fabs(y + 45.0) <= 4.0 && std::fabs(z) <= 4.0) {
                return 1000.0;  // cube at the anterior edge
        }
        if (std::fabs(x - 40.0) <= 3.0 && std::fabs(y) <= 3.0 && z >= -50.0 && z <= 20.0) {
                return 1000.0;  // rod on the patient's left
        }
        if (x * x + y * y + z * z <= 900.0) {
                return 40.0;    // sphere
        }
        return -1000.0;
}

bool phantom::Write(Variant v, const std::string& dir)
{
        const Grid g = MakeGrid(v);
        const std::string seriesUid = NewUid();
        const bool reversed = (v == AxialReversed);

        for (int index = 0; index < g.slices; ++index) {
                if (v == Gap && index == 30) {
                        continue;
                }
                // File order is instance order; the stack position runs backwards in the reversed series
                const int instanceNumber = index + 1;
                const int k = reversed ? g.slices - 1 - index : index;

                double position[3];
                for (int i = 0; i < 3; ++i) {
                        position[i] = g.first[i] + k * g.step * g.n[i];
                }

                std::vector<Uint16> pixels(static_cast<size_t>(g.rows) * g.columns);
                for (int row = 0; row < g.rows; ++row) {
                        for (int column = 0; column < g.columns; ++column) {
                                double p[3];
                                for (int i = 0; i < 3; ++i) {
                                        p[i] = position[i] + column * PixelSize * g.r[i] + row * PixelSize * g.c[i];
                                }
                                const long stored = std::lround(HU(p) - Intercept);
                                pixels[static_cast<size_t>(row) * g.columns + column] = static_cast<Uint16>(stored);
                        }
                }

                DcmFileFormat file;
                DcmDataset* ds = file.getDataset();
                const double spacing[2] = { PixelSize, PixelSize };
                double orientation[6];
                for (int i = 0; i < 3; ++i) {
                        orientation[i] = g.r[i];
                        orientation[3 + i] = g.c[i];
                }
                char number[16];

                bool ok = true;
                ok &= Put(ds, DCM_SOPClassUID, UID_CTImageStorage);
                ok &= Put(ds, DCM_SOPInstanceUID, NewUid());
                ok &= Put(ds, DCM_ImageType, "ORIGINAL\\PRIMARY\\AXIAL");
                ok &= Put(ds, DCM_Modality, "CT");
                ok &= Put(ds, DCM_PatientName, "Phantom^MPR");
                ok &= Put(ds, DCM_PatientID, "PHANTOM");
                ok &= Put(ds, DCM_PatientBirthDate, "");
                ok &= Put(ds, DCM_PatientSex, "O");
                ok &= Put(ds, DCM_StudyInstanceUID, StudyUid());
                ok &= Put(ds, DCM_StudyDate, "20260101");
                ok &= Put(ds, DCM_StudyTime, "120000");
                ok &= Put(ds, DCM_StudyID, "1");
                ok &= Put(ds, DCM_AccessionNumber, "");
                ok &= Put(ds, DCM_ReferringPhysicianName, "");
                ok &= Put(ds, DCM_StudyDescription, "MPR phantom");
                ok &= Put(ds, DCM_SeriesInstanceUID, seriesUid);
                ok &= Put(ds, DCM_SeriesDescription, Name(v));
                std::snprintf(number, sizeof number, "%d", static_cast<int>(v) + 1);
                ok &= Put(ds, DCM_SeriesNumber, number);
                ok &= Put(ds, DCM_FrameOfReferenceUID, FrameOfReferenceUid());
                ok &= Put(ds, DCM_PositionReferenceIndicator, "");
                ok &= Put(ds, DCM_PatientPosition, "HFS");
                std::snprintf(number, sizeof number, "%d", instanceNumber);
                ok &= Put(ds, DCM_InstanceNumber, number);
                ok &= Put(ds, DCM_ImagePositionPatient, Numbers(position, 3));
                ok &= Put(ds, DCM_ImageOrientationPatient, Numbers(orientation, 6));
                ok &= Put(ds, DCM_SliceThickness, "2");
                ok &= Put(ds, DCM_PixelSpacing, Numbers(spacing, 2));
                ok &= Put(ds, DCM_PhotometricInterpretation, "MONOCHROME2");
                ok &= ds->putAndInsertUint16(DCM_SamplesPerPixel, 1).good();
                ok &= ds->putAndInsertUint16(DCM_Rows, static_cast<Uint16>(g.rows)).good();
                ok &= ds->putAndInsertUint16(DCM_Columns, static_cast<Uint16>(g.columns)).good();
                ok &= ds->putAndInsertUint16(DCM_BitsAllocated, 16).good();
                ok &= ds->putAndInsertUint16(DCM_BitsStored, 12).good();
                ok &= ds->putAndInsertUint16(DCM_HighBit, 11).good();
                ok &= ds->putAndInsertUint16(DCM_PixelRepresentation, 1).good();
                ok &= Put(ds, DCM_RescaleIntercept, "-1024");
                ok &= Put(ds, DCM_RescaleSlope, "1");
                ok &= Put(ds, DCM_RescaleType, "HU");
                ok &= Put(ds, DCM_WindowCenter, "40");
                ok &= Put(ds, DCM_WindowWidth, "400");
                ok &= ds->putAndInsertUint16Array(DCM_PixelData, pixels.data(), pixels.size()).good();
                if (!ok) {
                        return false;
                }

                char name[32];
                std::snprintf(name, sizeof name, "IM%04d.dcm", instanceNumber);
                if (file.saveFile((dir + "/" + name).c_str(), EXS_LittleEndianExplicit).bad()) {
                        return false;
                }
        }
        return true;
}
