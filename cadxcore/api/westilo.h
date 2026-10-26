/*
 * This file is part of Ginkgo CADx
 *
 * Copyright (c) 2015-2016 Gert Wollny
 * Copyright (c) 2008-2014 MetaEmotion S.L. All rights reserved.
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

#include <widgets/openglhelper.h>
#include <api/icontexto.h>
#include <api/iwidgetsrenderer.h>
#include <vtk/vtkginkgoimageviewer.h>
#include <cairo/cairo.h>
#include <vtk/gldraw.h>

typedef struct TWidgetColor {
        float r, g, b, a;
        TWidgetColor(float r, float g, float b, float a)
        {
                this->r = r;
                this->g = g;
                this->b = b;
                this->a = a;
        }

        inline void Aplicar() const
        {
                GNC::GCS::GLDraw::Color4f(this->r, this->g, this->b, this->a);
        }
        inline void AplicarCairo(struct _cairo* cr) const
        {
                cairo_set_source_rgba(cr, this->r, this->g, this->b, this->a);
        }
} TWidgetColor;

#define ConsumirEvento() evento.Skip(false)
#define EventoConsumido() !evento.GetSkipped()

#define WIDGETS_TEXT_FONT_FAMILY "Arial"

#define ANNOTATOR_TEXT_COLOR        0.0f, 0.90f, 0.90f, 1.0f
#define DEFAULT_TEXT_COLOR          0.0f, 0.75f, 0.75f, 1.0f
#define DEFAULT_TEXT_COLOR_INVERTED 0.0f, 0.0f, 0.0f, 1.0f

static TWidgetColor wColorNormal(0.0f, 1.0f, 0.0f, 1.0f); // GNC::GCS::GLDraw::Color4f(0.0f, 1.0f, 0.0f, 0.75f);
static TWidgetColor wColorNormalSuave(0.0f, 0.5f, 0.0f, 0.5f); // GNC::GCS::GLDraw::Color4f(0.0f, 1.0f, 0.0f, 0.75f);
static TWidgetColor wColorIluminado(1.0f, 1.0f, 0.0f, 1.0f); // GNC::GCS::GLDraw::Color4f(1.0f, 1.0f, 0.0f, 1.0f);
static TWidgetColor wColorMoviendose(1.0f, 0.0f, 0.0f, 1.0f);
static TWidgetColor wColorSeleccionado(1.0f, 0.0f, 0.0f, 1.0f);
static TWidgetColor wColorSeleccionadoIluminado(1.0f, 0.5f, 0.0f, 1.0f);

static TWidgetColor wColorSombra(0.0f, 0.0f, 0.0f, 0.2f);
static TWidgetColor wColorLineaArrastre(1.0f, 0.0f, 0.0f, 1.0f);

#define WIDGET_SIZE_PUNTO 8.0f

static GNC::GCS::Vector wRadioPuntoIluminado(15.0f, 15.0f);

static TWidgetColor wColorPuntoIluminadoInterior(1.0f, 1.0f, 0.0f, 0.3f);
static TWidgetColor wColorPuntoIluminadoExterior(1.0f, 1.0f, 0.0f, 0.0f);

static TWidgetColor wColorAngulo(1.0f, 0.0f, 0.0f, 0.8f);
static TWidgetColor wColorAnguloInterior(1.0f, 0.0f, 0.0f, 0.25f);

#define WIDGET_OFFSET_X_SOMBRA					1.0f
#define WIDGET_OFFSET_Y_SOMBRA					1.0f

#define WIDGET_OFFSET_X_SOMBRA_SELECCIONADO		2.0f
#define WIDGET_OFFSET_Y_SOMBRA_SELECCIONADO		2.0f

#define WIDGET_INCREMENTO_DETALLE_SELECCIONADO	1.15f

#define ANNOTATION_FP_PRECISION 2
#define ANNOTATION_TEXT_WITH_SHADOW true

// -- Comportamiento --------------------------------------------------------------------------------

inline void wAplicarColor(bool iluminado, bool moviendose, bool seleccionado)
{
        if (moviendose && seleccionado) {
                wColorSeleccionadoIluminado.Aplicar();
                return;
        } else if (!iluminado && seleccionado) {
                wColorSeleccionado.Aplicar();
                return;
        } else if (iluminado && seleccionado) {
                wColorSeleccionadoIluminado.Aplicar();
                return;
        } else if (iluminado && !seleccionado) {
                wColorIluminado.Aplicar();
                return;
        } else {
                wColorNormal.Aplicar();
                return;
        }
}

inline void wAplicarColorSuave(bool iluminado, bool moviendose, bool seleccionado)
{
        if (moviendose && seleccionado) {
                wColorSeleccionadoIluminado.Aplicar();
                return;
        } else if (!iluminado && seleccionado) {
                wColorSeleccionado.Aplicar();
                return;
        } else if (iluminado && seleccionado) {
                wColorSeleccionadoIluminado.Aplicar();
                return;
        } else if (iluminado && !seleccionado) {
                wColorIluminado.Aplicar();
                return;
        } else {
                wColorNormalSuave.Aplicar();
                return;
        }
}


inline void wDibujarElipseDegradado(const GNC::GCS::Vector &centro, const GNC::GCS::Vector &radio, const TWidgetColor &interior, const TWidgetColor &exterior, float resolucion=60.0f)
{
        GNC::GCS::GLDraw::Begin(GL_TRIANGLE_FAN);
        float step = M_2PI / resolucion;

        interior.Aplicar();
        GNC::GCS::GLDraw::Vertex3f(centro.x, centro.y, 0.0f);

        exterior.Aplicar();
        for (float angulo = 0; angulo < M_2PI; angulo += step) {
                GNC::GCS::GLDraw::Vertex3f(centro.x + (radio.x * std::cos (angulo)) , centro.y + (radio.y * std::sin (angulo)), 0.0f);
        }
        GNC::GCS::GLDraw::End();
}


inline void dibujarDecoracionTextura(const GNC::GCS::Vector* const quad, const GNC::GCS::Vector& escala)
{
        /*
         * Posicion del Quad
         *  q3   q2
         *  q0   q1
         * */

        // Dibujo el fondo
        GNC::GCS::GLDraw::Color4f(0.0f, 0.0f, 0.0f, 0.61f);
        GNC::GCS::GLDraw::Begin(GL_QUADS);
        GNC::GCS::GLDraw::Vertex2d(quad[0].x, quad[0].y);
        GNC::GCS::GLDraw::Vertex2d(quad[1].x, quad[1].y);
        GNC::GCS::GLDraw::Vertex2d(quad[2].x, quad[2].y);
        GNC::GCS::GLDraw::Vertex2d(quad[3].x, quad[3].y);
        GNC::GCS::GLDraw::End();

        // Dibujado el borde
        GNC::GCS::GLDraw::Color4f(0.2f, 0.2f, 0.2f, 1.0f);
        GNC::GCS::GLDraw::Begin(GL_LINE_LOOP);
        GNC::GCS::GLDraw::Vertex2d(quad[0].x, quad[0].y);
        GNC::GCS::GLDraw::Vertex2d(quad[1].x, quad[1].y);
        GNC::GCS::GLDraw::Vertex2d(quad[2].x, quad[2].y);
        GNC::GCS::GLDraw::Vertex2d(quad[3].x, quad[3].y);
        GNC::GCS::GLDraw::End();

        // Dibujado de la sombra, lo hago en 2 tramos para evitar overlapping
        GNC::GCS::GLDraw::Color4f(0.0f, 0.0f, 0.0f, 0.3f);
        GNC::GCS::GLDraw::Begin(GL_LINE_STRIP);
        GNC::GCS::GLDraw::Vertex2d(quad[1].x + 2 * escala.x, quad[1].y - 2 * escala.y);
        GNC::GCS::GLDraw::Vertex2d(quad[2].x + 2 * escala.x, quad[2].y - 2 * escala.y);
        GNC::GCS::GLDraw::End();

        GNC::GCS::GLDraw::Begin(GL_LINE_STRIP);
        GNC::GCS::GLDraw::Vertex2d(quad[2].x + 1 * escala.x, quad[2].y - 2 * escala.y);
        GNC::GCS::GLDraw::Vertex2d(quad[3].x + 2 * escala.x, quad[3].y - 2 * escala.y);
        GNC::GCS::GLDraw::End();

}

