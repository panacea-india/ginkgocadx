
/*
*
*  $Id: vtkopenglginkgotexture.cpp $
*  Ginkgo CADx Project
*
*  Code adapted from VTK
*
*
=========================================================================

Program:   Visualization Toolkit
Module:    $RCSfile: vtkGinkgoOpenGLTexture.cxx,v $

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
All rights reserved.
See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#if defined max
#undef max
#endif
#if defined min
#undef min
#endif

#include <cmath>
#include <limits>

// GLEW must come before any other OpenGL header
#include <vtk_glew.h>

#if defined(__DEPRECATED)
#undef __DEPRECATED
#endif
#include "vtkopenglginkgotexture.h"

#include <vtkHomogeneousTransform.h>
#include <vtkImageData.h>
#include <vtkUnsignedCharArray.h>
#include <vtkUnsignedShortArray.h>
#include <vtkUnsignedIntArray.h>
#include <vtkFloatArray.h>
#include <vtkDoubleArray.h>
#include <vtkLookupTable.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLRenderer.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkOpenGLRenderWindow.h>
#include <vtkTransform.h>
#include <vtkPixelBufferObject.h>
#include <vtkCellArray.h>
#include <vtkPolyData.h>
#include <vtkOpenGLBufferObject.h>
#include <vtkOpenGLShaderCache.h>
#include <vtkOpenGLState.h>
#include <vtkOpenGLVertexArrayObject.h>
#include <vtkShaderProgram.h>
#include <vtkTextureUnitManager.h>

#include <algorithm>
#include <vector>

#include <main/controllers/controladorlog.h>
#include <main/controllers/controladorpermisos.h>

#ifndef VTK_IMPLEMENT_MESA_CXX
vtkStandardNewMacro(vtkGinkgoOpenGLTexture);
#endif

#define ADAPTATIVE_BRIGHNESS_CONTRAST

inline char clampByte(int color)
{
        if( color & (~255) ) {
                color = (char)((-(color)) >> 31);
        } else {
                color = (char)(color);
        }
        return color;
}

// integer implementation (fast but incomplete)
inline char mixFixedPoint( char x, char y, char a)
{
        unsigned int alpha = a;
        unsigned int inv_alpha = 255 - a;
        return clampByte( (alpha * x + inv_alpha * y) >> 8);
}

/** Interpolate the normalized values between "x" and "y" with factor "1-a, a" (slow) **/
inline float mixNormalized(float x, float y, float a)
{
        return x * (1.0f - a) + (y * a);
}

inline float clamp(float min, float max, float val)
{
        if (val > max) {
                return max;
        } else {
                if (val < min) {
                        return min;
                } else {
                        return val;
                }
        }
}

// ----------------------------------------------------------------------------
// Initializes an instance, generates a unique index.
vtkGinkgoOpenGLTexture::vtkGinkgoOpenGLTexture()
{
        this->Index = 0;
        this->RenderWindow = 0;
        this->CheckedHardwareSupport=false;
        this->SupportsNonPowerOfTwoTextures=false;
        this->SupportsPBO=false;
        this->SupportsVertexShaders=false;
        this->SupportsFragmentShaders=false;
        this->SupportsMultiTexture=false;
        this->PBO=0;
        this->VAO=NULL;
        this->VBO=NULL;
        this->LUTIndex=0;
        this->UseShader=false;
        this->InternalEnableShaders = true;
        this->zsize=0;
        this->TIndex=0;
        this->TIndexChanged=false;
        this->LookupTableChanged = false;
        this->RGBImage = false;
        this->ResetBrightnessAndContrast();
}

void vtkGinkgoOpenGLTexture::SetBrightness(float brightness)
{
#if defined(ADAPTATIVE_BRIGHNESS_CONTRAST)
        this->Brightness = brightness;
#else
        this->Brightness = clamp(-1.0f, 1.0f, brightness);
#endif

        this->LookupTableChanged = true;
}

void vtkGinkgoOpenGLTexture::SetContrast(float contrast)
{
#if defined(ADAPTATIVE_BRIGHNESS_CONTRAST)
        this->Contrast = contrast;
#else
        this->Contrast = clamp(0.0f, 2.0f, contrast);
#endif

        this->LookupTableChanged = true;
}

float vtkGinkgoOpenGLTexture::GetBrightness()
{
        return this->Brightness;
}

float vtkGinkgoOpenGLTexture::GetContrast()
{
        return this->Contrast;
}

void vtkGinkgoOpenGLTexture::ResetBrightnessAndContrast()
{

#if defined(ADAPTATIVE_BRIGHNESS_CONTRAST)
        this->Brightness = 1.0f;
#else
        this->Brightness = 0.0f;
#endif
        this->Contrast = 1.0f;
        this->LookupTableChanged = true;
}

// ----------------------------------------------------------------------------
vtkGinkgoOpenGLTexture::~vtkGinkgoOpenGLTexture()
{
        if (this->RenderWindow) {
                this->ReleaseGraphicsResources(this->RenderWindow);
        }
        if(this->PBO!=0) {
                vtkErrorMacro(<< "PBO should have been deleted in ReleaseGraphicsResources()");
        }
        this->RenderWindow = NULL;
}

// ----------------------------------------------------------------------------
void vtkGinkgoOpenGLTexture::Initialize(vtkRenderer * vtkNotUsed(ren))
{
}

void vtkGinkgoOpenGLTexture::SetLookupTable(vtkScalarsToColors* table)
{
        vtkTexture::SetLookupTable(table);
        this->LookupTableChanged = true;

}

