#pragma once
#define VOX_API_VERSION 0x00000100

typedef void*          VOXhandle;
typedef char           VOXbool;
typedef unsigned short VOXushort;
typedef unsigned int   VOXuint;
typedef float          VOXfloat;

typedef enum {

    VOX_FALSE = 0x0000,
    VOX_TRUE  = 0x0001,
    
    // TODO: Popular pixel and voxel types need to be enumerated
    VOX_TYPE_UINT32 = 0x4000,
    VOX_TYPE_UINT16 = 0x4001,
    
    VOX_SOURCE_CL_BUFFER   = 0x3001, // sourceData is a handle to an OpenCL memory buffer
    VOX_SOURCE_CL_IMAGE    = 0x3002, // sourceData is a handle to an OpenCL image object
    VOX_SOURCE_GL_TEXTURE  = 0x3003, // sourceData is a handle to an OpenGL texture
    VOX_SOURCE_GL_BUFFER   = 0x3004, // sourceData is a handle to an OpenGL pixel buffer object
    VOX_SOURCE_CPU_MEMORY  = 0x3005, // sourceData is a CPU-side pointer to raw data (cannot be used with VOX_SOURCE_USE_PTR)
    VOX_SOURCE_USE_PTR     = 0x3110, // OpenVOX can use memory referenced by sourceData as the storage bits for the memory object
    VOX_SOURCE_COPY_PTR    = 0x3120, // OpenVOX should allocate its own memory and copy data from memory referenced by sourceData
    VOX_SOURCE_IGNORE_PTR  = 0x0000, // OpenVOX should allocate uninitialized memory and ignore sourceData

    VOX_VOXELIZE_VOLUMETRIC           = 0x0100,
    VOX_VOXELIZE_SURFACE_BRUTE        = 0x0101,
    VOX_VOXELIZE_SURFACE_CONSERVATIVE = 0x0102,
    VOX_VOXELIZE_SPATIAL_HASH         = 0x0103,
    VOX_VOXELIZE_SURFACE_OCTREE       = 0x0104,
    VOX_VOXELIZE_SURFACE_THIN         = 0x0105,

    VOX_TRANSFORM_DISTANCE_EUCLIDEAN     = 0x0200,
    VOX_TRANSFORM_DISTANCE_SQR_EUCLIDEAN = 0x0201,
    VOX_TRANSFORM_DISTANCE_MANHATTAN     = 0x0202,
    VOX_TRANSFORM_GRADIENT               = 0x0300,
    VOX_TRANSFORM_CURL                   = 0x0301,
    VOX_TRANSFORM_FLUID_ADVECT           = 0x0400,
    VOX_TRANSFORM_FLUID_JACOBI           = 0x0401,

    VOX_BLEND_ADD      = 0x1000,
    VOX_BLEND_SUBTRACT = 0x1001,
    VOX_BLEND_LERP     = 0x1002,
    VOX_BLEND_BLIT     = 0x1003,

    VOX_GENERATE_CLEAR = 0x2000,
    VOX_GENERATE_NOISE = 0x2001,
    VOX_GENERATE_SPLAT = 0x2002,

    VOX_PARAM_CLEAR_VALUE      = 0x80000000,
    VOX_PARAM_SCISSOR_ENABLE   = 0x80000001,
    VOX_PARAM_SCISSOR_REGION   = 0x80000002,
    VOX_PARAM_NOISE_OCTAVE     = 0x80000003,
    VOX_PARAM_NOISE_COEFF      = 0x80000004,
    VOX_PARAM_SPLAT_COEFF      = 0x80000005,
    VOX_PARAM_FLUID_OBSTACLES  = 0x80000006,

} VOXenum;

VOXhandle voxCreateContext(
    void (*error_callback)(const char *, void *),
    void *user_data);

void voxDeleteHandle(VOXhandle);

VOXhandle voxRegisterMesh(
    VOXhandle context,
    GLuint vertexBuffer,
    VOXuint vertStride,
    VOXuint triangleCount);

VOXhandle voxRegisterMeshIndexed(
    VOXhandle context,
    GLuint vertexBuffer,
    GLuint indexBuffer,
    VOXuint vertStride,
    VOXenum indexType,
    VOXuint triangleCount);

VOXhandle voxCreateVolume(
    VOXhandle context,
    VOXuint width,
    VOXuint height,
    VOXuint depth,
    VOXenum type,
    VOXenum sourceFlags,
    void* sourceData);

void voxUpdateVolume(
    VOXhandle volume,
    VOXenum sourceFlags,
    void* sourceData);

VOXhandle voxCreateImage(
    VOXhandle context,
    VOXuint width,
    VOXuint height,
    VOXenum type,
    VOXenum sourceFlags,
    void* sourceData);

void voxUpdateImage(
    VOXhandle image,
    VOXenum sourceFlags,
    void* sourceData);

VOXhandle voxCreatePath(
    VOXhandle context,
    VOXuint nodeCount,
    VOXenum sourceFlags,
    void* sourceData);

void voxUpdatePath(
    VOXhandle path,
    VOXenum sourceFlags,
    void* sourceData);

void voxVoxelize(VOXhandle mesh, VOXhandle volume, VOXenum voxelizeOp);
void voxSweepImage(VOXhandle destVolume, VOXhandle srcImage, VOXhandle path, VOXenum blendOp);
void voxSweepVolume(VOXhandle destVolume, VOXhandle srcVolume, VOXhandle path, VOXenum blendOp);
void voxGenerate(VOXhandle destVolume, VOXenum generateOp);
void voxTransform(VOXhandle destVolume, VOXhandle srcVolume, VOXenum transformOp);
void voxBlend(VOXhandle destVolume, VOXhandle volume0, VOXhandle volume1, VOXenum blendOp);
void voxCopy(VOXhandle destVolume, VOXhandle srcVolume);

void voxGetParamv(VOXenum param, void*);
void voxResetParamv(VOXenum param);
void voxSetParam1h(VOXenum param, VOXhandle);
void voxSetParam1b(VOXenum param, VOXbool);
void voxSetParam1ui(VOXenum param, VOXuint);
void voxSetParam2ui(VOXenum param, VOXuint, VOXuint);
void voxSetParam3ui(VOXenum param, VOXuint, VOXuint, VOXuint);
void voxSetParam4ui(VOXenum param, VOXuint, VOXuint, VOXuint, VOXuint);
void voxSetParamuiv(VOXenum param, VOXuint*);
void voxSetParam1f(VOXenum param, VOXfloat);
void voxSetParam2f(VOXenum param, VOXfloat, VOXfloat);
void voxSetParam3f(VOXenum param, VOXfloat, VOXfloat, VOXfloat);
void voxSetParam4f(VOXenum param, VOXfloat, VOXfloat, VOXfloat, VOXfloat);
void voxSetParamfv(VOXenum param, VOXfloat*);