inline void wDibujarTexturaTextoDecorada(GNC::GCS::TexturaCairo& tc, const GNC::GCS::Vector* const quad, const GNC::GCS::Vector& escala, bool flipHorizontal, bool flipVertical, double anguloCamara)
{

        dibujarDecoracionTextura(quad, escala);
        tc.Render(quad, flipHorizontal, flipVertical, anguloCamara);
}

inline void wDibujarTexturaTextoDecorada2D(GNC::GCS::Contexto3D* c, GNC::GCS::TexturaCairo& tc, const GNC::GCS::Vector& rasterPos)
{
        /*
         * Posicion del Quad
         *  q3   q2
         *  q0   q1
         * */

        GNC::GCS::Vector escala(1.0, 1.0);

        GNC::GCS::GLDraw::MatrixMode(GL_PROJECTION);
        GNC::GCS::GLDraw::PushMatrix();
        GNC::GCS::GLDraw::LoadIdentity();

        GNC::GCS::GLDraw::Ortho(0, c->ancho, 0, c->alto, -1, 1);
        GNC::GCS::GLDraw::MatrixMode(GL_MODELVIEW);
        GNC::GCS::GLDraw::PushMatrix();
        GNC::GCS::GLDraw::LoadIdentity();

        GNC::GCS::Vector quad[4];

        quad[0].x = rasterPos.x;
        quad[0].y = rasterPos.y;

        quad[2].x = quad[0].x + (double)tc.ancho;
        quad[2].y = quad[0].y - (double)tc.alto;

        quad[1].x = quad[2].x;
        quad[1].y = quad[0].y;

        quad[3].x = quad[0].x;
        quad[3].y = quad[2].y;

        dibujarDecoracionTextura(quad, escala);

        quad[2].x = quad[0].x + (double)tc.anchoPOT;
        quad[2].y = quad[0].y - (double)tc.altoPOT;

        quad[1].x = quad[2].x;
        quad[1].y = quad[0].y;

        quad[3].x = quad[0].x;
        quad[3].y = quad[2].y;

        tc.Render2D(quad);

        GNC::GCS::GLDraw::PopMatrix();
        GNC::GCS::GLDraw::MatrixMode(GL_PROJECTION);
        GNC::GCS::GLDraw::PopMatrix();
        GNC::GCS::GLDraw::MatrixMode(GL_MODELVIEW);
}