// ----------------------------------------------------------------------------
// Release the graphics resources used by this texture.
void vtkGinkgoOpenGLTexture::ReleaseGraphicsResources(vtkWindow *renWin)
{
        const bool contextAvailable = renWin && renWin->GetMapped();
        if (contextAvailable && (this->Index || this->LUTIndex || this->VAO)) {
                static_cast<vtkRenderWindow *>(renWin)->MakeCurrent();
                if (this->Index && glIsTexture(static_cast<GLuint>(this->Index))) {
                        GLuint tempIndex = this->Index;
                        glDeleteTextures(1, &tempIndex);
                }
                if (this->LUTIndex && glIsTexture(this->LUTIndex)) {
                        glDeleteTextures(1, &this->LUTIndex);
                }
                if (this->VAO) {
                        this->VAO->ReleaseGraphicsResources();
                        this->VBO->ReleaseGraphicsResources();
                }
        }
        if (this->VAO) {
                this->VAO->Delete();
                this->VAO = NULL;
                this->VBO->Delete();
                this->VBO = NULL;
        }
        this->Index = 0;
        this->LUTIndex = 0;
        this->RenderWindow = NULL;
        this->CheckedHardwareSupport=false;
        this->SupportsNonPowerOfTwoTextures=false;
        this->SupportsPBO=false;
        if(this->PBO!=0) {
                this->PBO->Delete();
                this->PBO=0;
        }
        this->Modified();
}


// Image shaders. They follow VTK's conventions so vtkOpenGLShaderCache adds
// the GLSL version line and fragment output for the context in use.
static const char* ImageVertexShader =
        "//VTK::System::Dec\n"
        "in vec3 aPos;\n"
        "in vec2 aTex;\n"
        "uniform mat4 uMVP;\n"
        "out vec2 vTex;\n"
        "void main()\n"
        "{\n"
        "  gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "  vTex = aTex;\n"
        "}\n";

// uMode 0: texture as is; 1: grey value through the lookup table;
// 2: RGB with brightness and contrast ("The OpenGL Shading Language" C.19)
static const char* ImageFragmentShader =
        "//VTK::System::Dec\n"
        "//VTK::Output::Dec\n"
        "in vec2 vTex;\n"
        "uniform sampler2D uImage;\n"
        "uniform sampler1D uLookupTable;\n"
        "uniform int uMode;\n"
        "uniform float uLutShift;\n"
        "uniform float uLutScale;\n"
        "uniform float uBrightness;\n"
        "uniform float uContrast;\n"
        "uniform float uOpacity;\n"
        "void main()\n"
        "{\n"
        "  vec4 color = texture(uImage, vTex);\n"
        "  if (uMode == 1) {\n"
        "    float lutColor = clamp((color.r + uLutShift) * uLutScale, 0.0, 1.0);\n"
        "    color = texture(uLookupTable, lutColor);\n"
        "  } else if (uMode == 2) {\n"
        "    vec4 averageLuminance = vec4(0.5, 0.5, 0.5, 1.0);\n"
        "    color = mix(color * uBrightness, mix(averageLuminance, color, uContrast), 0.5);\n"
        "  }\n"
        "  color *= uOpacity;\n"
        "  // fully transparent texels must not write depth\n"
        "  if (!(color.a > 0.0)) discard;\n"
        "  gl_FragData[0] = color;\n"
        "}\n";


