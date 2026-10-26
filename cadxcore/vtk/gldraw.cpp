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
#include "gldraw.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <vector>

#include <vtkCamera.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkOpenGLBufferObject.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkOpenGLShaderCache.h>
#include <vtkOpenGLState.h>
#include <vtkOpenGLVertexArrayObject.h>
#include <vtkRenderer.h>
#include <vtkShaderProgram.h>
#include <vtkSmartPointer.h>
#include <vtkWeakPointer.h>

namespace
{

// Shaders follow VTK's conventions so vtkOpenGLShaderCache can add the GLSL
// version line and the fragment output declaration for the context in use.
const char* VertexShader =
        "//VTK::System::Dec\n"
        "in vec4 aPos;\n"
        "in vec4 aColor;\n"
        "in vec2 aTex;\n"
        "in float aDist;\n"
        "uniform mat4 uMVP;\n"
        "uniform float uPointSize;\n"
        "out vec4 vColor;\n"
        "out vec2 vTex;\n"
        "out float vDist;\n"
        "void main()\n"
        "{\n"
        "  gl_Position = uMVP * aPos;\n"
        "  gl_PointSize = uPointSize;\n"
        "  vColor = aColor;\n"
        "  vTex = aTex;\n"
        "  vDist = aDist;\n"
        "}\n";

const char* FragmentShader =
        "//VTK::System::Dec\n"
        "//VTK::Output::Dec\n"
        "in vec4 vColor;\n"
        "in vec2 vTex;\n"
        "in float vDist;\n"
        "uniform sampler2D uTexture;\n"
        "uniform int uTextureMode;\n"     // 0 off, 1 modulate, 2 replace
        "uniform int uStipplePattern;\n"  // < 0: no stipple
        "uniform float uStippleFactor;\n"
        "uniform int uRoundPoints;\n"
        "uniform int uAlphaTest;\n"
        "uniform float uAlphaRef;\n"
        "void main()\n"
        "{\n"
        "  if (uStipplePattern >= 0) {\n"
        "    int bit = int(mod(floor(vDist / uStippleFactor), 16.0));\n"
        "    if (((uStipplePattern >> bit) & 1) == 0) discard;\n"
        "  }\n"
        "  if (uRoundPoints != 0) {\n"
        "    vec2 d = gl_PointCoord - vec2(0.5);\n"
        "    if (dot(d, d) > 0.25) discard;\n"
        "  }\n"
        "  vec4 c = vColor;\n"
        "  if (uTextureMode == 1) c *= texture(uTexture, vTex);\n"
        "  else if (uTextureMode == 2) c = texture(uTexture, vTex);\n"
        "  if (uAlphaTest != 0 && !(c.a > uAlphaRef)) discard;\n"
        "  gl_FragData[0] = c;\n"
        "}\n";

// Column-major 4x4 matrices, as OpenGL stores them
struct Matrix {
        double m[16];
};

void SetIdentity(Matrix& out)
{
        for (int i = 0; i < 16; ++i) {
                out.m[i] = (i % 5 == 0) ? 1.0 : 0.0;
        }
}

// out = a * b
Matrix Multiply(const Matrix& a, const Matrix& b)
{
        Matrix out;
        for (int col = 0; col < 4; ++col) {
                for (int row = 0; row < 4; ++row) {
                        double sum = 0.0;
                        for (int k = 0; k < 4; ++k) {
                                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                        }
                        out.m[col * 4 + row] = sum;
                }
        }
        return out;
}

void Transform(const Matrix& m, const double in[4], double out[4])
{
        for (int row = 0; row < 4; ++row) {
                out[row] = m.m[row] * in[0] + m.m[4 + row] * in[1] + m.m[8 + row] * in[2] + m.m[12 + row] * in[3];
        }
}

// Row-major vtkMatrix4x4 -> column-major
Matrix FromVTK(vtkMatrix4x4* vm)
{
        Matrix out;
        for (int row = 0; row < 4; ++row) {
                for (int col = 0; col < 4; ++col) {
                        out.m[col * 4 + row] = vm->GetElement(row, col);
                }
        }
        return out;
}

struct Vertex {
        float pos[4];
        float color[4];
        float tex[2];
        float dist;
};

struct State {
        float color[4];
        float texCoord[2];
        float lineWidth;
        float pointSize;
        bool stipple;
        int stippleFactor;
        unsigned short stipplePattern;
        bool texture2D;
        GLint textureEnvMode;
        bool pointSmooth;
        bool alphaTest;
        float alphaRef;
};

struct SavedAttributes {
        State state;
        bool blend;
        bool depthTest;
        bool scissorTest;
        GLboolean depthMask;
        GLint blendSrc;
        GLint blendDst;
        bool lineSmooth;
        bool polygonSmooth;
        bool cullFace;
        GLint cullFaceMode;
        GLint frontFace;
        GLint polygonMode[2];
};

// GL objects belong to one context; each render window has its own.
struct WindowResources {
        vtkWeakPointer<vtkOpenGLRenderWindow> window;
        vtkMTimeType contextTime;
        vtkSmartPointer<vtkOpenGLVertexArrayObject> vao;
        vtkSmartPointer<vtkOpenGLBufferObject> vbo;
};

std::map<vtkOpenGLRenderWindow*, WindowResources> Resources;
vtkOpenGLRenderWindow* CurrentWindow = NULL;

GLenum CurrentMode = GL_MODELVIEW;
std::vector<Matrix> ModelView(1);
std::vector<Matrix> Projection(1);
State Current;
std::vector<SavedAttributes> AttributeStack;

bool InPrimitive = false;
GLenum PrimitiveMode = GL_POINTS;
std::vector<Vertex> PrimitiveVertices;

void ResetState()
{
        Current.color[0] = Current.color[1] = Current.color[2] = Current.color[3] = 1.0f;
        Current.texCoord[0] = Current.texCoord[1] = 0.0f;
        Current.lineWidth = 1.0f;
        Current.pointSize = 1.0f;
        Current.stipple = false;
        Current.stippleFactor = 1;
        Current.stipplePattern = 0xFFFF;
        Current.texture2D = false;
        Current.textureEnvMode = GL_MODULATE;
        Current.pointSmooth = false;
        Current.alphaTest = false;
        Current.alphaRef = 0.0f;
}

std::vector<Matrix>& CurrentStack()
{
        return (CurrentMode == GL_PROJECTION) ? Projection : ModelView;
}

WindowResources* GetResources()
{
        if (CurrentWindow == NULL) {
                return NULL;
        }
        WindowResources& res = Resources[CurrentWindow];
        if (res.window.GetPointer() != CurrentWindow || res.contextTime != CurrentWindow->GetContextCreationTime() || !res.vao) {
                res.window = CurrentWindow;
                res.contextTime = CurrentWindow->GetContextCreationTime();
                res.vao = vtkSmartPointer<vtkOpenGLVertexArrayObject>::New();
                res.vbo = vtkSmartPointer<vtkOpenGLBufferObject>::New();
                res.vbo->SetType(vtkOpenGLBufferObject::ArrayBuffer);
        }
        return &res;
}

void PushVertex(double x, double y, double z)
{
        if (!InPrimitive) {
                return;
        }
        Vertex v;
        v.pos[0] = static_cast<float>(x);
        v.pos[1] = static_cast<float>(y);
        v.pos[2] = static_cast<float>(z);
        v.pos[3] = 1.0f;
        std::memcpy(v.color, Current.color, sizeof(v.color));
        v.tex[0] = Current.texCoord[0];
        v.tex[1] = Current.texCoord[1];
        v.dist = 0.0f;
        PrimitiveVertices.push_back(v);
}

// Line segments as index pairs into PrimitiveVertices
void LineSegments(std::vector<std::pair<size_t, size_t> >& segments)
{
        const size_t n = PrimitiveVertices.size();
        if (PrimitiveMode == GL_LINES) {
                for (size_t i = 0; i + 1 < n; i += 2) {
                        segments.push_back(std::make_pair(i, i + 1));
                }
        } else {
                for (size_t i = 0; i + 1 < n; ++i) {
                        segments.push_back(std::make_pair(i, i + 1));
                }
                if (PrimitiveMode == GL_LINE_LOOP && n > 2) {
                        segments.push_back(std::make_pair(n - 1, size_t(0)));
                }
        }
}

void Draw(GLenum glMode, const std::vector<Vertex>& vertices, const Matrix& mvp, bool lines)
{
        WindowResources* res = GetResources();
        if (res == NULL || vertices.empty()) {
                return;
        }

        vtkShaderProgram* program = CurrentWindow->GetShaderCache()->ReadyShaderProgram(VertexShader, FragmentShader, "");
        if (program == NULL) {
                return;
        }

        float mvpf[16];
        for (int i = 0; i < 16; ++i) {
                mvpf[i] = static_cast<float>(mvp.m[i]);
        }
        program->SetUniformMatrix4x4("uMVP", mvpf);
        program->SetUniformf("uPointSize", Current.pointSize);

        int textureMode = 0;
        if (Current.texture2D) {
                textureMode = (Current.textureEnvMode == GL_REPLACE) ? 2 : 1;
                glActiveTexture(GL_TEXTURE0);
                program->SetUniformi("uTexture", 0);
        }
        program->SetUniformi("uTextureMode", textureMode);
        program->SetUniformi("uStipplePattern", (lines && Current.stipple) ? static_cast<int>(Current.stipplePattern) : -1);
        program->SetUniformf("uStippleFactor", static_cast<float>(Current.stippleFactor > 0 ? Current.stippleFactor : 1));
        program->SetUniformi("uRoundPoints", (glMode == GL_POINTS && Current.pointSmooth) ? 1 : 0);
        program->SetUniformi("uAlphaTest", Current.alphaTest ? 1 : 0);
        program->SetUniformf("uAlphaRef", Current.alphaRef);

        res->vao->Bind();
        res->vbo->Upload(reinterpret_cast<const float*>(&vertices[0]), vertices.size() * sizeof(Vertex) / sizeof(float),
                         vtkOpenGLBufferObject::ArrayBuffer);
        const size_t stride = sizeof(Vertex);
        res->vao->AddAttributeArray(program, res->vbo, "aPos", offsetof(Vertex, pos), stride, VTK_FLOAT, 4, false);
        res->vao->AddAttributeArray(program, res->vbo, "aColor", offsetof(Vertex, color), stride, VTK_FLOAT, 4, false);
        res->vao->AddAttributeArray(program, res->vbo, "aTex", offsetof(Vertex, tex), stride, VTK_FLOAT, 2, false);
        res->vao->AddAttributeArray(program, res->vbo, "aDist", offsetof(Vertex, dist), stride, VTK_FLOAT, 1, false);

        if (glMode == GL_POINTS) {
                glEnable(GL_PROGRAM_POINT_SIZE);
        }
        glDrawArrays(glMode, 0, static_cast<GLsizei>(vertices.size()));
        if (glMode == GL_POINTS) {
                glDisable(GL_PROGRAM_POINT_SIZE);
        }

        res->vao->Release();
        // Code that still draws with fixed-function OpenGL may follow
        CurrentWindow->GetShaderCache()->ReleaseCurrentShader();
}

void DrawLines(const Matrix& mvp)
{
        std::vector<std::pair<size_t, size_t> > segments;
        LineSegments(segments);
        if (segments.empty()) {
                return;
        }

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        const double vw = viewport[2] > 0 ? viewport[2] : 1;
        const double vh = viewport[3] > 0 ? viewport[3] : 1;

        // Window coordinates of every vertex: stipple and wide lines are
        // defined in pixels, as in fixed-function OpenGL
        const size_t n = PrimitiveVertices.size();
        std::vector<double> sx(n), sy(n), nz(n);
        std::vector<bool> visible(n);
        for (size_t i = 0; i < n; ++i) {
                double in[4] = { PrimitiveVertices[i].pos[0], PrimitiveVertices[i].pos[1], PrimitiveVertices[i].pos[2], 1.0 };
                double clip[4];
                Transform(mvp, in, clip);
                visible[i] = clip[3] > 0.0;
                const double w = visible[i] ? clip[3] : 1.0;
                sx[i] = (clip[0] / w + 1.0) * 0.5 * vw;
                sy[i] = (clip[1] / w + 1.0) * 0.5 * vh;
                nz[i] = clip[2] / w;
        }

        const bool wide = Current.lineWidth > 1.0f;
        std::vector<Vertex> out;
        out.reserve(segments.size() * (wide ? 6 : 2));

        // The stipple counter restarts for every GL_LINES segment and runs on
        // along strips and loops
        double distance = 0.0;
        for (size_t s = 0; s < segments.size(); ++s) {
                const size_t a = segments[s].first;
                const size_t b = segments[s].second;
                if (PrimitiveMode == GL_LINES) {
                        distance = 0.0;
                }
                const double dx = sx[b] - sx[a];
                const double dy = sy[b] - sy[a];
                const double length = std::sqrt(dx * dx + dy * dy);
                if (!visible[a] || !visible[b]) {
                        distance += length;
                        continue;
                }

                Vertex va = PrimitiveVertices[a];
                Vertex vb = PrimitiveVertices[b];
                va.dist = static_cast<float>(distance);
                vb.dist = static_cast<float>(distance + length);
                distance += length;

                if (!wide) {
                        out.push_back(va);
                        out.push_back(vb);
                        continue;
                }

                // Expand to a quad in window coordinates, emitted in
                // normalized device coordinates (drawn with an identity MVP)
                double px = 0.0, py = 0.0;
                if (length > 0.0) {
                        px = -dy / length * Current.lineWidth * 0.5;
                        py =  dx / length * Current.lineWidth * 0.5;
                }
                const double corners[4][3] = {
                        { sx[a] + px, sy[a] + py, nz[a] },
                        { sx[a] - px, sy[a] - py, nz[a] },
                        { sx[b] + px, sy[b] + py, nz[b] },
                        { sx[b] - px, sy[b] - py, nz[b] }
                };
                Vertex quad[4] = { va, va, vb, vb };
                for (int c = 0; c < 4; ++c) {
                        quad[c].pos[0] = static_cast<float>(corners[c][0] / vw * 2.0 - 1.0);
                        quad[c].pos[1] = static_cast<float>(corners[c][1] / vh * 2.0 - 1.0);
                        quad[c].pos[2] = static_cast<float>(corners[c][2]);
                        quad[c].pos[3] = 1.0f;
                }
                out.push_back(quad[0]);
                out.push_back(quad[1]);
                out.push_back(quad[2]);
                out.push_back(quad[2]);
                out.push_back(quad[1]);
                out.push_back(quad[3]);
        }

        if (wide) {
                Matrix identity;
                SetIdentity(identity);
                Draw(GL_TRIANGLES, out, identity, true);
        } else {
                Draw(GL_LINES, out, mvp, true);
        }
}

void DrawPolygons(const Matrix& mvp)
{
        const std::vector<Vertex>& v = PrimitiveVertices;
        const size_t n = v.size();
        std::vector<Vertex> out;
        switch (PrimitiveMode) {
        case GL_TRIANGLES:
                out.assign(v.begin(), v.begin() + (n / 3) * 3);
                break;
        case GL_QUADS:
                for (size_t i = 0; i + 3 < n; i += 4) {
                        out.push_back(v[i]);
                        out.push_back(v[i + 1]);
                        out.push_back(v[i + 2]);
                        out.push_back(v[i]);
                        out.push_back(v[i + 2]);
                        out.push_back(v[i + 3]);
                }
                break;
        case GL_TRIANGLE_STRIP:
        case GL_QUAD_STRIP:
                for (size_t i = 0; i + 2 < n; ++i) {
                        if (PrimitiveMode == GL_QUAD_STRIP && (i % 2) == 1) {
                                continue;
                        }
                        if (PrimitiveMode == GL_QUAD_STRIP) {
                                if (i + 3 >= n) {
                                        break;
                                }
                                out.push_back(v[i]);
                                out.push_back(v[i + 1]);
                                out.push_back(v[i + 2]);
                                out.push_back(v[i + 2]);
                                out.push_back(v[i + 1]);
                                out.push_back(v[i + 3]);
                        } else {
                                out.push_back(v[i]);
                                out.push_back(v[(i % 2) ? i + 2 : i + 1]);
                                out.push_back(v[(i % 2) ? i + 1 : i + 2]);
                        }
                }
                break;
        default: // GL_TRIANGLE_FAN, GL_POLYGON
                for (size_t i = 1; i + 1 < n; ++i) {
                        out.push_back(v[0]);
                        out.push_back(v[i]);
                        out.push_back(v[i + 1]);
                }
                break;
        }
        Draw(GL_TRIANGLES, out, mvp, false);
}

bool EmulatedCap(GLenum cap, bool enable)
{
        switch (cap) {
        case GL_LINE_STIPPLE:
                Current.stipple = enable;
                return true;
        case GL_TEXTURE_2D:
                Current.texture2D = enable;
                return true;
        case GL_POINT_SMOOTH:
                Current.pointSmooth = enable;
                return true;
        case GL_ALPHA_TEST:
                Current.alphaTest = enable;
                return true;
        case GL_LIGHTING:
        case GL_TEXTURE_1D:
                return true;
        default:
                return false;
        }
}

}