inline void wDibujarTexturaTexto2D(GNC::GCS::Contexto3D* c, GNC::GCS::TexturaCairo& tc, const GNC::GCS::Vector& rasterPos)
{
        /*
         * Posicion del Quad
         *  q3   q2
         *  q0   q1
         * */

        GNC::GCS::GLDraw::MatrixMode(GL_PROJECTION);
        GNC::GCS::GLDraw::PushMatrix();
        GNC::GCS::GLDraw::LoadIdentity();

        GNC::GCS::GLDraw::Ortho(0, c->ancho, 0, c->alto, -1, 1);
        GNC::GCS::GLDraw::MatrixMode(GL_MODELVIEW);
        GNC::GCS::GLDraw::PushMatrix();
        GNC::GCS::GLDraw::LoadIdentity();

        GNC::GCS::Vector quad[4];

        quad[0].x = rasterPos.x;
        quad[0].y = rasterPos.y;

        quad[2].x = quad[0].x + (double)tc.anchoPOT;
        quad[2].y = quad[0].y - (double)tc.altoPOT;

        quad[1].x = quad[2].x;
        quad[1].y = quad[0].y;

        quad[3].x = quad[0].x;
        quad[3].y = quad[2].y;

        tc.Render2D(quad);

        GNC::GCS::GLDraw::PopMatrix();
        GNC::GCS::GLDraw::MatrixMode(GL_PROJECTION);
        GNC::GCS::GLDraw::PopMatrix();
        GNC::GCS::GLDraw::MatrixMode(GL_MODELVIEW);
}