// ----------------------------------------------------------------------------
// Implement base class method.
void vtkGinkgoOpenGLTexture::Load(vtkRenderer *ren)
{
        vtkImageData *input = NULL;

        GNC::GCS::ILocker lock(this);
        {
                input = this->GetInput();

                GLenum TexFormat         = GL_RED;
                GLenum TexInternalFormat = GL_R8;
                GLenum TexType           = GL_UNSIGNED_BYTE;


                this->Initialize(ren);

                // Need to reload the texture.
                // There used to be a check on the render window's mtime, but
                // this is too broad of a check (e.g. it would cause all textures
                // to load when only the desired update rate changed).
                // If a better check is required, check something more specific,
                // like the graphics context.
                vtkOpenGLRenderWindow* renWin = static_cast<vtkOpenGLRenderWindow*>(ren->GetRenderWindow());


                bool ImageDataChanged =
                        ( this->GetMTime() > this->LoadTime.GetMTime() )
                        || ( input->GetMTime() > this->LoadTime.GetMTime() )
                        || ( !UseShader && this->GetLookupTable() && (this->GetLookupTable()->GetMTime () > this->LoadTime.GetMTime()) )
                        || ( !UseShader && LookupTableChanged )
                        || ( renWin != this->RenderWindow.GetPointer() )
                        || ( renWin->GetContextCreationTime() > this->LoadTime );

                if (TIndexChanged || ImageDataChanged ) {
                        int bytesPerPixel;
                        int size[3];
                        vtkDataArray *scalars = NULL;
                        unsigned char* vptr = NULL;
                        unsigned char *resultData=NULL;
                        int xsize, ysize;
                        unsigned int xs,ys;
                        GLuint tempIndex=0;

                        unsigned long off = 0;
                        unsigned char* pNPOTPixelData = NULL;
                        unsigned char* pResampledPixelData = NULL;

                        // Get the scalars the user choose to color with.
                        scalars = this->GetInputArrayToProcess(0, input);

                        // make sure scalars are non null
                        if (!input || !scalars) {
                                LOG_ERROR("OpenGLComponent", "NULL INPUT");
                                return;
                        }

                        // get some info
                        input->GetDimensions(size);

                        int e[6] = {0, 0, 0, 0, 0, 0};
                        input->GetExtent(e);

                        //std::cout << "Update extent: [ " << e[0] << ", " << e[1] << ", " << e[2] << ", " << e[3] << ", " << e[4] << ", " << e[5] << " ] " << std::endl;

                        if (input->GetNumberOfCells() == scalars->GetNumberOfTuples()) {
                                // we are using cell scalars. Adjust image size for cells.
                                for (int kk=0; kk < 3; kk++) {
                                        if (size[kk]>1) {
                                                size[kk]--;
                                        }
                                }
                        }

                        bytesPerPixel = scalars->GetNumberOfComponents();

                        xsize = size[0];
                        ysize = size[1];
                        zsize = size[2];

                        if (xsize <= 0 || ysize <= 0 || zsize <= 0) {
                                LOG_ERROR("OpenGLComponent", "Wrong size: " << xsize << "x" << ysize << "x" << zsize);
                                return;
                        }
                        ///CHECK HARDWARE SUPPORT
                        if(!this->CheckedHardwareSupport) {
                                // VTK 9 needs OpenGL 3.2, which includes everything checked
                                // for here (non-power-of-two textures, multitexture, GLSL).
                                this->CheckedHardwareSupport = true;
                                this->SupportsNonPowerOfTwoTextures = true;
                                this->SupportsPBO=vtkPixelBufferObject::IsSupported(renWin);

                                //ENABLE/DISABLE SHADER!
                                if (this->InternalEnableShaders && GNC::GCS::IControladorPermisos::Instance()->Get("core.opengl", "enable_shaders").Activo()) {
                                        this->UseShader = true;
                                }
                        }

                        if (!UseShader) {
                                LookupTableChanged = false; // Reset LUT changed state
                        }

                        TIndex = std::max<int>(0, std::min<int>(TIndex, zsize - 1));
                        TIndexChanged = false;
                        off = size[0] * size[1] * TIndex * bytesPerPixel;
                        //LOG_DEBUG("OpenGLComponent", "Fijado indice " << TIndex);

                        // make sure using unsigned char data of color scalars type
                        // IF MAP SCALARS
                        if ( (this->GetColorMode() == VTK_COLOR_MODE_MAP_SCALARS) || (scalars->GetDataType() != VTK_UNSIGNED_CHAR) ) {
                                const unsigned int wdh = size[0] * size[1];
                                const unsigned int numComponents = scalars->GetNumberOfComponents();

                                this->RGBImage = false;

                                //IF THERE IS ONLY ONE COMPONENT COULD USE SHADERS...
                                if (numComponents == 1) {

                                        if (UseShader) {
                                                double range[2] = {0.0, 1.0};
                                                scalars->GetRange(range);

                                                TIndex = std::max<int>(0, std::min<int>(TIndex, zsize - 1));
                                                TIndexChanged = false;

                                                LOG_DEBUG("OpenGLComponent", "Tipo de componente: " << scalars->GetDataTypeAsString());
                                                if (scalars->GetDataType() == VTK_UNSIGNED_SHORT || scalars->GetDataType() == VTK_SHORT) {
                                                        //RESCALE TO UNSIGNED SHORT
                                                        bytesPerPixel         = 2;
                                                        TexInternalFormat = GL_R16;
                                                        TexType           = GL_UNSIGNED_SHORT;

                                                        unsigned short* resamplePixelData = new unsigned short[wdh];

                                                        const unsigned int psize = wdh;
                                                        double diff = range[1] - range[0];

                                                        if (diff < std::numeric_limits<double>::epsilon() ) {
                                                                range[0] = (scalars->GetDataType() == VTK_UNSIGNED_SHORT)?((double) std::numeric_limits<unsigned short>::min()):((double) std::numeric_limits<short>::min());
                                                                range[1] = (scalars->GetDataType() == VTK_UNSIGNED_SHORT)?((double) std::numeric_limits<unsigned short>::max()):((double) std::numeric_limits<short>::max());
                                                                diff = range[1] - range[0];
                                                        }

                                                        off = size[0] * size[1] * TIndex * bytesPerPixel;
                                                        unsigned char* dptr = (unsigned char*) scalars->GetVoidPointer(0);
                                                        if (dptr == NULL) {
                                                                delete[] resamplePixelData;
                                                                LOG_ERROR("OpenGLComponent", "Could not get pixel data pointer");
                                                                return;
                                                        }
                                                        vptr = (unsigned char*) (dptr + off);

                                                        double scale = 1 / diff;
                                                        unsigned short* dst = resamplePixelData;
                                                        short* src = (short*)vptr;

                                                        // Verificaciones extra para VIX. El flujo permanece igual.
                                                        unsigned short v;
                                                        long long maxVal1 = - ( (long long) std::numeric_limits<unsigned short>::min());
                                                        long long minVal1 = ( (long long) std::numeric_limits<unsigned short>::max());

                                                        long long maxVal2 = - ( (long long) std::numeric_limits<unsigned short>::min());
                                                        long long minVal2 = ( (long long) std::numeric_limits<unsigned short>::max());

                                                        for (unsigned int i = 0; i < psize; i++) {
                                                                if (scalars->GetDataType() == VTK_UNSIGNED_SHORT) {
                                                                        const unsigned short v1 = *((unsigned short*) src++);
                                                                        minVal1 = std::min(minVal1, (long long)v1);
                                                                        maxVal1 = std::max(maxVal1, (long long)v1);
                                                                        v = (unsigned short)(( scale * ((double)v1 - range[0]) ) * std::numeric_limits<unsigned short>::max());
                                                                        minVal2 = std::min(minVal2, (long long)v);
                                                                        maxVal2 = std::max(maxVal2, (long long)v);
                                                                } else {
                                                                        const signed short v1 = *((signed short*) src++);
                                                                        minVal1 = std::min(minVal1, (long long)v1);
                                                                        maxVal1 = std::max(maxVal1, (long long)v1);
                                                                        v = (unsigned short)(( scale * ((double)v1 - range[0]) ) * std::numeric_limits<unsigned short>::max());
                                                                        minVal2 = std::min(minVal2, (long long)v);
                                                                        maxVal2 = std::max(maxVal2, (long long)v);
                                                                }
                                                                *(dst++) =  v;
                                                        }
                                                        pResampledPixelData = (unsigned char*) resamplePixelData;
                                                        vptr = pResampledPixelData;
                                                } else if (scalars->GetDataType() == VTK_UNSIGNED_CHAR || scalars->GetDataType() == VTK_CHAR) {
                                                        //rescale to unsigned byte
                                                        bytesPerPixel         = 1;
                                                        TexInternalFormat = GL_R8;
                                                        TexType = GL_UNSIGNED_BYTE;

                                                        unsigned char* resamplePixelData = new unsigned char[wdh];

                                                        const unsigned int psize = wdh;
                                                        double diff = range[1] - range[0];

                                                        if (diff < std::numeric_limits<double>::epsilon() ) {
                                                                range[0] = (scalars->GetDataType() == VTK_UNSIGNED_CHAR)?((double) std::numeric_limits<unsigned char>::min()):((double) std::numeric_limits<char>::min());
                                                                range[1] = (scalars->GetDataType() == VTK_UNSIGNED_CHAR)?((double) std::numeric_limits<unsigned char>::max()):((double) std::numeric_limits<char>::max());
                                                                diff = range[1] - range[0];
                                                        }

                                                        off = size[0] * size[1] * TIndex * bytesPerPixel;
                                                        unsigned char* dptr = (unsigned char*) scalars->GetVoidPointer(0);
                                                        if (dptr == NULL) {
                                                                delete[] resamplePixelData;
                                                                LOG_ERROR("OpenGLComponent", "Could not get pixel data pointer");
                                                                return;
                                                        }
                                                        vptr = (unsigned char*) (dptr + off);

                                                        double scale = 1 / diff;
                                                        unsigned char* dst = resamplePixelData;
                                                        char* src = (char*)vptr;

                                                        unsigned char v;
                                                        for (unsigned int i = 0; i < psize; i++) {
                                                                if (scalars->GetDataType() == VTK_UNSIGNED_CHAR) {
                                                                        const unsigned char v1 = *((unsigned char*)src);
                                                                        v = (unsigned char)(( scale * ( v1 - range[0]) )*std::numeric_limits<unsigned char>::max());
                                                                        src++;
                                                                } else {
                                                                        v = (unsigned char)(( scale * ((char) *(src++) - range[0]) )*std::numeric_limits<unsigned char>::max());
                                                                }
                                                                *(dst++) =  v;
                                                        }
                                                        pResampledPixelData = (unsigned char*) resamplePixelData;
                                                        vptr = pResampledPixelData;
                                                } else {
                                                        LOG_DEBUG("OpenGLComponent", "Shaders don't support this component type: Wrong size: " << scalars->GetDataTypeAsString());
                                                        this->UseShader = false;
                                                }
                                        }
                                        //if support shaders but scalar type isn't short or char, vtk makes the mapping
                                        if (!this->UseShader) {
                                                //optimization, if we call mapscalars to colors with all scalars multiframe performance will be very bad
                                                if (zsize > 1) {
                                                        vtkDataArray* myScalars = vtkDataArray::CreateDataArray(scalars->GetDataType());
                                                        myScalars->SetNumberOfComponents(scalars->GetNumberOfComponents());
                                                        myScalars->SetNumberOfTuples(wdh);
                                                        myScalars->SetVoidArray((scalars->GetVoidPointer(wdh * TIndex)), wdh, 1);
                                                        myScalars->SetLookupTable(scalars->GetLookupTable());
                                                        vptr = this->MapScalarsToColors (myScalars);
                                                        myScalars->Delete();
                                                } else {
                                                        vptr = this->MapScalarsToColors (scalars);
                                                }
                                                if (vptr == NULL) {
                                                        LOG_DEBUG("OpenGLComponent", "Could not map pixel data");
                                                        return;
                                                }
                                                bytesPerPixel = 4;
                                        }
                                }//end 1 component
                                else {
                                        //lookup table with more than one component... extrange
                                        this->UseShader = false;
                                        pResampledPixelData = new unsigned char[wdh * 4];
                                        const unsigned int psize = wdh;
                                        double range[2] = {0.0, 1.0};
                                        scalars->GetRange(range);
                                        double diff = range[1] - range[0];

                                        vptr = (unsigned char*) scalars->GetVoidPointer(0);
                                        unsigned char *dst = pResampledPixelData;

                                        switch (scalars->GetDataType()) {
                                        case VTK_CHAR: {
                                                if (diff < std::numeric_limits<double>::epsilon() ) {
                                                        range[0] = (double) std::numeric_limits<char>::min();
                                                        range[1] = (double) std::numeric_limits<char>::max();
                                                        diff = range[1] - range[0];
                                                }

                                                double scale = 255.0 / diff;

                                                char *src = ((char*) vptr) + (wdh * TIndex * numComponents);

                                                for (unsigned int i = 0; i < psize; i++) {
                                                        for (unsigned int j = 0; j < 4; j++) {
                                                                if (j < numComponents) {
                                                                        const unsigned char v = (unsigned char) ( scale * ((double) *(src++)) );
                                                                        *(dst++) =  v;
                                                                } else {
                                                                        *(dst++) = 255;
                                                                }
                                                        }
                                                }

                                        }
                                        break;
                                        case VTK_UNSIGNED_CHAR: {
                                                unsigned char *src = ((unsigned char*) vptr) + (wdh * TIndex * numComponents);

                                                for (unsigned int i = 0; i < psize; i++) {
                                                        for (unsigned int j = 0; j < 4; j++) {
                                                                if (j < numComponents) {
                                                                        const unsigned char v = *(src++);
                                                                        *(dst++) =  v;
                                                                } else {
                                                                        *(dst++) = 255;
                                                                }
                                                        }
                                                }

                                        }
                                        break;
                                        case VTK_SHORT: {

                                                if (diff < std::numeric_limits<double>::epsilon() ) {
                                                        range[0] = (double) std::numeric_limits<short>::min();
                                                        range[1] = (double) std::numeric_limits<short>::max();
                                                        diff = range[1] - range[0];
                                                }

                                                short *src = ((short*) vptr) + (wdh * TIndex * numComponents);
                                                double scale = 255.0 / diff;

                                                for (unsigned int i = 0; i < psize; i++) {
                                                        for (unsigned int j = 0; j < 4; j++) {
                                                                if (j < numComponents) {
                                                                        const unsigned char v = (unsigned char) ( scale * ((double) *(src++)) );
                                                                        *(dst++) =  v;
                                                                } else {
                                                                        *(dst++) = 255;
                                                                }
                                                        }
                                                }
                                        }
                                        break;
                                        case VTK_UNSIGNED_SHORT: {
                                                if (diff < std::numeric_limits<double>::epsilon() ) {
                                                        range[0] = (double) std::numeric_limits<unsigned short>::min();
                                                        range[1] = (double) std::numeric_limits<unsigned short>::max();
                                                        diff = range[1] - range[0];
                                                }

                                                unsigned short *src = ((unsigned short*) vptr) + (wdh * TIndex * numComponents);
                                                double scale = 255.0 / diff;

                                                for (unsigned int i = 0; i < psize; i++) {
                                                        for (unsigned int j = 0; j < 4; j++) {
                                                                if (j < numComponents) {
                                                                        const unsigned char v = (unsigned char) ( scale * ((double) *(src++)) );
                                                                        *(dst++) =  v;
                                                                } else {
                                                                        *(dst++) = 255;
                                                                }
                                                        }
                                                }
                                        }
                                        break;

                                        case VTK_INT: {
                                                if (diff < std::numeric_limits<double>::epsilon() ) {
                                                        range[0] = (double) std::numeric_limits<int>::min();
                                                        range[1] = (double) std::numeric_limits<int>::max();
                                                        diff = range[1] - range[0];
                                                }

                                                int *src = ((int*) vptr) + (wdh * TIndex * numComponents);
                                                double scale = 255.0 / diff;

                                                for (unsigned int i = 0; i < psize; i++) {
                                                        for (unsigned int j = 0; j < 4; j++) {
                                                                if (j < numComponents) {
                                                                        const unsigned char v = (unsigned char) ( scale * ((double) *(src++)) );
                                                                        *(dst++) =  v;
                                                                } else {
                                                                        *(dst++) = 255;
                                                                }
                                                        }
                                                }
                                        }
                                        break;
                                        case VTK_UNSIGNED_INT: {
                                                if (diff < std::numeric_limits<double>::epsilon() ) {
                                                        range[0] = (double) std::numeric_limits<unsigned int>::min();
                                                        range[1] = (double) std::numeric_limits<unsigned int>::max();
                                                        diff = range[1] - range[0];
                                                }

                                                unsigned int *src = ((unsigned int*) vptr) + (wdh * TIndex * numComponents);
                                                double scale = 255.0 / diff;

                                                for (unsigned int i = 0; i < psize; i++) {
                                                        for (unsigned int j = 0; j < 4; j++) {
                                                                if (j < numComponents) {
                                                                        const unsigned char v = (unsigned char) ( scale * ((double) *(src++)) );
                                                                        *(dst++) =  v;
                                                                } else {
                                                                        *(dst++) = 255;
                                                                }
                                                        }
                                                }
                                        }
                                        break;
                                        case VTK_FLOAT: {
                                                if (diff < std::numeric_limits<double>::epsilon() ) {
                                                        range[0] = (double) std::numeric_limits<float>::min();
                                                        range[1] = (double) std::numeric_limits<float>::max();
                                                        diff = range[1] - range[0];
                                                }

                                                float *src = ((float*) vptr) + (wdh * TIndex * numComponents);
                                                double scale = 255.0 / diff;

                                                for (unsigned int i = 0; i < psize; i++) {
                                                        for (unsigned int j = 0; j < 4; j++) {
                                                                if (j < numComponents) {
                                                                        const unsigned char v = (unsigned char) ( scale * ((double) *(src++)) );
                                                                        *(dst++) =  v;
                                                                } else {
                                                                        *(dst++) = 255;
                                                                }
                                                        }
                                                }
                                        }
                                        break;
                                        case VTK_DOUBLE: {
                                                if (diff < std::numeric_limits<double>::epsilon() ) {
                                                        range[0] = (double) std::numeric_limits<double>::min();
                                                        range[1] = (double) std::numeric_limits<double>::max();
                                                        diff = range[1] - range[0];
                                                }

                                                double *src = ((double*) vptr) + (wdh * TIndex * numComponents);
                                                double scale = 255.0 / diff;

                                                for (unsigned int i = 0; i < psize; i++) {
                                                        for (unsigned int j = 0; j < 4; j++) {
                                                                if (j < numComponents) {
                                                                        const unsigned char v = (unsigned char) ( scale * ((double) *(src++)) );
                                                                        *(dst++) =  v;
                                                                } else {
                                                                        *(dst++) = 255;
                                                                }
                                                        }
                                                }
                                        }
                                        break;
                                        default:
                                                delete[] pResampledPixelData;
                                                LOG_ERROR("OpenGLComponent", "Pixel data not supported");
                                                return;
                                        }

                                        vptr = pResampledPixelData;
                                        bytesPerPixel = 4;
                                }//end more than one component...
                        } else {
                                if ( (scalars->GetDataType() == VTK_UNSIGNED_CHAR) ) {
                                        this->RGBImage = true;
                                } else {
                                        this->RGBImage = false;
                                        this->UseShader = false;
                                }

                                //RGB IMAGES
                                unsigned char *s_array = static_cast<vtkUnsignedCharArray *>(scalars)->GetPointer(0);
                                if (s_array != NULL) {
                                        int bsize = bytesPerPixel * size[0] * size[1];

                                        unsigned char *src = s_array + off;
                                        unsigned char *dst = vptr = pResampledPixelData = new unsigned char[bsize];

#if defined (ADAPTATIVE_BRIGHNESS_CONTRAST)
                                        for (int i = 0; i < bsize; i++) {
                                                const float colorIn = (float) ( *(src++) ) / 255.0f;
                                                const float clampedResult = clamp(0.0f, 1.0f, mixNormalized( colorIn * Brightness, mixNormalized(0.5f, colorIn, Contrast), 0.5f ));
                                                const char colorOut = (char) ( clamp(0.0f, 255.0f, 255.0f * clampedResult) );
                                                *(dst++) = colorOut;
                                        }
#else
                                        std::cout << "---\nBrightness = " << Brightness << std::endl;
                                        std::cout << "Contrast =  " << Contrast << std::endl;

                                        for (int i = 0; i < bsize; i++) {
                                                const float colorIn = (float) ( *(src++) ) / 255.0f;
                                                const float clampedResult = clamp(0.0f, 1.0f, ((colorIn - 0.5f) * std::max<float>(Contrast, 0.0f)) + 0.5f + Brightness );
                                                const char colorOut = (char) ( clamp(0.0f, 255.0f, 255.0f * clampedResult) );
                                                *(dst++) = colorOut;
                                        }
#endif
                                } else {
                                        LOG_DEBUG("OpenGLComponent", "Could not get pixel data pointer");
                                        return;
                                }
                        }

                        //if no use pixel shader...
                        if (!this->UseShader || RGBImage) {
                                // sized formats: core profiles reject a bare component count
                                static const GLenum sizedFormats[4] = { GL_R8, GL_RG8, GL_RGB8, GL_RGBA8 };
                                TexInternalFormat = sizedFormats[std::max(1, std::min(4, static_cast<int>(bytesPerPixel))) - 1];
                                TexType = GL_UNSIGNED_BYTE;
                                switch (bytesPerPixel) {
                                case 1:
                                        TexFormat = GL_RED;
                                        break;
                                case 2:
                                        TexFormat = GL_RG;
                                        break;
                                case 3:
                                        TexFormat = GL_RGB;
                                        break;
                                case 4:
                                        TexFormat = GL_RGBA;
                                        break;
                                }
                                // if we are using OpenGL 1.1, you can force 32 or16 bit textures
#ifdef GL_VERSION_1_1
                                if (this->Quality == VTK_TEXTURE_QUALITY_32BIT) {
                                        switch (bytesPerPixel) {
                                        case 1:
                                                TexInternalFormat = GL_R8;
                                                break;
                                        case 2:
                                                TexInternalFormat = GL_RG8;
                                                break;
                                        case 3:
                                                TexInternalFormat = GL_RGB8;
                                                break;
                                        case 4:
                                                TexInternalFormat = GL_RGBA8;
                                                break;
                                        }
                                } else if (this->Quality == VTK_TEXTURE_QUALITY_16BIT) {
                                        switch (bytesPerPixel) {
                                        case 1:
                                                TexInternalFormat = GL_R8;
                                                break;
                                        case 2:
                                                TexInternalFormat = GL_RG8;
                                                break;
                                        case 3:
                                                TexInternalFormat = GL_RGB4;
                                                break;
                                        case 4:
                                                TexInternalFormat = GL_RGBA4;
                                                break;
                                        }
                                }
#endif
                        }

                        // -- decide whether the texture needs to be resampled --

                        GLint maxDimGL;
                        glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxDimGL);
                        // if larger than permitted by the graphics library then must resample
                        bool resampleNeeded = xsize > maxDimGL || ysize > maxDimGL;
                        if(resampleNeeded) {
                                vtkDebugMacro( "Texture too big for gl, maximum is " << maxDimGL);
                        }

                        if(!resampleNeeded && !this->SupportsNonPowerOfTwoTextures) {
                                // xsize and ysize must be a power of 2 in OpenGL
                                xs = static_cast<unsigned int>(xsize);
                                ys = static_cast<unsigned int>(ysize);
                                while (!(xs & 0x01)) {
                                        xs = xs >> 1;
                                }
                                while (!(ys & 0x01)) {
                                        ys = ys >> 1;
                                }
                                // if not a power of two then resampling is required
                                resampleNeeded= (xs>1) || (ys>1);
                        }

                        if(resampleNeeded) {
                                vtkDebugMacro(<< "Resampling texture to power of two for OpenGL");
                                resultData = this->ResampleToPowerOfTwo(xsize, ysize, vptr, bytesPerPixel);
                                pNPOTPixelData = resultData;

                        }

                        if ( resultData == NULL ) {
                                resultData = vptr;
                        }

                        // free any old display lists (from the old context)
                        if (this->RenderWindow) {
                                this->ReleaseGraphicsResources(this->RenderWindow);
                        }
                        this->RenderWindow = ren->GetRenderWindow();
                        if (this->RenderWindow == NULL) {
                                if (pResampledPixelData) {
                                        delete[] pResampledPixelData;
                                }
                                if (pNPOTPixelData) {
                                        delete[] pNPOTPixelData;
                                }
                                return;
                        }

                        // make the new context current before we mess with opengl
                        this->RenderWindow->MakeCurrent();

                        // define a display list for this texture
                        // get a unique display list id

                        glGenTextures(1, &tempIndex);
                        this->Index = static_cast<long>(tempIndex);
                        glBindTexture(GL_TEXTURE_2D, this->Index);
                        //pixel interpolate
                        if (this->GetInterpolate()) {
                                glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                                 GL_LINEAR);
                                glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                                 GL_LINEAR );
                        } else {
                                glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
                                glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
                        }
                        /////repeat texture??
                        if (this->GetRepeat()) {
                                glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_REPEAT );
                                glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_REPEAT );
                        } else {
                                if (this->GetEdgeClamp()) {
                                        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                                                         GL_CLAMP_TO_EDGE );
                                        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                                                         GL_CLAMP_TO_EDGE );
                                } else {
                                        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE );
                                        glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_EDGE );
                                }
                        }


                        //UPLOAD TEXTURE...
                        if(this->SupportsPBO) {
                                if(this->PBO==0) {
                                        this->PBO=vtkPixelBufferObject::New();
                                        this->PBO->SetContext(renWin);
                                }
                                //std::cout << ">> UpdatePBO" << std::endl;
                                unsigned int dims[2];
                                vtkIdType increments[2];
                                dims[0] = static_cast<unsigned int>(xsize);
                                dims[1] = static_cast<unsigned int>(ysize);
                                increments[0] = 0;
                                increments[1] = 0;

                                if (RGBImage) {
                                        this->PBO->Upload2D(VTK_UNSIGNED_CHAR, resultData, dims, bytesPerPixel, increments);
                                } else {
                                        if (UseShader) {
                                                this->PBO->Upload2D(scalars->GetDataType(), resultData, dims, scalars->GetNumberOfComponents(), increments);
                                        } else {
                                                this->PBO->Upload2D(VTK_UNSIGNED_CHAR, resultData, dims, bytesPerPixel, increments);
                                        }
                                }
                                // non-blocking call
                                this->PBO->Bind(vtkPixelBufferObject::UNPACKED_BUFFER);
                                glTexImage2D( GL_TEXTURE_2D, 0 , TexInternalFormat, xsize, ysize, 0, TexFormat, TexType, 0);
                                this->PBO->UnBind();
                                //std::cout << "<< UpdatePBO" << std::endl;
                        } else {
                                //std::cout << ">> UpdateText(slow). ResultData = " << (void*)resultData << std::endl;
                                // blocking call
                                glTexImage2D( GL_TEXTURE_2D, 0 , TexInternalFormat, xsize, ysize, 0, TexFormat, TexType, resultData);
                                //std::cout << "<< UpdateText(slow)" << std::endl;

                        }

                        // Grey images are stored as RED (or RED+GREEN for grey+alpha)
                        // since core profiles have no LUMINANCE formats. Swizzle them
                        // back so anything sampling this texture (the magnifier too)
                        // still sees grey.
                        if (TexFormat == GL_RED || TexFormat == GL_RG) {
                                const GLint swizzle[4] = { GL_RED, GL_RED, GL_RED, (TexFormat == GL_RG) ? GL_GREEN : GL_ONE };
                                glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
                        }

                        // modify the load time to the current time
                        this->LoadTime.Modified();

                        // free memory
                        if (pResampledPixelData) {
                                delete[] pResampledPixelData;
                        }
                        if (pNPOTPixelData) {
                                delete[] pNPOTPixelData;
                        }
                }// end if changes...

                //UPLOAD LOOKUP TABLE IF NEEDED
                bool LUT_NeedsToBeInitialized = !RGBImage && this->LUTIndex == 0;

                if (this->UseShader && ( LUT_NeedsToBeInitialized || LookupTableChanged)) {
                        if (RGBImage) {
                        } else if (LookupTable != NULL) {

                                GLsizei lutNvals = 1024;
                                GLvoid* lutData = NULL;

                                if (LookupTable != NULL) {
                                        vtkLookupTable* LUT = static_cast<vtkLookupTable*>(LookupTable);
                                        lutNvals = LUT->GetNumberOfTableValues();
                                        lutData = LUT->GetPointer(0);
                                } else {
                                        LOG_ERROR("OpenGLComponent", "LUT Not found. Bad initiallization");
                                }

                                // Draw() binds it to its own texture unit
                                if (this->LUTIndex == 0) {
                                        glGenTextures(1, &this->LUTIndex);
                                }
                                glBindTexture(GL_TEXTURE_1D, this->LUTIndex);
                                glTexParameterf( GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                                glTexParameterf( GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
                                glTexParameterf( GL_TEXTURE_1D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_EDGE );

                                glTexImage1D( GL_TEXTURE_1D, 0 , GL_RGBA8,
                                              lutNvals, 0, GL_RGBA,
                                              GL_UNSIGNED_BYTE, lutData);
                        }
                }
                LookupTableChanged = false;
        }

}

