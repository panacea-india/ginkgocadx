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

// GLDraw's matrix stacks. They are plain CPU code, so no OpenGL context is
// needed; the expected values are what fixed-function OpenGL computes.

#include <gtest/gtest.h>

#include <vtk/gldraw.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

namespace GLDraw = GNC::GCS::GLDraw;

namespace
{

class GLDrawMatrices : public ::testing::Test
{
protected:
        void SetUp() override
        {
                // Resets GLDraw's state; with no render window nothing is drawn
                renderer = vtkSmartPointer<vtkRenderer>::New();
                GLDraw::BeginFrame(renderer);
                GLDraw::MatrixMode(GL_PROJECTION);
                GLDraw::LoadIdentity();
                GLDraw::MatrixMode(GL_MODELVIEW);
                GLDraw::LoadIdentity();
        }

        void TearDown() override
        {
                GLDraw::EndFrame();
        }

        static void ExpectMatrix(GLenum which, const double expected[16])
        {
                GLdouble m[16];
                GLDraw::GetDoublev(which, m);
                for (int i = 0; i < 16; ++i) {
                        EXPECT_NEAR(m[i], expected[i], 1e-9) << "element " << i;
                }
        }

        vtkSmartPointer<vtkRenderer> renderer;
};

const double Identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

}

TEST_F(GLDrawMatrices, OrthoMatchesGlOrtho)
{
        GLDraw::MatrixMode(GL_PROJECTION);
        GLDraw::Ortho(0, 200, 0, 100, -1, 1);
        const double expected[16] = {
                2.0 / 200, 0, 0, 0,
                0, 2.0 / 100, 0, 0,
                0, 0, -1, 0,
                -1, -1, 0, 1
        };
        ExpectMatrix(GL_PROJECTION_MATRIX, expected);
}

TEST_F(GLDrawMatrices, PushPopRestoresMatrix)
{
        GLDraw::PushMatrix();
        GLDraw::Translated(5, 6, 7);
        const double translated[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1 };
        ExpectMatrix(GL_MODELVIEW_MATRIX, translated);
        GLDraw::PopMatrix();
        ExpectMatrix(GL_MODELVIEW_MATRIX, Identity);
}

TEST_F(GLDrawMatrices, TransformsComposeInCallOrder)
{
        // glTranslate then glScale: points are scaled first, then translated
        GLDraw::Translated(1, 2, 3);
        GLDraw::Scaled(2, 2, 2);
        const double expected[16] = { 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 1, 2, 3, 1 };
        ExpectMatrix(GL_MODELVIEW_MATRIX, expected);
}

TEST_F(GLDrawMatrices, RotateAboutZ)
{
        GLDraw::Rotated(90, 0, 0, 1);
        const double expected[16] = { 0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        ExpectMatrix(GL_MODELVIEW_MATRIX, expected);
}

TEST_F(GLDrawMatrices, OverlayLeavesMatricesUnchanged)
{
        // The sequence corner annotations, the brightness/contrast overlay and
        // the static rule use to draw in screen coordinates. Afterwards both
        // matrices must be as before; when GL_TEXTURE was ignored, its push
        // and pop landed on the other stacks and left them off by one.
        GLDraw::MatrixMode(GL_PROJECTION);
        GLDraw::Ortho(0, 10, 0, 10, -1, 1);
        GLDraw::MatrixMode(GL_MODELVIEW);
        GLDraw::Translated(1, 2, 3);

        GLDraw::MatrixMode(GL_TEXTURE);
        GLDraw::PushMatrix();
        GLDraw::LoadIdentity();
        GLDraw::MatrixMode(GL_PROJECTION);
        GLDraw::PushMatrix();
        GLDraw::LoadIdentity();
        GLDraw::Ortho(0, 800, 600, 0, -1, 1);
        GLDraw::MatrixMode(GL_MODELVIEW);
        GLDraw::PushMatrix();
        GLDraw::LoadIdentity();
        // ... overlay drawing ...
        GLDraw::PopMatrix();
        GLDraw::MatrixMode(GL_PROJECTION);
        GLDraw::PopMatrix();
        GLDraw::MatrixMode(GL_TEXTURE);
        GLDraw::PopMatrix();
        GLDraw::MatrixMode(GL_MODELVIEW);

        const double translated[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 2, 3, 1 };
        ExpectMatrix(GL_MODELVIEW_MATRIX, translated);
        const double ortho[16] = { 0.2, 0, 0, 0, 0, 0.2, 0, 0, 0, 0, -1, 0, -1, -1, 0, 1 };
        ExpectMatrix(GL_PROJECTION_MATRIX, ortho);
}

TEST_F(GLDrawMatrices, EmulatedCapabilities)
{
        EXPECT_FALSE(GLDraw::IsEnabled(GL_LINE_STIPPLE));
        GLDraw::Enable(GL_LINE_STIPPLE);
        EXPECT_TRUE(GLDraw::IsEnabled(GL_LINE_STIPPLE));
        GLDraw::Disable(GL_LINE_STIPPLE);
        EXPECT_FALSE(GLDraw::IsEnabled(GL_LINE_STIPPLE));
}