void GNC::GCS::GLDraw::BeginFrame(vtkRenderer* ren)
{
        CurrentWindow = vtkOpenGLRenderWindow::SafeDownCast(ren->GetRenderWindow());
        ResetState();
        AttributeStack.clear();
        InPrimitive = false;
        CurrentMode = GL_MODELVIEW;

        int width = 0, height = 0, lowerLeft[2];
        ren->GetTiledSizeAndOrigin(&width, &height, lowerLeft, lowerLeft + 1);
        const double aspect = (height > 0) ? static_cast<double>(width) / height : 1.0;
        vtkCamera* cam = ren->GetActiveCamera();
        Projection.assign(1, FromVTK(cam->GetProjectionTransformMatrix(aspect, -1, 1)));
        ModelView.assign(1, FromVTK(cam->GetViewTransformMatrix()));
}

void GNC::GCS::GLDraw::EndFrame()
{
        CurrentWindow = NULL;
}

GNC::GCS::GLDraw::FrameScope::FrameScope(vtkRenderer* ren) :
        m_pRenWin(vtkOpenGLRenderWindow::SafeDownCast(ren->GetRenderWindow()))
{
        if (m_pRenWin != NULL) {
                m_pRenWin->GetState()->Push();
                // The drawing code binds its textures (text labels, the
                // magnifier) on whatever unit is active, and GLDraw samples
                // unit 0. VTK leaves other units active.
                m_pRenWin->GetState()->vtkglActiveTexture(GL_TEXTURE0);
        }
        BeginFrame(ren);
}