// ----------------------------------------------------------------------------
void vtkGinkgoOpenGLTexture::PostRender(vtkRenderer *vtkNotUsed(ren))
{
        // Draw() restores everything it changes
}

// ----------------------------------------------------------------------------
void vtkGinkgoOpenGLTexture::Draw(vtkRenderer* ren, vtkPolyData* poly, const float mvp[16], double opacity)
{
        GNC::GCS::ILocker lock(this);

        vtkOpenGLRenderWindow* renWin = vtkOpenGLRenderWindow::SafeDownCast(ren->GetRenderWindow());
        vtkPoints* points = poly->GetPoints();
        vtkDataArray* tcoords = poly->GetPointData()->GetTCoords();
        if (renWin == NULL || this->Index == 0 || points == NULL || tcoords == NULL) {
                return;
        }

        // Triangles with position and texture coordinates
        std::vector<float> vertices;
        vtkCellArray* polys = poly->GetPolys();
        vtkIdType npts = 0;
        const vtkIdType* ids = NULL;
        for (polys->InitTraversal(); polys->GetNextCell(npts, ids); ) {
                for (vtkIdType i = 1; i + 1 < npts; ++i) {
                        const vtkIdType corners[3] = { ids[0], ids[i], ids[i + 1] };
                        for (int c = 0; c < 3; ++c) {
                                double p[3];
                                points->GetPoint(corners[c], p);
                                double* tc = tcoords->GetTuple(corners[c]);
                                vertices.push_back(static_cast<float>(p[0]));
                                vertices.push_back(static_cast<float>(p[1]));
                                vertices.push_back(static_cast<float>(p[2]));
                                vertices.push_back(static_cast<float>(tc[0]));
                                vertices.push_back(static_cast<float>(tc[1]));
                        }
                }
        }
        if (vertices.empty()) {
                return;
        }

        // 0: texture as is (colours already mapped on the CPU)
        // 1: grey value through the lookup table
        // 2: RGB with brightness and contrast
        int mode = 0;
        float lutShift = 0.0f, lutScale = 1.0f;
        if (this->UseShader && this->RGBImage) {
                mode = 2;
        } else if (this->UseShader && this->LookupTable != NULL && this->LUTIndex != 0) {
                mode = 1;
                double normRange[2] = {0.0, 1.0};
                vtkImageData* input = this->GetInput();
                vtkDataArray* scalars = (input != NULL) ? this->GetInputArrayToProcess(0, input) : NULL;
                if (scalars != NULL) {
                        scalars->GetRange(normRange);
                }
                const double* drange = this->LookupTable->GetRange();
                const double span = (normRange[1] - normRange[0]) != 0.0 ? (normRange[1] - normRange[0]) : 1.0;
                const float range0 = static_cast<float>((drange[0] - normRange[0]) / span);
                const float range1 = static_cast<float>((drange[1] - normRange[0]) / span);
                lutShift = -range0;
                lutScale = (range1 != range0) ? 1.0f / (range1 - range0) : 1.0f;
        }

        vtkShaderProgram* program = renWin->GetShaderCache()->ReadyShaderProgram(ImageVertexShader, ImageFragmentShader, "");
        if (program == NULL) {
                return;
        }

        vtkTextureUnitManager* units = renWin->GetTextureUnitManager();
        const int imageUnit = units->Allocate();
        const int lutUnit = (mode == 1) ? units->Allocate() : -1;
        vtkOpenGLState* state = renWin->GetState();

        state->vtkglActiveTexture(GL_TEXTURE0 + imageUnit);
        glBindTexture(GL_TEXTURE_2D, this->Index);
        if (lutUnit >= 0) {
                state->vtkglActiveTexture(GL_TEXTURE0 + lutUnit);
                glBindTexture(GL_TEXTURE_1D, this->LUTIndex);
        }

        float mvpCopy[16];
        std::copy(mvp, mvp + 16, mvpCopy);
        program->SetUniformMatrix4x4("uMVP", mvpCopy);
        program->SetUniformi("uImage", imageUnit);
        program->SetUniformi("uLookupTable", (lutUnit >= 0) ? lutUnit : imageUnit);
        program->SetUniformi("uMode", mode);
        program->SetUniformf("uLutShift", lutShift);
        program->SetUniformf("uLutScale", lutScale);
        program->SetUniformf("uBrightness", this->Brightness);
        program->SetUniformf("uContrast", this->Contrast);
        program->SetUniformf("uOpacity", static_cast<float>(opacity));

        // Textures premultiplied by alpha need a matching blend function
        vtkOpenGLState::ScopedglBlendFuncSeparate blendSaver(state);
        if (this->PremultipliedAlpha) {
                state->vtkglBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }

        if (this->VAO == NULL) {
                this->VAO = vtkOpenGLVertexArrayObject::New();
                this->VBO = vtkOpenGLBufferObject::New();
                this->VBO->SetType(vtkOpenGLBufferObject::ArrayBuffer);
        }
        this->VAO->Bind();
        this->VBO->Upload(vertices, vtkOpenGLBufferObject::ArrayBuffer);
        const size_t stride = 5 * sizeof(float);
        this->VAO->AddAttributeArray(program, this->VBO, "aPos", 0, stride, VTK_FLOAT, 3, false);
        this->VAO->AddAttributeArray(program, this->VBO, "aTex", 3 * sizeof(float), stride, VTK_FLOAT, 2, false);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 5));
        this->VAO->Release();

        if (lutUnit >= 0) {
                units->Free(lutUnit);
        }
        units->Free(imageUnit);
}

