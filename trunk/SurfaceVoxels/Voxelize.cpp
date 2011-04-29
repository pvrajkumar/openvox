#include "Common.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <glew.h>
#include <pez.h>
#include <glsw.h>
#include <CL/opencl.h>

using namespace vmath;

#define MAX_MESH_COUNT 16
#define DIRECT_TEXTURE_WRITES

static cl_context context;
static cl_program program;
static cl_int err;
static cl_kernel voxelizeKernel, clearKernel;
static cl_command_queue commandQueue;
static cl_device_id deviceId;
static cl_mem inBuffers[1+MAX_MESH_COUNT*2];
static unsigned int triangleCount[MAX_MESH_COUNT];
static unsigned int meshCount = 0;
static SurfacePod* VolumeSurface;

static cl_platform_id GpuGetPlatform();

void __stdcall handle_error(const char* errinfo, const void* private_info, size_t cb, void* user_data)
{
    PezFatalError(errinfo);
}

void InitOpenCL(SurfacePod& destination)
{
    cl_platform_id platformId = GpuGetPlatform();
    const char* kernelSource;
    cl_context_properties glContext, hdc;
    char version_string[128] = {0}, extensions[256] = {0};
    int maxSize;

    VolumeSurface = &destination;

    clGetDeviceIDs(platformId, CL_DEVICE_TYPE_GPU, 1, &deviceId, NULL);

    clGetDeviceInfo(deviceId, CL_DEVICE_VERSION, sizeof(version_string), &version_string[0], NULL);
    clGetDeviceInfo(deviceId, CL_DEVICE_EXTENSIONS, sizeof(extensions), &extensions[0], NULL);
    clGetDeviceInfo(deviceId, CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(maxSize), &maxSize, NULL);
    PezDebugString("%s\n%d max work items\n%s\n", version_string, maxSize, extensions);

    PezGetContext((int**) &glContext, (int**) &hdc);

    cl_context_properties props[] = {
        CL_GL_CONTEXT_KHR, glContext,
        CL_WGL_HDC_KHR, hdc,
        0
    };
    
    context = clCreateContext(props, 1, &deviceId, handle_error, NULL, 0);

    PezCheckCondition(context != 0, "Failed to create OpenCL context.\n");
    
    err = 0;
    inBuffers[0] = clCreateFromGLBuffer(context, CL_MEM_WRITE_ONLY, VolumeSurface->ColorTexture, &err);
    switch (err)
    {
        case CL_INVALID_CONTEXT:   PezFatalError("OpenCL invalid context."); break;
        case CL_INVALID_VALUE:     PezFatalError("OpenCL invalid value."); break;
        case CL_INVALID_MIP_LEVEL: PezFatalError("OpenCL invalid mip level."); break;
        case CL_INVALID_GL_OBJECT: PezFatalError("OpenCL invalid GL object."); break;
        case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR: PezFatalError("OpenCL image format desc."); break;
        case CL_INVALID_OPERATION:  PezFatalError("OpenCL invalid operation."); break;
        case CL_OUT_OF_RESOURCES:   PezFatalError("OpenCL out of resources."); break;
        case CL_OUT_OF_HOST_MEMORY: PezFatalError("OpenCL out of host memory."); break;
        case CL_SUCCESS: break;
        default: PezFatalError("OpenCL error.");
    }
    
    kernelSource = glswGetShader("Kernels.Surfaces");
    program = clCreateProgramWithSource(context, 1, &kernelSource, NULL, NULL);
    
    err = clBuildProgram(program, 0, NULL, "-cl-fast-relaxed-math", NULL, NULL);
    if (err != CL_SUCCESS) {
        size_t len;
        char buffer[2048] = {0};
        memset(buffer, 0, sizeof(buffer));
        clGetProgramBuildInfo(program, deviceId, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        PezFatalError("Error: Failed to build OpenCL kernel\n%s\n", buffer);
    }
    
    
    voxelizeKernel = clCreateKernel(program, "voxelize", NULL);
    clearKernel = clCreateKernel(program, "fast_clear", NULL);
    commandQueue = clCreateCommandQueue(context, deviceId, 0, NULL);

    destination.ComputeBuffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, destination.ByteCount, NULL, &err);
    PezCheckCondition(!err, "Failed to create staging buffer for 3D writes.\n");

    void* empty = calloc(destination.ByteCount, 1);
    destination.ClearBuffer = clCreateBuffer(context, CL_MEM_COPY_HOST_PTR, destination.ByteCount, empty, &err);
    free(empty);
    PezCheckCondition(!err, "Failed to create clear buffer.\n");
}

