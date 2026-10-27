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

#include <api/api.h>
#include <main/volume/seriesgeometry.h>

#include <vtkSmartPointer.h>

#include <list>
#include <string>

class vtkImageData;

namespace GNC
{
namespace GCS
{

class IComando;

/**
 * Reads a series into one volume.
 *
 * The slices are sorted and checked with BuildSeriesGeometry, so \a paths may
 * be in any order. The volume's origin is (0,0,0) and its spacing is set; where
 * it sits in patient space is in \a geometry (origin and direction).
 *
 * Throws ControladorCargaException with a message for the user if the slices
 * can't be stacked or can't be read. Runs on the calling thread, so it can be
 * called from a command; it must not be called from the UI thread for a large
 * series.
 */
EXTAPI void LoadSeriesVolume(IComando* cmd, const std::list<std::string>& paths, vtkSmartPointer<vtkImageData>& volume, SeriesGeometry& geometry);

}
}