GNC::GCS::GLDraw::FrameScope::~FrameScope()
{
        EndFrame();
        if (m_pRenWin != NULL) {
                // Pop reapplies the state VTK had cached when the frame started
                m_pRenWin->GetState()->Pop();
                m_pRenWin->GetState()->ResetGLActiveTexture();
        }
}

void GNC::GCS::GLDraw::MatrixMode(GLenum mode)
{
        if (mode == GL_MODELVIEW || mode == GL_PROJECTION) {
                CurrentMode = mode;
        }
}

void GNC::GCS::GLDraw::PushMatrix()
{
        std::vector<Matrix>& stack = CurrentStack();
        stack.push_back(stack.back());
}

void GNC::GCS::GLDraw::PopMatrix()
{
        std::vector<Matrix>& stack = CurrentStack();
        if (stack.size() > 1) {
                stack.pop_back();
        }
}

void GNC::GCS::GLDraw::LoadIdentity()
{
        SetIdentity(CurrentStack().back());
}

void GNC::GCS::GLDraw::LoadMatrixd(const GLdouble* m)
{
        std::memcpy(CurrentStack().back().m, m, sizeof(Matrix));
}

void GNC::GCS::GLDraw::MultMatrixd(const GLdouble* m)
{
        Matrix other;
        std::memcpy(other.m, m, sizeof(Matrix));
        Matrix& top = CurrentStack().back();
        top = Multiply(top, other);
}

