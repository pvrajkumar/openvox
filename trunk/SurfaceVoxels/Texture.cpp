#include "Common.hpp"
#include <stdio.h>
#include <memory.h>
#include <string.h>

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((unsigned int)(unsigned char)(ch0) | ((unsigned int)(unsigned char)(ch1) << 8) | \
    ((unsigned int)(unsigned char)(ch2) << 16) | ((unsigned int)(unsigned char)(ch3) << 24))
#endif

#define FOURCC_DXT1 MAKEFOURCC('D', 'X', 'T', '1')
#define FOURCC_DXT2 MAKEFOURCC('D', 'X', 'T', '2')
#define FOURCC_DXT3 MAKEFOURCC('D', 'X', 'T', '3')
#define FOURCC_DXT4 MAKEFOURCC('D', 'X', 'T', '4')
#define FOURCC_DXT5 MAKEFOURCC('D', 'X', 'T', '5')

#define CR_DDS_PIXEL_DATA_OFFSET 128
#define CR_FOURCC_OFFSET 84
#define CR_MIPMAP_COUNT_OFFSET 28
#define CR_LINEAR_SIZE_OFFSET 20
#define CR_DDS_WIDTH_OFFSET 16
#define CR_DDS_HEIGHT_OFFSET 12

struct DDSData
{
    int	Width;
    int	Height;
    int	Components;
    GLenum Format;
    int	NumMipMaps;
    unsigned char* Pixels;
};

static DDSData *ReadCompressedTexture(const char *pFileName);

TexturePod LoadTexture(std::string ddsFilename)
{
    TexturePod texture;
    DDSData *pDDSImageData = ReadCompressedTexture(ddsFilename.c_str());

    PezCheckCondition(pDDSImageData != 0, "Unable to load DDS file %s\n", ddsFilename.c_str());

    int iSize;
    int iBlockSize;
    int iOffset		= 0;
    int iHeight		= pDDSImageData->Height;
    int iWidth		= pDDSImageData->Width;
    int iNumMipMaps = pDDSImageData->NumMipMaps;

    iBlockSize = (pDDSImageData->Format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) ? 8 : 16;

    glGenTextures(1, &texture.Handle);
    glBindTexture(GL_TEXTURE_2D, texture.Handle);

    if(iNumMipMaps < 2)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        iNumMipMaps = 1;
    }
    else
    {
        //Set texture filtering.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    for(int i = 0; i < iNumMipMaps; i++)
    {
        iSize = ((iWidth + 3) / 4) * ((iHeight + 3) / 4) * iBlockSize;
        glCompressedTexImage2D(GL_TEXTURE_2D, i, pDDSImageData->Format, iWidth, iHeight, 0, iSize, pDDSImageData->Pixels + iOffset);
        iOffset += iSize;

        //Scale next level.
        iWidth  /= 2;
        iHeight /= 2;
    }

    texture.Format = pDDSImageData->Format;
    texture.Width = pDDSImageData->Width;
    texture.Height = pDDSImageData->Height;

    if(pDDSImageData->Pixels)
        delete[] pDDSImageData->Pixels;

    delete pDDSImageData;

    return texture;
}

DDSData *ReadCompressedTexture(const char *pFileName)
{
    FILE *pFile;
    if(!(pFile = fopen(pFileName, "rb")))
        return NULL;

    char pFileCode[4];
    fread(pFileCode, 1, 4, pFile);
    if(strncmp(pFileCode, "DDS ", 4) != 0)
        return NULL;

    //Get the descriptor.
    unsigned int uiFourCC;
    fseek(pFile, CR_FOURCC_OFFSET, SEEK_SET);
    fread(&uiFourCC, sizeof(unsigned int), 1, pFile);

    unsigned int uiLinearSize;
    fseek(pFile, CR_LINEAR_SIZE_OFFSET, SEEK_SET);
    fread(&uiLinearSize, sizeof(unsigned int), 1, pFile);

    unsigned int uiMipMapCount;
    fseek(pFile, CR_MIPMAP_COUNT_OFFSET, SEEK_SET);
    fread(&uiMipMapCount, sizeof(unsigned int), 1, pFile);

    unsigned int uiWidth;
    fseek(pFile, CR_DDS_WIDTH_OFFSET, SEEK_SET);
    fread(&uiWidth, sizeof(unsigned int), 1, pFile);

    unsigned int uiHeight;
    fseek(pFile, CR_DDS_HEIGHT_OFFSET, SEEK_SET);
    fread(&uiHeight, sizeof(unsigned int), 1, pFile);

    int iFactor;
    int iBufferSize;

    DDSData *pDDSImageData = new DDSData;
    memset(pDDSImageData, 0, sizeof(DDSData));

    switch(uiFourCC)
    {
        case FOURCC_DXT1: pDDSImageData->Format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT; iFactor = 2; break;
        case FOURCC_DXT3: pDDSImageData->Format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT; iFactor = 4; break;
        case FOURCC_DXT5: pDDSImageData->Format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT; iFactor = 4; break;
        default:
            PezFatalError("Unknown FOURCC code in DDS file.");
    }

    iBufferSize = (uiMipMapCount > 1) ? (uiLinearSize * iFactor) : uiLinearSize;
    pDDSImageData->Pixels = new unsigned char[iBufferSize];

    pDDSImageData->Width      = uiWidth;
    pDDSImageData->Height     = uiHeight;
    pDDSImageData->NumMipMaps = uiMipMapCount;
    pDDSImageData->Components = (uiFourCC == FOURCC_DXT1) ? 3 : 4;

    fseek(pFile, CR_DDS_PIXEL_DATA_OFFSET, SEEK_SET);
    fread(pDDSImageData->Pixels, 1, iBufferSize, pFile);
    fclose(pFile);

    return pDDSImageData;
}

