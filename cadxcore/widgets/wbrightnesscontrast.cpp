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


#include <cmath>

#include <api/globals.h>
#include "wbrightnesscontrast.h"

#include <api/controllers/ieventscontroller.h>
#include <eventos/modificacionimagen.h>
#include <main/managers/widgetsmanager.h>
#include <api/westilo.h>

#include <vtk_glew.h>
#include <vtk/gldraw.h>

namespace GLDraw = GNC::GCS::GLDraw;

//region "Implementacion de WBrightnessContrastBuilder"

GNC::GCS::Widgets::WBrightnessContrastBuilder::WBrightnessContrastBuilder(GNC::GCS::IWidgetsManager* pManager, const GNC::GCS::TriggerButton& buttonMask, long gid) : GNC::GCS::Widgets::IWidgetBuilder(pManager, buttonMask, gid)
{
        m_MouseDown = false;
        m_Changed = false;
}

GNC::GCS::Widgets::WBrightnessContrastBuilder::~WBrightnessContrastBuilder()
{
}

void GNC::GCS::Widgets::WBrightnessContrastBuilder::OnMouseEvents(GNC::GCS::Events::EventoRaton& evento)
{
        if (!m_pManager) {
                return;
        }
        if (m_MouseDown && evento.ButtonUp(m_ButtonMask)) {
                m_MouseDown = false;
                m_pManager->Modificado();
                if (m_Changed) {
                        ConsumirEvento();
                }
        } else if (evento.ButtonDown(m_ButtonMask)) {

                m_MouseDown = true;
                m_Changed = false;

                m_StartBCPoint.Asignar(m_pManager->GetRendererActivo()->m_pImageViewer->GetContrast(), m_pManager->GetRendererActivo()->m_pImageViewer->GetBrightness());
                m_Start.Asignar(evento.sX, evento.sY);
                m_Stop = m_Start;

                m_pManager->Modificado();
                ConsumirEvento();
        } else if (evento.Dragging() && m_MouseDown) {

                m_Stop.Asignar(evento.sX, evento.sY);
                if (m_Start.DistanciaEuclideaCuadrado(m_Stop) > TVector::EpsilonDeComponentes()) {
                        m_Changed = true;
                }

                AdjustBrightnessContrast(m_Stop - m_Start);

                m_pManager->Modificado();
                ConsumirEvento();
        } else if( evento.Moving() ) {
                m_NodoMoviendose.Asignar(evento.sX, evento.sY);
        }

}

void GNC::GCS::Widgets::WBrightnessContrastBuilder::OnKeyEvents(GNC::GCS::Events::EventoTeclado&)
{

}

void GNC::GCS::Widgets::WBrightnessContrastBuilder::Render(GNC::GCS::Contexto3D* /*c*/)
{
        if (!m_MouseDown) {
                return;
        }

        double viewport[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // { x, y, ancho, alto }. Convenio de coordenadas: {x, y} == {bottom, left}, {ancho, alto} == {top, right}
        GLDraw::GetDoublev(GL_VIEWPORT, viewport);

        GLDraw::PushAttrib(GL_ALL_ATTRIB_BITS);

        GLDraw::MatrixMode(GL_TEXTURE);
        GLDraw::PushMatrix();
        GLDraw::LoadIdentity();

        GLDraw::MatrixMode(GL_PROJECTION);
        GLDraw::PushMatrix();
        GLDraw::LoadIdentity();
        GLDraw::Ortho( 0, viewport[2] , viewport[3] , 0, -1, 1 );

        GLDraw::MatrixMode(GL_MODELVIEW);
        GLDraw::PushMatrix();
        GLDraw::LoadIdentity();

        {
                // Dibujado de la regla
                GLDraw::LineWidth(m_LineWidthProp);

                wAplicarColor(false, false, false);
                GLDraw::Begin(GL_LINES);
                GLDraw::Vertex2d(m_Start.x - 50.0f, m_Start.y);
                GLDraw::Vertex2d(m_Start.x + 50.0f, m_Start.y);
                GLDraw::Vertex2d(m_Start.x, m_Start.y - 50.0f);
                GLDraw::Vertex2d(m_Start.x, m_Start.y + 50.0f);
                GLDraw::End();

                wAplicarColor(true, true, false);
                GLDraw::LineWidth(m_LineWidthProp * 0.5f);
                GLDraw::Begin(GL_LINES);
                GLDraw::Vertex2d(m_Start.x, m_Start.y);
                GLDraw::Vertex2d(m_Stop.x, m_Start.y);
                GLDraw::Vertex2d(m_Start.x, m_Start.y);
                GLDraw::Vertex2d(m_Start.x, m_Stop.y);
                GLDraw::End();

                GLDraw::Enable(GL_LINE_STIPPLE);

                GLDraw::PointSize(WIDGET_SIZE_PUNTO * 0.5f);
                GLDraw::Begin(GL_POINTS);
                GLDraw::Vertex2d(m_Stop.x, m_Start.y);
                GLDraw::Vertex2d(m_Start.x, m_Stop.y);
                GLDraw::End();

                wAplicarColorSuave(true, false, false);
                GLDraw::LineStipple(3, 0xAAAA);
                GLDraw::Begin(GL_LINES);
                GLDraw::Vertex2dv(m_Start);
                GLDraw::Vertex2dv(m_Stop);
                GLDraw::End();
                GLDraw::Disable(GL_LINE_STIPPLE);
        }

        GLDraw::PopMatrix();

        GLDraw::MatrixMode(GL_PROJECTION);
        GLDraw::PopMatrix();

        GLDraw::MatrixMode(GL_TEXTURE);
        GLDraw::PopMatrix();

        GLDraw::MatrixMode(GL_MODELVIEW);

        GLDraw::PopAttrib();
}

GNC::GCS::Widgets::TipoCursor GNC::GCS::Widgets::WBrightnessContrastBuilder::GetCursor()
{
        return GNC::GCS::Widgets::CUR_WINDOW_LEVEL;
}

void GNC::GCS::Widgets::WBrightnessContrastBuilder::AdjustBrightnessContrast(const TVector& offset)
{

        TVector viewPort( TVector(m_pManager->GetRendererActivo()->m_c->ancho, m_pManager->GetRendererActivo()->m_c->alto));
        TVector rel = 4.0 * (offset/viewPort) * TVector(1.0, -1.0);

        m_pManager->GetRendererActivo()->m_pImageViewer->SetBrightness( (m_StartBCPoint + rel).y );
        m_pManager->GetRendererActivo()->m_pImageViewer->SetContrast( (m_StartBCPoint + rel).x );

        GNC::GCS::IEventsController::Instance()->ProcesarEvento(new GNC::GCS::Events::EventoModificacionImagen(m_pManager->GetVista(), GNC::GCS::Events::EventoModificacionImagen::AnotacionesEstaticasModificadas));

}

//endregion