void GNC::GCS::GLDraw::Ortho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
        Matrix o;
        SetIdentity(o);
        o.m[0] = 2.0 / (r - l);
        o.m[5] = 2.0 / (t - b);
        o.m[10] = -2.0 / (f - n);
        o.m[12] = -(r + l) / (r - l);
        o.m[13] = -(t + b) / (t - b);
        o.m[14] = -(f + n) / (f - n);
        MultMatrixd(o.m);
}

void GNC::GCS::GLDraw::Translated(GLdouble x, GLdouble y, GLdouble z)
{
        Matrix t;
        SetIdentity(t);
        t.m[12] = x;
        t.m[13] = y;
        t.m[14] = z;
        MultMatrixd(t.m);
}

void GNC::GCS::GLDraw::Translatef(GLfloat x, GLfloat y, GLfloat z)
{
        Translated(x, y, z);
}

void GNC::GCS::GLDraw::Scaled(GLdouble x, GLdouble y, GLdouble z)
{
        Matrix s;
        SetIdentity(s);
        s.m[0] = x;
        s.m[5] = y;
        s.m[10] = z;
        MultMatrixd(s.m);
}

void GNC::GCS::GLDraw::Rotated(GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
        const double len = std::sqrt(x * x + y * y + z * z);
        if (len == 0.0) {
                return;
        }
        x /= len;
        y /= len;
        z /= len;
        const double rad = angle * M_PI / 180.0;
        const double c = std::cos(rad), s = std::sin(rad), ic = 1.0 - c;
        Matrix r;
        SetIdentity(r);
        r.m[0] = x * x * ic + c;
        r.m[1] = y * x * ic + z * s;
        r.m[2] = x * z * ic - y * s;
        r.m[4] = x * y * ic - z * s;
        r.m[5] = y * y * ic + c;
        r.m[6] = y * z * ic + x * s;
        r.m[8] = x * z * ic + y * s;
        r.m[9] = y * z * ic - x * s;
        r.m[10] = z * z * ic + c;
        MultMatrixd(r.m);
}