// ----------------------------------------------------------------------------
static int FindPowerOfTwo(int i)
{
        int size;

        for ( i--, size=1; i > 0; size*=2 ) {
                i /= 2;
        }

        // [these lines added by Tim Hutton (implementing Joris Vanden Wyngaerd's
        // suggestions)]
        // limit the size of the texture to the maximum allowed by OpenGL
        // (slightly more graceful than texture failing but not ideal)
        GLint maxDimGL;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxDimGL);
        if ( size > maxDimGL ) {
                size = maxDimGL ;
        }
        // end of Tim's additions

        return size;
}

// ----------------------------------------------------------------------------
// Creates resampled unsigned char texture map that is a power of two in both
// x and y.
unsigned char *vtkGinkgoOpenGLTexture::ResampleToPowerOfTwo(int &xs,
                int &ys,
                unsigned char *dptr,
                int bpp)
{
        unsigned char *tptr, *p, *p1, *p2, *p3, *p4;
        int xsize, ysize, i, j, k, jOffset, iIdx, jIdx;
        double pcoords[3], hx, hy, rm, sm, w0, w1, w2, w3;

        xsize = FindPowerOfTwo(xs);
        ysize = FindPowerOfTwo(ys);
        if (this->RestrictPowerOf2ImageSmaller) {
                if (xsize > xs) {
                        xsize /= 2;
                }
                if (ysize > ys) {
                        ysize /= 2;
                }
        }
        hx = (xs - 1.0) / (xsize - 1.0);
        hy = (ys - 1.0) / (ysize - 1.0);

        tptr = p = new unsigned char[xsize*ysize*bpp];

        // Resample from the previous image. Compute parametric coordinates and
        // interpolate
        for (j=0; j < ysize; j++) {
                pcoords[1] = j*hy;

                jIdx = static_cast<int>(pcoords[1]);
                if ( jIdx >= (ys-1) ) { //make sure to interpolate correctly at edge
                        jIdx = ys - 2;
                        pcoords[1] = 1.0;
                } else {
                        pcoords[1] = pcoords[1] - jIdx;
                }
                jOffset = jIdx*xs;
                sm = 1.0 - pcoords[1];

                for (i=0; i < xsize; i++) {
                        pcoords[0] = i*hx;
                        iIdx = static_cast<int>(pcoords[0]);
                        if ( iIdx >= (xs-1) ) {
                                iIdx = xs - 2;
                                pcoords[0] = 1.0;
                        } else {
                                pcoords[0] = pcoords[0] - iIdx;
                        }
                        rm = 1.0 - pcoords[0];

                        // Get pointers to 4 surrounding pixels
                        p1 = dptr + bpp*(iIdx + jOffset);
                        p2 = p1 + bpp;
                        p3 = p1 + bpp*xs;
                        p4 = p3 + bpp;

                        // Compute interpolation weights interpolate components
                        w0 = rm*sm;
                        w1 = pcoords[0]*sm;
                        w2 = rm*pcoords[1];
                        w3 = pcoords[0]*pcoords[1];
                        for (k=0; k < bpp; k++) {
                                *p++ = static_cast<unsigned char>(p1[k]*w0 + p2[k]*w1 + p3[k]*w2
                                                                  + p4[k]*w3);
                        }
                }
        }

        xs = xsize;
        ys = ysize;

        return tptr;
}

// ----------------------------------------------------------------------------
void vtkGinkgoOpenGLTexture::PrintSelf(ostream& os, vtkIndent indent)
{
        this->Superclass::PrintSelf(os,indent);
        os << indent << "Index: " << this->Index << endl;
}


void vtkGinkgoOpenGLTexture::ForceEnableShaders(bool force)
{
        InternalEnableShaders = force;
}
