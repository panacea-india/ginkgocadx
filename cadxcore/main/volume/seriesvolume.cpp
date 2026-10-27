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
#include "seriesvolume.h"

#include <api/controllers/icontroladorcarga.h>
#include <api/dicom/idicommanager.h>
#include <api/dicom/dcmdictionary.h>
#include <api/internationalization/internationalization.h>
#include <main/controllers/controladorcarga.h>
#include <main/controllers/pacscontroller.h>

#include <vtkImageData.h>

#include <locale>
#include <sstream>
#include <vector>

namespace
{

// Reads \a count backslash-separated numbers; false if there are fewer
bool ParseValues(std::string text, double* values, int count)
{
        for (size_t i = 0; i < text.size(); ++i) {
                if (text[i] == '\\') {
                        text[i] = ' ';
                }
        }
        std::istringstream in(text);
        in.imbue(std::locale::classic());
        for (int i = 0; i < count; ++i) {
                if (!(in >> values[i])) {
                        return false;
                }
        }
        return true;
}

bool ReadTag(GIL::DICOM::IDICOMManager* manager, unsigned int group, unsigned int element, double* values, int count)
{
        std::string text;
        return manager->GetTag(group, element, text) && ParseValues(text, values, count);
}

// The tags BuildSeriesGeometry needs, read without loading the pixel data
GNC::GCS::SliceInfo ReadSliceInfo(GIL::DICOM::IDICOMManager* manager, const std::string& path)
{
        GNC::GCS::SliceInfo info;
        info.path = path;
        const std::string what = " (" + path + ")";

        if (!manager->CargarFichero(path, true)) {
                throw GNC::GCS::ControladorCargaException(_Std("Error reading the study: ") + "can't read the file" + what, "SeriesVolume/ReadSliceInfo");
        }

        double value[1];
        if (!ReadTag(manager, 0x0020, 0x0032, info.position, 3)) {
                throw GNC::GCS::ControladorCargaException(_Std("Error reading the study: ") + "a slice has no Image Position" + what, "SeriesVolume/ReadSliceInfo");
        }
        if (!ReadTag(manager, 0x0020, 0x0037, info.orientation, 6)) {
                throw GNC::GCS::ControladorCargaException(_Std("Error reading the study: ") + "a slice has no Image Orientation" + what, "SeriesVolume/ReadSliceInfo");
        }
        // Pixel Spacing may be missing; BuildSeriesGeometry refuses a zero spacing
        ReadTag(manager, 0x0028, 0x0030, info.pixelSpacing, 2);

        if (ReadTag(manager, 0x0028, 0x0010, value, 1)) {
                info.rows = static_cast<int>(value[0]);
        }
        if (ReadTag(manager, 0x0028, 0x0011, value, 1)) {
                info.columns = static_cast<int>(value[0]);
        }
        if (ReadTag(manager, 0x0028, 0x0008, value, 1)) {
                info.frames = static_cast<int>(value[0]);
        }
        if (ReadTag(manager, 0x0028, 0x0002, value, 1)) {
                info.samplesPerPixel = static_cast<int>(value[0]);
        }
        return info;
}

}

void GNC::GCS::LoadSeriesVolume(IComando* cmd, const std::list<std::string>& paths, vtkSmartPointer<vtkImageData>& volume, SeriesGeometry& geometry)
{
        std::vector<SliceInfo> slices;
        {
                GIL::DICOM::IDICOMManager* manager = GIL::DICOM::PACSController::Instance()->CrearInstanciaDeDICOMManager();
                try {
                        for (std::list<std::string>::const_iterator it = paths.begin(); it != paths.end(); ++it) {
                                slices.push_back(ReadSliceInfo(manager, *it));
                        }
                } catch (...) {
                        GIL::DICOM::PACSController::Instance()->LiberarInstanciaDeDICOMManager(manager);
                        throw;
                }
                GIL::DICOM::PACSController::Instance()->LiberarInstanciaDeDICOMManager(manager);
        }

        const std::string problem = BuildSeriesGeometry(slices, geometry);
        if (!problem.empty()) {
                throw ControladorCargaException(_Std("This series can't be shown as a volume: ") + problem, "SeriesVolume/LoadSeriesVolume");
        }

        std::list<std::string> sorted(geometry.sortedPaths.begin(), geometry.sortedPaths.end());
        volume = ControladorCarga::Instance()->CargarITKMultidimensional(cmd, sorted, NULL, geometry.spacing);
        if (!volume) {
                throw ControladorCargaException(_Std("Error reading the study: ") + "no image data", "SeriesVolume/LoadSeriesVolume");
        }
        volume->SetOrigin(0.0, 0.0, 0.0);
}