SurfacePod CreateSurface(int width, int height)
{
    SurfacePod pod;

    // Create a FBO:
    glGenFramebuffers(1, &pod.Fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, pod.Fbo);

    // Create a color texture:
    glGenTextures(1, &pod.ColorTexture);
    glBindTexture(GL_TEXTURE_2D, pod.ColorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, 0);
    glGenerateMipmap(GL_TEXTURE_2D);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to create FBO color texture");

    // Attach the color buffer:
    GLuint colorbuffer;
    glGenRenderbuffers(1, &colorbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorbuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pod.ColorTexture, 0);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to attach color buffer");

    // Validate the FBO:
    PezCheckCondition(GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER), "Unable to create FBO.");
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error.");

    pod.Width = width;
    pod.Height = height;
    pod.Depth = 1;
    return pod;
}

SurfacePod CreatePboSurface(int width, int height)
{
    SurfacePod pod;

    // Create a FBO:
    glGenFramebuffers(1, &pod.Fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, pod.Fbo);

    // Create a color texture:
    glGenTextures(1, &pod.ColorTexture);
    glBindTexture(GL_TEXTURE_2D, pod.ColorTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glGenerateMipmap(GL_TEXTURE_2D);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to create FBO color texture");

    // Attach the color buffer:
    GLuint colorbuffer;
    glGenRenderbuffers(1, &colorbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorbuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pod.ColorTexture, 0);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to attach color buffer");

    // Validate the FBO:
    PezCheckCondition(GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER), "Unable to create FBO.");
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error.");

    // Allocate an uninitialized PBO (written to by OpenCL, read from by OpenGL):
    pod.ByteCount = width * height * sizeof(float) * 4;
    glGenBuffers(1, &pod.Pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pod.Pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pod.ByteCount, 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error with PBO allocation.");

    pod.Width = width;
    pod.Height = height;
    pod.Depth = 1;
    return pod;
}

SurfacePod CreateFboVolume(int width, int height, int depth)
{
    SurfacePod pod;
    pod.Width = width;
    pod.Height = height;
    pod.Depth = depth;

    GLenum internalFormat, format, type;
    internalFormat = GL_R16F;
    format = GL_RED;
    type = GL_HALF_FLOAT;
    pod.RowPitch = 2 * width;
    pod.SlicePitch = pod.RowPitch * depth;

    // Create a FBO:
    glGenFramebuffers(1, &pod.Fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, pod.Fbo);

    // Create a color texture:
    glGenTextures(1, &pod.ColorTexture);
    glBindTexture(GL_TEXTURE_3D, pod.ColorTexture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, width, height, depth, 0, format, type, 0);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to create volume texture");

    // Attach the color buffer:
    GLuint colorbuffer;
    glGenRenderbuffers(1, &colorbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorbuffer);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, pod.ColorTexture, 0);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to attach color buffer");

    // Validate the FBO:
    PezCheckCondition(GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER), "Unable to create FBO.");
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error with volume FBO creation.");

    return pod;
}

SurfacePod CreatePboVolume(int width, int height, int depth)
{
    SurfacePod pod;
    pod.Width = width;
    pod.Height = height;
    pod.Depth = depth;

    GLenum internalFormat, format, type;
    internalFormat = GL_R8;
    format = GL_RED;
    type = GL_UNSIGNED_BYTE;
    pod.RowPitch = width;
    pod.SlicePitch = pod.RowPitch * height;
    pod.ByteCount = pod.SlicePitch * pod.Depth;
    void* pEmpty = calloc(pod.ByteCount, 1);

    // Create a color texture:
    glGenTextures(1, &pod.ColorTexture);
    glBindTexture(GL_TEXTURE_3D, pod.ColorTexture);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage3D(GL_TEXTURE_3D, 0, internalFormat, width, height, depth, 0, format, type, pEmpty);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to create volume texture");

    // Allocate an uninitialized PBO (written to by OpenCL, read from by OpenGL):
    glGenBuffers(1, &pod.Pbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pod.Pbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pod.ByteCount, 0, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error with PBO allocation.");

    // Allocate an uninitialized PBO (written to by OpenCL, read from by OpenGL):
    glGenBuffers(1, &pod.ClearPbo);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pod.ClearPbo);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, pod.ByteCount, pEmpty, GL_STATIC_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error with PBO allocation.");

    free(pEmpty);
    return pod;
}

SurfacePod CreateIntervalSurface(int width, int height)
{
    SurfacePod surface;
    glGenFramebuffers(1, &surface.Fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, surface.Fbo);

    GLuint textureHandle;
    glGenTextures(1, &textureHandle);
    glBindTexture(GL_TEXTURE_2D, textureHandle);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    surface.ColorTexture = textureHandle;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_HALF_FLOAT, 0);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to create FBO texture");

    GLuint colorbuffer;
    glGenRenderbuffers(1, &colorbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorbuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureHandle, 0);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to attach color buffer");

    // Create a depth texture:
    glGenTextures(1, &surface.DepthTexture);
    glBindTexture(GL_TEXTURE_2D, surface.DepthTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 0);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to create depth texture");

    // Create and attach a depth buffer:
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, surface.DepthTexture, 0);
    PezCheckCondition(GL_NO_ERROR == glGetError(), "Unable to attach depth buffer");

    PezCheckCondition(GL_FRAMEBUFFER_COMPLETE == glCheckFramebufferStatus(GL_FRAMEBUFFER), "Unable to create FBO.");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    surface.Width = width;
    surface.Height = height;
    surface.Depth = 0;
    return surface;
}