void AddOpenCL(const MeshPod& mesh)
{
    inBuffers[1+2*meshCount] = clCreateFromGLBuffer(context, CL_MEM_READ_ONLY, mesh.PositionsBuffer, &err);
    PezCheckCondition(err == 0, "Unable to create OpenCL point buffer");

    inBuffers[2+2*meshCount] = clCreateFromGLBuffer(context, CL_MEM_READ_ONLY, mesh.TriangleBuffer, &err);
    PezCheckCondition(err == 0, "Unable to create OpenCL triangle buffer");

    triangleCount[meshCount++] = mesh.TriangleCount;
}

void RunOpenCL(Point3 minCorner, Point3 maxCorner)
{
    size_t localSize;
    clGetKernelWorkGroupInfo(voxelizeKernel, deviceId, CL_KERNEL_WORK_GROUP_SIZE, sizeof(int), &localSize, 0) ;

    err = clEnqueueAcquireGLObjects(commandQueue, 1+2*meshCount, &inBuffers[0], 0,0,0);
    PezCheckCondition(err == 0, "Unable to lock vertex buffers for OpenCL\n");

    float xscale = VolumeSurface->Width / (maxCorner[0] - minCorner[0]);
    float yscale = VolumeSurface->Height / (maxCorner[1] - minCorner[1]);
    float zscale = VolumeSurface->Depth / (maxCorner[2] - minCorner[2]);
    float xoffset = -minCorner[0];
    float yoffset = -minCorner[1];
    float zoffset = -minCorner[2];

#ifdef DIRECT_TEXTURE_WRITES
    err |= clEnqueueCopyBuffer(commandQueue, (cl_mem) VolumeSurface->ClearBuffer, (cl_mem) inBuffers[0], 0, 0, VolumeSurface->ByteCount, 0, 0, 0);
    err |= clSetKernelArg(voxelizeKernel, 0, sizeof(cl_mem), &inBuffers[0]);
#else
    err |= clEnqueueCopyBuffer(commandQueue, (cl_mem) VolumeSurface->ClearBuffer, (cl_mem) VolumeSurface->ComputeBuffer, 0, 0, VolumeSurface->ByteCount, 0, 0, 0);
    err |= clSetKernelArg(voxelizeKernel, 0, sizeof(cl_mem), &VolumeSurface->ComputeBuffer);
#endif

    err |= clSetKernelArg(voxelizeKernel, 1, sizeof(float), &xscale);
    err |= clSetKernelArg(voxelizeKernel, 2, sizeof(float), &yscale);
    err |= clSetKernelArg(voxelizeKernel, 3, sizeof(float), &zscale);
    err |= clSetKernelArg(voxelizeKernel, 4, sizeof(float), &xoffset);
    err |= clSetKernelArg(voxelizeKernel, 5, sizeof(float), &yoffset);
    err |= clSetKernelArg(voxelizeKernel, 6, sizeof(float), &zoffset);
    err |= clSetKernelArg(voxelizeKernel, 7, sizeof(int), &VolumeSurface->RowPitch);
    err |= clSetKernelArg(voxelizeKernel, 8, sizeof(int), &VolumeSurface->SlicePitch);
    err |= clSetKernelArg(voxelizeKernel, 9, sizeof(int), &VolumeSurface->Width);
    err |= clSetKernelArg(voxelizeKernel, 10, sizeof(int), &VolumeSurface->Height);
    err |= clSetKernelArg(voxelizeKernel, 11, sizeof(int), &VolumeSurface->Depth);
    PezCheckCondition(!err, "Unable to set arguments 0-11 on OpenCL kernel");

    size_t localWorkSize[] = { localSize };

    for (unsigned int meshIndex = 0; meshIndex < meshCount; ++meshIndex)
    {
        err |= clSetKernelArg(voxelizeKernel, 12, sizeof(cl_mem), (void*) &inBuffers[1+meshIndex*2]);
        err |= clSetKernelArg(voxelizeKernel, 13, sizeof(cl_mem), (void*) &inBuffers[2+meshIndex*2]);
        err |= clSetKernelArg(voxelizeKernel, 14, sizeof(int), (void*) &triangleCount[meshIndex]);
        PezCheckCondition(err == 0, "Unable to set arguments 12-14 on OpenCL kernel");

        size_t globalWorkSize[] = { snap(triangleCount[meshIndex], localWorkSize[0]) };

        err = clEnqueueNDRangeKernel(commandQueue, voxelizeKernel, 1, NULL, globalWorkSize, localWorkSize, 0, NULL, NULL);
        PezCheckCondition(err != CL_INVALID_KERNEL_ARGS, "Unable to enqueue 'Voxelize' kernel: invalid kernel args\n");
        PezCheckCondition(err == 0, "Unable to enqueue OpenCL kernel: error code is %d=%8.8x\n", err, err);
    }

#ifndef DIRECT_TEXTURE_WRITES
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, VolumeSurface->Pbo);
    void* pDest = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    PezCheckCondition(glGetError() == GL_NO_ERROR, "Unable to map OpenGL PBO.\n");
    err = clEnqueueReadBuffer(commandQueue, (cl_mem) VolumeSurface->ComputeBuffer, CL_TRUE, 0, VolumeSurface->ByteCount, pDest, 0, 0, 0);
    PezCheckCondition(!err, "Unable to copy buffer: error code is %d\n", err);
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindTexture(GL_TEXTURE_3D, VolumeSurface->ColorTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, VolumeSurface->Width, VolumeSurface->Height, VolumeSurface->Depth, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    PezCheckCondition(glGetError() == GL_NO_ERROR, "Unable to copy PBO to OpenGL Texture.\n");
#endif

    err = clEnqueueReleaseGLObjects(commandQueue, 1+2*meshCount, &inBuffers[0], 0,0,0);
    PezCheckCondition(err == 0, "Unable to release buffers back to OpenGL");

    // Yes, we're clearing TWICE when using direct texture writes.  Works around a driver issue.
#ifdef DIRECT_TEXTURE_WRITES
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, VolumeSurface->ClearPbo);
    glBindTexture(GL_TEXTURE_3D, VolumeSurface->ColorTexture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, VolumeSurface->Width, VolumeSurface->Height, VolumeSurface->Depth, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
    glBindTexture(GL_TEXTURE_3D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    PezCheckCondition(glGetError() == GL_NO_ERROR, "Unable to copy PBO to OpenGL Texture.\n");
#endif
}

void ClearOpenCL()
{
}

static cl_platform_id GpuGetPlatform()
{
    char chBuffer[1024];
    cl_uint num_platforms; 
    cl_platform_id* clPlatformIDs;
    cl_uint i;
    cl_platform_id id;
    cl_int err;
    
    err = clGetPlatformIDs (0, NULL, &num_platforms);
    if (err != CL_SUCCESS) {
        puts("Error with clGetPlatformIDs.\n\n");
        exit(1);
    }
    
    if (num_platforms == 0) {
        puts("No OpenCL platform found.\n\n");
        exit(1);
    }
    
    clPlatformIDs = (cl_platform_id*) malloc(num_platforms * sizeof(cl_platform_id));
    clGetPlatformIDs(num_platforms, clPlatformIDs, NULL);
    
    id = clPlatformIDs[0];
    
    for (i = 0; i < num_platforms; ++i) {
        err = clGetPlatformInfo(clPlatformIDs[i], CL_PLATFORM_NAME, 1024, &chBuffer, NULL);
        if (err == CL_SUCCESS && strstr(chBuffer, "NVIDIA")) {
            id = clPlatformIDs[i];
            break;
        }
    }
    
    free(clPlatformIDs);
    return id;
}
