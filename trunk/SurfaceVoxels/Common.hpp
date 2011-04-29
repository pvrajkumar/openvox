#pragma once
#include <vector>
#include <string.>
#include <vmath.hpp>
#include <pez.h>
#include <glew.h>

struct Particle {
    float Px;  // Position X
    float Py;  // Position Y
    float Pz;  // Position Z
    float ToB; // Time of Birth
    float Vx;  // Velocity X
    float Vy;  // Velocity Y
    float Vz;  // Velocity Z
};

struct GpuParticle {
    float Px;     // Position X
    float Py;     // Position Y
    float Pz;     // Position Z
    float Radius; // Sphere Radius
};

struct TubeVertex {
    float Px;  // Position X
    float Py;  // Position Y
    float Pz;  // Position Z
};

struct PathNode {
    vmath::Point3 Position;
    vmath::Vector3 Guide;
    float MinorRadius;
};

typedef std::vector<GpuParticle> GpuParticleList;
typedef std::vector<Particle> ParticleList;
typedef std::vector<PathNode> TubePath;
typedef std::vector<TubeVertex> TubeBuffer;

enum AttributeSlot {
    SlotPosition,
    SlotTexCoord,
    SlotNormal,
    SlotBirthTime,
    SlotVelocity,
    SlotRadius,
};

struct ITrackball {
    virtual void MouseDown(int x, int y) = 0;
    virtual void MouseUp(int x, int y) = 0;
    virtual void MouseMove(int x, int y) = 0;
    virtual void ReturnHome() = 0;
    virtual vmath::Matrix3 GetRotation() const = 0;
    virtual void Update(unsigned int microseconds) = 0;
};

struct MeshPod {
    GLuint LineBuffer;
    GLuint TriangleBuffer;
    GLuint PositionsBuffer;
    GLuint NormalsBuffer;
    GLuint TexCoordsBuffer;
    GLsizei LineCount;
    GLsizei TriangleCount;
    GLsizei VertexCount;
    vmath::Point3 MinCorner;
    vmath::Point3 MaxCorner;
};

struct TexturePod {
    GLuint Handle;
    GLsizei Width;
    GLsizei Height;
    GLenum Format;
};

struct SurfacePod {
    GLuint Fbo;
    GLuint Pbo;
    GLuint ClearPbo;
    GLuint ColorTexture;
    GLuint DepthTexture;
    GLsizei ByteCount;
    int Width;
    int Height;
    int Depth;
    unsigned int RowPitch;
    unsigned int SlicePitch;
    void* ComputeBuffer;
    void* ClearBuffer;
};

struct TubePod {
    TubePath Path;
    MeshPod Mesh;
    float AnimationPercentage;
    float AnimationDuration;
    float ElapsedTime;
    GLuint SliceCount;
    GLuint StackCount;
    float Length;
    TubeBuffer Verts;
    bool Loop;
};

struct ParticleSystem {
    ParticleList Particles;
    GLuint VertexBuffer;
    float Time;
    TubePod* TravelTube;
};

struct GpuParticleSystem {
    GpuParticleList Particles;
    GLuint VertexBuffer;
    float Time;
};

// Utility functions:
inline float sign(float v) { return v < 0 ? -1.0f : (v > 0 ? +1.0f : 0.0f); }
inline float lerp(float t, float a, float b) { return ( a + ( ( b - a ) * t ) ); }
inline float fract(float f) { return f - floor(f); }
inline size_t snap(size_t a, size_t b) { return ((a % b) == 0) ? a : (a - (a % b) + b); }

// Trackball.cpp
ITrackball* CreateTrackball(float width, float height, float radius);

// Shader.cpp
GLuint LoadProgram(const char* vsKey, const char* gsKey, const char* fsKey);
void SetUniform(const char* name, int value);
void SetUniform(const char* name, float value);
void SetUniform(const char* name, float x, float y);
void SetUniform(const char* name, vmath::Matrix4 value);
void SetUniform(const char* name, vmath::Matrix3 value);
void SetUniform(const char* name, vmath::Vector3 value);
void SetUniform(const char* name, vmath::Vector4 value);

// Geometry.cpp
MeshPod CreateQuad(float left, float top, float right, float bottom);
MeshPod CreateQuad();
void RenderMesh(MeshPod mesh);
void RenderMeshInstanced(MeshPod mesh, int instanceCount);
void RenderWireframe(MeshPod mesh);
MeshPod CreateCube();

// Tube.cpp
TubePod CreatePrimary(int granularity);
TubePod CreateHelix(int granularity, const TubePod& primary);
TubePod CreateStent(int granularity);
void AnimateTubes(TubePod& primary, TubePod& helix, float dt);

// Texture.cpp
TexturePod LoadTexture(std::string ddsFile);
SurfacePod CreateSurface(int width, int height);
SurfacePod CreatePboSurface(int width, int height);
SurfacePod CreateFboVolume(int width, int height, int depth);
SurfacePod CreatePboVolume(int width, int height, int depth);
SurfacePod CreateIntervalSurface(int width, int height);

// Particles.cpp
void AdvectParticles(ParticleSystem& particles, float dt, float speed);
ParticleSystem CreateParticles(int count, TubePod& tube, float spread);
GpuParticleSystem CreateGpuParticles(size_t count);
void RenderParticles(ParticleSystem& particles);
void RenderGpuParticles(GpuParticleSystem& particles);
void FillGpuParticles(GpuParticleSystem& dest, float radius, ParticleList* source, size_t count);
void BinParticles(const SurfacePod& binningSurface,
                  const GpuParticleList& particles,
                  int numBinColumns, int numBinRows,
                  size_t maxParticlesPerBin,
                  vmath::Matrix4 mvp);

// Text.cpp
TexturePod OverlayText(std::string message);
TexturePod OverlayTextf(const char* pStr, ...);

// Voxelize.c
void InitOpenCL(SurfacePod& destination);
void AddOpenCL(const MeshPod& mesh);
void RunOpenCL(vmath::Point3 minCorner, vmath::Point3 maxCorner);
