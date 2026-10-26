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

class vtkRenderer;
class vtkOpenGLRenderWindow;

namespace GNC
{
namespace GCS
{

/**
 * Lets fixed-function OpenGL code (glBegin/glEnd, the matrix stack) draw
 * inside a VTK 9 render pass.
 *
 * VTK 6's legacy backend left the camera loaded in GL_PROJECTION and
 * GL_MODELVIEW and used no shaders; VTK 9 does neither and caches GL state.
 * The constructor recreates the old state for the renderer's active camera;
 * the destructor restores everything and resynchronises VTK's state cache.
 *
 * Requires a compatibility-profile context (see wxVTKRenderWindowInteractor).
 */
class EXTAPI LegacyGLScope
{
public:
        explicit LegacyGLScope(vtkRenderer* ren);
        ~LegacyGLScope();

private:
        LegacyGLScope(const LegacyGLScope&) = delete;
        LegacyGLScope& operator=(const LegacyGLScope&) = delete;

        vtkOpenGLRenderWindow* m_pRenWin;
};

}
}
