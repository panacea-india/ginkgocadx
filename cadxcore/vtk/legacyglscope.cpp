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

#include <vtk_glew.h>
#include "legacyglscope.h"

#include <vtkCamera.h>
#include <vtkMatrix4x4.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkOpenGLShaderCache.h>
#include <vtkOpenGLState.h>
#include <vtkRenderer.h>

GNC::GCS::LegacyGLScope::LegacyGLScope(vtkRenderer* ren) :
        m_pRenWin(vtkOpenGLRenderWindow::SafeDownCast(ren->GetRenderWindow()))
{
        if (m_pRenWin != NULL) {
                // VTK keeps its last shader program bound, which would override
                // fixed-function drawing; forget it so VTK rebinds it next time.
                m_pRenWin->GetShaderCache()->ReleaseCurrentShader();
                m_pRenWin->GetState()->Push();
        }
        glUseProgram(0);
        glBindVertexArray(0);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

        // VTK 9 leaves whichever texture unit it used last active. Legacy code
        // assumes unit 0 (glTexCoord only feeds unit 0), and any fixed-function
        // texturing left enabled on another unit would modulate every colour.
        GLint units = 0;
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
        for (GLint unit = units - 1; unit >= 0; --unit) {
                glActiveTexture(GL_TEXTURE0 + unit);
                glDisable(GL_TEXTURE_1D);
                glDisable(GL_TEXTURE_2D);
                glDisable(GL_TEXTURE_3D);
        }
        glClientActiveTexture(GL_TEXTURE0);

        // vtkMatrix4x4 is row-major, hence the transpose loads. (The
        // GetProjectionTransformMatrix(vtkRenderer*) overload returns an
        // already transposed matrix, so use the explicit one, with the aspect
        // computed as vtkOpenGLCamera does.)
        int width = 0, height = 0, lowerLeft[2];
        ren->GetTiledSizeAndOrigin(&width, &height, lowerLeft, lowerLeft + 1);
        const double aspect = (height > 0) ? static_cast<double>(width) / height : 1.0;
        vtkCamera* cam = ren->GetActiveCamera();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadTransposeMatrixd(cam->GetProjectionTransformMatrix(aspect, -1, 1)->GetData());
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadTransposeMatrixd(cam->GetViewTransformMatrix()->GetData());
}

GNC::GCS::LegacyGLScope::~LegacyGLScope()
{
        glActiveTexture(GL_TEXTURE0);
        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glPopClientAttrib();
        glPopAttrib();
        glUseProgram(0);

        if (m_pRenWin != NULL) {
                // Pop re-applies the state VTK had cached before we started
                m_pRenWin->GetState()->Pop();
                m_pRenWin->GetState()->ResetGLActiveTexture();
        }
}