void GNC::GCS::GLDraw::Color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
        Current.color[0] = r;
        Current.color[1] = g;
        Current.color[2] = b;
        Current.color[3] = a;
}

void GNC::GCS::GLDraw::Color4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a)
{
        Color4f(static_cast<GLfloat>(r), static_cast<GLfloat>(g), static_cast<GLfloat>(b), static_cast<GLfloat>(a));
}

void GNC::GCS::GLDraw::Color3f(GLfloat r, GLfloat g, GLfloat b)
{
        Color4f(r, g, b, 1.0f);
}

void GNC::GCS::GLDraw::LineWidth(GLfloat width)
{
        Current.lineWidth = width;
}

void GNC::GCS::GLDraw::PointSize(GLfloat size)
{
        Current.pointSize = size;
}

void GNC::GCS::GLDraw::LineStipple(GLint factor, GLushort pattern)
{
        Current.stippleFactor = factor;
        Current.stipplePattern = pattern;
}

void GNC::GCS::GLDraw::TexEnvi(GLenum /*target*/, GLenum pname, GLint param)
{
        if (pname == GL_TEXTURE_ENV_MODE) {
                Current.textureEnvMode = param;
        }
}

void GNC::GCS::GLDraw::TexEnvf(GLenum target, GLenum pname, GLfloat param)
{
        TexEnvi(target, pname, static_cast<GLint>(param));
}

