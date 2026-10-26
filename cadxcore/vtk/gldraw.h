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
#include <vtk_glew.h>

class vtkRenderer;

namespace GNC
{
namespace GCS
{

/**
 * The subset of fixed-function OpenGL the widgets use (glBegin/glEnd, the
 * matrix stack, colour, line width and stipple, point size, 2D texturing),
 * implemented with a shader and vertex buffers so it also works in a core
 * profile context.
 *
 * Functions are named after the GL calls they replace and have the same
 * semantics. Calls must happen between BeginFrame and EndFrame, which
 * GNC::GCS::LegacyGLScope does around widget rendering.
 */
namespace GLDraw
{
// Frame: sets the matrix stacks from the renderer's camera and selects the
// GL resources of its render window.
EXTAPI void BeginFrame(vtkRenderer* ren);
EXTAPI void EndFrame();

// Matrix stack (GL_MODELVIEW and GL_PROJECTION)
EXTAPI void MatrixMode(GLenum mode);
EXTAPI void PushMatrix();
EXTAPI void PopMatrix();
EXTAPI void LoadIdentity();
EXTAPI void LoadMatrixd(const GLdouble* m);
EXTAPI void MultMatrixd(const GLdouble* m);
EXTAPI void Ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
EXTAPI void Translated(GLdouble x, GLdouble y, GLdouble z);
EXTAPI void Translatef(GLfloat x, GLfloat y, GLfloat z);
EXTAPI void Scaled(GLdouble x, GLdouble y, GLdouble z);
EXTAPI void Rotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);

// Current state
EXTAPI void Color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
EXTAPI void Color4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a);
EXTAPI void Color3f(GLfloat r, GLfloat g, GLfloat b);
EXTAPI void LineWidth(GLfloat width);
EXTAPI void PointSize(GLfloat size);
EXTAPI void LineStipple(GLint factor, GLushort pattern);
EXTAPI void TexEnvi(GLenum target, GLenum pname, GLint param);
EXTAPI void TexEnvf(GLenum target, GLenum pname, GLfloat param);
EXTAPI void AlphaFunc(GLenum func, GLclampf ref);
EXTAPI void Hint(GLenum target, GLenum mode);

// Capabilities. Emulated: GL_LINE_STIPPLE, GL_TEXTURE_2D, GL_POINT_SMOOTH,
// GL_ALPHA_TEST, GL_LIGHTING (ignored). Everything else goes to OpenGL.
EXTAPI void Enable(GLenum cap);
EXTAPI void Disable(GLenum cap);
EXTAPI GLboolean IsEnabled(GLenum cap);

// Saves and restores the emulated state plus blending, depth and scissor.
EXTAPI void PushAttrib(GLbitfield mask);
EXTAPI void PopAttrib();

// Matrix queries (GL_MODELVIEW_MATRIX, GL_PROJECTION_MATRIX); other names go to OpenGL.
EXTAPI void GetDoublev(GLenum pname, GLdouble* params);
EXTAPI void GetFloatv(GLenum pname, GLfloat* params);

// Immediate mode
EXTAPI void Begin(GLenum mode);
EXTAPI void End();
EXTAPI void Vertex2d(GLdouble x, GLdouble y);
EXTAPI void Vertex2f(GLfloat x, GLfloat y);
EXTAPI void Vertex2dv(const GLdouble* v);
EXTAPI void Vertex3f(GLfloat x, GLfloat y, GLfloat z);
EXTAPI void Vertex3d(GLdouble x, GLdouble y, GLdouble z);
EXTAPI void Vertex3dv(const GLdouble* v);
EXTAPI void TexCoord2d(GLdouble s, GLdouble t);
EXTAPI void TexCoord2f(GLfloat s, GLfloat t);
}

}
}