inline void wDibujarBordeTexto2D(GNC::GCS::Contexto3D* c, GNC::GCS::TexturaCairo& tc, const GNC::GCS::Vector& rasterPos)
{
        /*
         * Posicion del Quad
         *  q3   q2
         *  q0   q1
         * */

        GNC::GCS::GLDraw::MatrixMode(GL_PROJECTION);
        GNC::GCS::GLDraw::PushMatrix();
        GNC::GCS::GLDraw::LoadIdentity();

        GNC::GCS::GLDraw::Ortho(0, c->ancho, 0, c->alto, -1, 1);
        GNC::GCS::GLDraw::MatrixMode(GL_MODELVIEW);
        GNC::GCS::GLDraw::PushMatrix();
        GNC::GCS::GLDraw::LoadIdentity();

        GNC::GCS::Vector quad[4];

        quad[0].x = rasterPos.x;
        quad[0].y = rasterPos.y;

        quad[2].x = quad[0].x + (double)tc.ancho;
        quad[2].y = quad[0].y - (double)tc.alto;

        quad[1].x = quad[2].x;
        quad[1].y = quad[0].y;

        quad[3].x = quad[0].x;
        quad[3].y = quad[2].y;

        GNC::GCS::GLDraw::Begin(GL_LINE_LOOP);
        GNC::GCS::GLDraw::Vertex2d(quad[0].x, quad[0].y);
        GNC::GCS::GLDraw::Vertex2d(quad[1].x, quad[1].y);
        GNC::GCS::GLDraw::Vertex2d(quad[2].x, quad[2].y);
        GNC::GCS::GLDraw::Vertex2d(quad[3].x, quad[3].y);
        GNC::GCS::GLDraw::End();

        GNC::GCS::GLDraw::PopMatrix();
        GNC::GCS::GLDraw::MatrixMode(GL_PROJECTION);
        GNC::GCS::GLDraw::PopMatrix();
        GNC::GCS::GLDraw::MatrixMode(GL_MODELVIEW);
}

inline void wDibujarFondoTexto2D(GNC::GCS::Contexto3D* c, GNC::GCS::TexturaCairo& tc, const GNC::GCS::Vector& rasterPos)
{
        /*
         * Posicion del Quad
         *  q3   q2
         *  q0   q1
         * */

        GNC::GCS::GLDraw::MatrixMode(GL_PROJECTION);
        GNC::GCS::GLDraw::PushMatrix();
        GNC::GCS::GLDraw::LoadIdentity();

        GNC::GCS::GLDraw::Ortho(0, c->ancho, 0, c->alto, -1, 1);
        GNC::GCS::GLDraw::MatrixMode(GL_MODELVIEW);
        GNC::GCS::GLDraw::PushMatrix();
        GNC::GCS::GLDraw::LoadIdentity();

        GNC::GCS::Vector quad[4];

        quad[0].x = rasterPos.x;
        quad[0].y = rasterPos.y;

        quad[2].x = quad[0].x + (double)tc.ancho;
        quad[2].y = quad[0].y - (double)tc.alto;

        quad[1].x = quad[2].x;
        quad[1].y = quad[0].y;

        quad[3].x = quad[0].x;
        quad[3].y = quad[2].y;

        GNC::GCS::GLDraw::Begin(GL_QUADS);
        for (int i = 0; i < 4; ++i) {
                GNC::GCS::GLDraw::Vertex2dv(quad[i].v);
        }
        GNC::GCS::GLDraw::End();

        GNC::GCS::GLDraw::PopMatrix();
        GNC::GCS::GLDraw::MatrixMode(GL_PROJECTION);
        GNC::GCS::GLDraw::PopMatrix();
        GNC::GCS::GLDraw::MatrixMode(GL_MODELVIEW);
}

inline void AjustarAPixelsPantalla(GNC::GCS::Contexto3D* c, GNC::GCS::Vector* v, int numVectores)
{
        if (numVectores < 1) {
                return;
        }
        GNC::GCS::Vector offsetCorreccion;
        double worldPos[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        double imagePos[4] = {v[0].x, v[0].y, 0.0f, 0.0f};

        c->GetRenderer()->m_pImageViewer->CoordenadasImagenACoordenadasMundo(imagePos, worldPos);

        double displayPos[3] = {0.0f, 0.0f, 0.0f};

        c->pRenderer->WorldToDisplay(worldPos, displayPos);
        offsetCorreccion.Asignar(displayPos[0], displayPos[1]);
        offsetCorreccion.Redondear();
        displayPos[0] = offsetCorreccion.x;
        displayPos[1] = offsetCorreccion.y;

        c->pRenderer->DisplayToWorld(displayPos, worldPos);
        c->GetRenderer()->m_pImageViewer->Proyect2D(worldPos, imagePos);

        offsetCorreccion.Asignar(imagePos[0], imagePos[1]);

        offsetCorreccion -= v[0];
        for (int i = 0; i < numVectores; i++) {
                v[i] += offsetCorreccion;
        }
}