void GNC::GCS::GLDraw::AlphaFunc(GLenum /*func*/, GLclampf ref)
{
        // Only GL_GREATER is used by the widgets
        Current.alphaRef = ref;
}

void GNC::GCS::GLDraw::Hint(GLenum /*target*/, GLenum /*mode*/)
{
}

void GNC::GCS::GLDraw::Enable(GLenum cap)
{
        if (!EmulatedCap(cap, true)) {
                glEnable(cap);
        }
}

void GNC::GCS::GLDraw::Disable(GLenum cap)
{
        if (!EmulatedCap(cap, false)) {
                glDisable(cap);
        }
}

GLboolean GNC::GCS::GLDraw::IsEnabled(GLenum cap)
{
        switch (cap) {
        case GL_LINE_STIPPLE:
                return Current.stipple;
        case GL_TEXTURE_2D:
                return Current.texture2D;
        case GL_POINT_SMOOTH:
                return Current.pointSmooth;
        case GL_ALPHA_TEST:
                return Current.alphaTest;
        case GL_LIGHTING:
        case GL_TEXTURE_1D:
                return GL_FALSE;
        default:
                return glIsEnabled(cap);
        }
}

void GNC::GCS::GLDraw::PushAttrib(GLbitfield /*mask*/)
{
        SavedAttributes saved;
        saved.state = Current;
        saved.blend = glIsEnabled(GL_BLEND);
        saved.depthTest = glIsEnabled(GL_DEPTH_TEST);
        saved.scissorTest = glIsEnabled(GL_SCISSOR_TEST);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &saved.depthMask);
        glGetIntegerv(GL_BLEND_SRC_RGB, &saved.blendSrc);
        glGetIntegerv(GL_BLEND_DST_RGB, &saved.blendDst);
        saved.lineSmooth = glIsEnabled(GL_LINE_SMOOTH);
        saved.polygonSmooth = glIsEnabled(GL_POLYGON_SMOOTH);
        saved.cullFace = glIsEnabled(GL_CULL_FACE);
        glGetIntegerv(GL_CULL_FACE_MODE, &saved.cullFaceMode);
        glGetIntegerv(GL_FRONT_FACE, &saved.frontFace);
        saved.polygonMode[0] = saved.polygonMode[1] = GL_FILL;
        glGetIntegerv(GL_POLYGON_MODE, saved.polygonMode);
        AttributeStack.push_back(saved);
}

void GNC::GCS::GLDraw::PopAttrib()
{
        if (AttributeStack.empty()) {
                return;
        }
        const SavedAttributes& saved = AttributeStack.back();
        Current = saved.state;
        saved.blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        saved.depthTest ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
        saved.scissorTest ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
        glDepthMask(saved.depthMask);
        glBlendFunc(saved.blendSrc, saved.blendDst);
        saved.lineSmooth ? glEnable(GL_LINE_SMOOTH) : glDisable(GL_LINE_SMOOTH);
        saved.polygonSmooth ? glEnable(GL_POLYGON_SMOOTH) : glDisable(GL_POLYGON_SMOOTH);
        saved.cullFace ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
        glCullFace(saved.cullFaceMode);
        glFrontFace(saved.frontFace);
        glPolygonMode(GL_FRONT_AND_BACK, saved.polygonMode[0]);
        AttributeStack.pop_back();
}

void GNC::GCS::GLDraw::GetDoublev(GLenum pname, GLdouble* params)
{
        if (pname == GL_MODELVIEW_MATRIX) {
                std::memcpy(params, ModelView.back().m, sizeof(Matrix));
        } else if (pname == GL_PROJECTION_MATRIX) {
                std::memcpy(params, Projection.back().m, sizeof(Matrix));
        } else {
                glGetDoublev(pname, params);
        }
}

void GNC::GCS::GLDraw::GetFloatv(GLenum pname, GLfloat* params)
{
        if (pname == GL_MODELVIEW_MATRIX || pname == GL_PROJECTION_MATRIX) {
                GLdouble d[16];
                GetDoublev(pname, d);
                for (int i = 0; i < 16; ++i) {
                        params[i] = static_cast<GLfloat>(d[i]);
                }
        } else {
                glGetFloatv(pname, params);
        }
}

void GNC::GCS::GLDraw::Begin(GLenum mode)
{
        InPrimitive = true;
        PrimitiveMode = mode;
        PrimitiveVertices.clear();
}

void GNC::GCS::GLDraw::End()
{
        if (!InPrimitive) {
                return;
        }
        InPrimitive = false;
        if (PrimitiveVertices.empty() || CurrentWindow == NULL) {
                return;
        }

        const Matrix mvp = Multiply(Projection.back(), ModelView.back());
        switch (PrimitiveMode) {
        case GL_POINTS:
                Draw(GL_POINTS, PrimitiveVertices, mvp, false);
                break;
        case GL_LINES:
        case GL_LINE_STRIP:
        case GL_LINE_LOOP:
                DrawLines(mvp);
                break;
        default:
                DrawPolygons(mvp);
                break;
        }
        PrimitiveVertices.clear();
}

void GNC::GCS::GLDraw::Vertex2d(GLdouble x, GLdouble y)
{
        PushVertex(x, y, 0.0);
}

void GNC::GCS::GLDraw::Vertex2f(GLfloat x, GLfloat y)
{
        PushVertex(x, y, 0.0);
}

void GNC::GCS::GLDraw::Vertex2dv(const GLdouble* v)
{
        PushVertex(v[0], v[1], 0.0);
}

void GNC::GCS::GLDraw::Vertex3f(GLfloat x, GLfloat y, GLfloat z)
{
        PushVertex(x, y, z);
}

void GNC::GCS::GLDraw::Vertex3d(GLdouble x, GLdouble y, GLdouble z)
{
        PushVertex(x, y, z);
}

void GNC::GCS::GLDraw::Vertex3dv(const GLdouble* v)
{
        PushVertex(v[0], v[1], v[2]);
}

void GNC::GCS::GLDraw::TexCoord2d(GLdouble s, GLdouble t)
{
        Current.texCoord[0] = static_cast<float>(s);
        Current.texCoord[1] = static_cast<float>(t);
}

void GNC::GCS::GLDraw::TexCoord2f(GLfloat s, GLfloat t)
{
        TexCoord2d(s, t);
}
