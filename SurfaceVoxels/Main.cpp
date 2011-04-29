#include <pez.h>
#include <list>
#include <map>
#include <set>
#include "Common.hpp"

using namespace vmath;

// Textures and render targets:
static SurfacePod SolidVoxels;
static SurfacePod SurfaceVoxels;
static SurfacePod ParticleIntervalSurface[2];
static SurfacePod VesselIntervalSurface[2];
static SurfacePod TightIntervalSurface[2];
static SurfacePod RaycastDestination;
static SurfacePod ParticleSurface;
static SurfacePod ParticleBins;

// Triangle meshes and line buffers:
static MeshPod ScreenQuad;
static MeshPod VolumeCube;

// OpenGL shader handles:
static GLuint BlitProgram;
static GLuint TransformParticlesProgram;
static GLuint RaycastVesselProgram;
static GLuint RaycastParticleProgram;
static GLuint SurfaceParticleProgram;
static GLuint SolidParticleProgram;
static GLuint RayIntervalProgram;
static GLuint WireframeProgram;
static GLuint VoxelizeProgram;
static GLuint BlitParticlesProgram;
static GLuint BlitGpuParticlesProgram;
static GLuint ParticleIntervalProgram;
static GLuint VesselIntervalProgram;
static GLuint CombineIntervalsProgram;

// Global variables for this file:
static Matrix4 ProjectionMatrix;
static Matrix4 ViewMatrix;
static Matrix4 ModelviewProjection;
static const float TimeStep = 5;
static float Time = 0;

// High-level application objects:
static TubePod PrimaryTube;
static TubePod HelixTube;
static TubePod StentTube;
static ParticleSystem PrimaryParticles;
static ParticleSystem HelixParticles;
static GpuParticleSystem GpuParticles;
static ITrackball* Trackball = 0;

// Settings:
static bool ShowVoxels = false;
static bool ShowWireframe = false;
static bool ShowParticles = true;
static bool ShowBillboards = false;
static bool SurfaceVoxelization = true;
static bool ClipParticles = true;
static bool ShowHelp = true;
static bool SpatialBinning = true;
static const bool ContinuousFill = false;
static const int NumBinColumns = 32;
static const int NumBinRows = 16;
static const int MaxParticlesPerBin = 31; // n-1 to accommodate terminator
static const float ParticleSize = 0.4f;
static bool DebugRaycast = false;

// Counters:
static float Fips = -4;
static double VoxelizationTime = -1;
static double SurfaceVoxelizationTime = -1;

PezConfig PezGetConfig()
{
    PezConfig config;
    config.Title = "Contrast";
    config.Width = 853*3/2;
    config.Height = 480*3/2;
    config.Multisampling = 0;
    config.VerticalSync = 0;
    return config;
}

void PezInitialize()
{
    PezConfig cfg = PezGetConfig();
    SolidVoxels = CreateFboVolume(512, 256, 64);
    SurfaceVoxels = CreatePboVolume(512, 256, 64);
    ParticleSurface = CreateSurface(cfg.Width/2, cfg.Height/2);
    ParticleBins = CreatePboSurface(NumBinColumns * (MaxParticlesPerBin + 1), NumBinRows);

    {
        int w = cfg.Width/2; int h = cfg.Height/2;
        RaycastDestination = CreateSurface(w, h);
        ParticleIntervalSurface[0] = CreateIntervalSurface(w, h);
        ParticleIntervalSurface[1] = CreateIntervalSurface(w, h);
        VesselIntervalSurface[0] = CreateIntervalSurface(w, h);
        VesselIntervalSurface[1] = CreateIntervalSurface(w, h);
        TightIntervalSurface[0] = CreateIntervalSurface(w, h);
        TightIntervalSurface[1] = CreateIntervalSurface(w, h);
    }

    Trackball = CreateTrackball(cfg.Width * 1.0f, cfg.Height * 1.0f, cfg.Height * 3.0f / 4.0f);
    ScreenQuad = CreateQuad();
    VolumeCube = CreateCube();
    VoxelizeProgram = LoadProgram("Voxelize.VS", "Voxelize.GS", "Voxelize.FS");
    TransformParticlesProgram = LoadProgram("Transform.GpuParticles", 0, 0);
    ParticleIntervalProgram = LoadProgram("Particle.VS", "Particle.Interval.GS", "Particle.Interval.FS");
    VesselIntervalProgram = LoadProgram("Vessel.Interval.VS", 0, "Vessel.Interval.FS");
    SurfaceParticleProgram = LoadProgram("Particle.VS", "Particle.GS", "Particle.Surface.FS");
    SolidParticleProgram = LoadProgram("Particle.VS", "Particle.GS", "Particle.Solid.FS");
    WireframeProgram = LoadProgram("Wireframe.VS", 0, "Wireframe.FS");
    RayIntervalProgram = LoadProgram("Interval.VS", 0, "Interval.FS");
    RaycastVesselProgram = LoadProgram("Blit.VS", 0, "Raycast");
    RaycastParticleProgram = LoadProgram("Blit.VS", 0, "Particle.Raycast");
    BlitParticlesProgram = LoadProgram("Blit.VS", 0, "Blit.Particles");
    BlitGpuParticlesProgram = LoadProgram("Blit.VS", 0, "Blit.GpuParticles");
    BlitProgram = LoadProgram("Blit.VS", 0, "Blit.FS");
    CombineIntervalsProgram = LoadProgram("Blit.VS", 0, "Combine.Interval.FS");
    
    PrimaryTube = CreatePrimary(96);
    StentTube = CreateStent(24);
    HelixTube = CreateHelix(64, PrimaryTube);

    if (ContinuousFill)
    {
        PrimaryParticles = CreateParticles(200, PrimaryTube, 3.0f);
        HelixParticles = CreateParticles(20, HelixTube, 0.25f);
    }
    else
    {
        PrimaryParticles = CreateParticles(15, PrimaryTube, 1.0f);
        HelixParticles = CreateParticles(15, HelixTube, 0.25f);
    }
    GpuParticles = CreateGpuParticles(PrimaryParticles.Particles.size() + HelixParticles.Particles.size());

    InitOpenCL(SurfaceVoxels);
    AddOpenCL(PrimaryTube.Mesh);
    AddOpenCL(StentTube.Mesh);
    AddOpenCL(HelixTube.Mesh);

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glEnable(GL_BLEND);
    glLineWidth(1.5f);
}

void PezRender()
{
    SurfacePod voxels = SurfaceVoxelization ? SurfaceVoxels : SolidVoxels;
    PezConfig cfg = PezGetConfig();

    float e = 0.01f;
    Point3 minCorner = PrimaryTube.Mesh.MinCorner;
    Point3 maxCorner = PrimaryTube.Mesh.MaxCorner;
    minCorner = minPerElem(minCorner, HelixTube.Mesh.MinCorner);
    maxCorner = maxPerElem(maxCorner, HelixTube.Mesh.MaxCorner);
    minCorner -= Vector3(e); maxCorner += Vector3(e);
    Matrix4 voxelProjection = Matrix4::orthographic(minCorner[0], maxCorner[0], minCorner[1], maxCorner[1], minCorner[2], maxCorner[2]);
    Matrix4 mvp = ModelviewProjection * inverse(voxelProjection);
    double startTime;

    // Voxelize the tubes:
    if (SurfaceVoxelization)
    {
        if (ShowVoxels)
        {
            glFinish();
            startTime = PezGetPreciseTime();
        }
        RunOpenCL(minCorner, maxCorner);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, SolidVoxels.Fbo);
        glViewport(0, 0, SolidVoxels.Width, SolidVoxels.Height);
        glClearColor(0, 0, 0, 0);
        glBlendFunc(GL_ONE, GL_ONE);
        glUseProgram(VoxelizeProgram);
        SetUniform("VoxelProjection", voxelProjection);
        SetUniform("SliceScale", (maxCorner[2]-minCorner[2]) / SolidVoxels.Depth);
        if (ShowVoxels)
        {
            glFinish();
            startTime = PezGetPreciseTime();
        }
        glClear(GL_COLOR_BUFFER_BIT);
        RenderMeshInstanced(PrimaryTube.Mesh, SolidVoxels.Depth);
        RenderMeshInstanced(StentTube.Mesh, SolidVoxels.Depth);
        RenderMeshInstanced(HelixTube.Mesh, SolidVoxels.Depth);
    }

    if (ShowVoxels)
    {
        // Dampen fluctuation in the voxelization counter:
        glFinish();
        float alpha = 0.05f;
        double voxelizationTime = PezGetPreciseTime() - startTime;
        if (VoxelizationTime < 0)
            VoxelizationTime = voxelizationTime;
        VoxelizationTime = voxelizationTime * alpha + VoxelizationTime * (1.0f - alpha);

        // Update the ray start & stop surfaces:
        glUseProgram(RayIntervalProgram);
        SetUniform("ModelviewProjection", mvp);
        glClearColor(0, 0, 0, 0);
        glViewport(0, 0, TightIntervalSurface[0].Width, TightIntervalSurface[0].Height);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDepthFunc(GL_LESS);
        glBindFramebuffer(GL_FRAMEBUFFER, TightIntervalSurface[1].Fbo);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderMesh(VolumeCube);
        glDepthFunc(GL_GEQUAL);
        glBindFramebuffer(GL_FRAMEBUFFER, TightIntervalSurface[0].Fbo);
        glClearDepth(0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderMesh(VolumeCube);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);

        // Raycast against the volume texture:
        glBindFramebuffer(GL_FRAMEBUFFER, RaycastDestination.Fbo);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, RaycastDestination.Width, RaycastDestination.Height);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(RaycastVesselProgram);
        SetUniform("RayStart", 0);
        SetUniform("RayStop", 1);
        SetUniform("Volume", 2);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, TightIntervalSurface[0].ColorTexture);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, TightIntervalSurface[1].ColorTexture);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, voxels.ColorTexture);
        RenderMesh(ScreenQuad);

        // Blit the result to the backbuffer:
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.86f, 0.86f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, cfg.Width, cfg.Height);
        glBindTexture(GL_TEXTURE_2D, RaycastDestination.ColorTexture);
        glGenerateMipmap(GL_TEXTURE_2D);
        glUseProgram(BlitProgram);
        RenderMesh(ScreenQuad);
    }
    else if (SpatialBinning)
    {
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);

        // Update the interval surfaces for particles:
        glUseProgram(ParticleIntervalProgram);
        SetUniform("ModelviewProjection", mvp);
        SetUniform("PointSize", ParticleSize);
        SetUniform("Time", Time);
        SetUniform("CubeProjection", voxelProjection);
        SetUniform("ModelviewProjection", ModelviewProjection);
        SetUniform("Modelview", ViewMatrix.getUpper3x3());
        glClearColor(0, 0, 0, 0);
        glViewport(0, 0, ParticleIntervalSurface[0].Width, ParticleIntervalSurface[0].Height);
        glDepthFunc(GL_LESS);
        glBindFramebuffer(GL_FRAMEBUFFER, ParticleIntervalSurface[1].Fbo);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderParticles(PrimaryParticles);
        RenderParticles(HelixParticles);
        glDepthFunc(GL_GEQUAL);
        glBindFramebuffer(GL_FRAMEBUFFER, ParticleIntervalSurface[0].Fbo);
        glClearDepth(0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderParticles(PrimaryParticles);
        RenderParticles(HelixParticles);

        // Update the interval surfaces for the anatomy:
        glUseProgram(VesselIntervalProgram);
        SetUniform("ModelviewProjection", mvp);
        SetUniform("CubeProjection", voxelProjection);
        SetUniform("ModelviewProjection", ModelviewProjection);
        glClearColor(0, 0, 0, 0);
        glViewport(0, 0, VesselIntervalSurface[0].Width, VesselIntervalSurface[0].Height);
        glDepthFunc(GL_LESS);
        glBindFramebuffer(GL_FRAMEBUFFER, VesselIntervalSurface[1].Fbo);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderMesh(PrimaryTube.Mesh);
        RenderMesh(StentTube.Mesh);
        RenderMesh(HelixTube.Mesh);
        glDepthFunc(GL_GEQUAL);
        glBindFramebuffer(GL_FRAMEBUFFER, VesselIntervalSurface[0].Fbo);
        glClearDepth(0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderMesh(PrimaryTube.Mesh);
        RenderMesh(StentTube.Mesh);
        RenderMesh(HelixTube.Mesh);

        if (DebugRaycast)
        {
            glUseProgram(RayIntervalProgram);
            SetUniform("ModelviewProjection", mvp);
            glClearColor(0, 0, 0, 0);
            glViewport(0, 0, TightIntervalSurface[0].Width, TightIntervalSurface[0].Height);
            glEnable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glDepthFunc(GL_LESS);
            glBindFramebuffer(GL_FRAMEBUFFER, TightIntervalSurface[1].Fbo);
            glClearDepth(1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderMesh(VolumeCube);
            glDepthFunc(GL_GEQUAL);
            glBindFramebuffer(GL_FRAMEBUFFER, TightIntervalSurface[0].Fbo);
            glClearDepth(0.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderMesh(VolumeCube);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
        }
        else
        {
            // Find the intersection of the two intervals:
            glUseProgram(CombineIntervalsProgram);
            SetUniform("ParticleCoords", 0);
            SetUniform("ParticleDepths", 1);
            SetUniform("VesselCoords", 2);
            SetUniform("VesselDepths", 3);
            glClearColor(0, 0, 0, 0);
            glViewport(0, 0, TightIntervalSurface[0].Width, TightIntervalSurface[0].Height);
            glBindFramebuffer(GL_FRAMEBUFFER, TightIntervalSurface[0].Fbo);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            SetUniform("RayStart", true);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ParticleIntervalSurface[0].ColorTexture);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, ParticleIntervalSurface[0].DepthTexture);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, VesselIntervalSurface[0].ColorTexture);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, VesselIntervalSurface[0].DepthTexture);
            RenderMesh(ScreenQuad);
            glBindFramebuffer(GL_FRAMEBUFFER, TightIntervalSurface[1].Fbo);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            SetUniform("RayStart", false);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, ParticleIntervalSurface[1].ColorTexture);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, ParticleIntervalSurface[1].DepthTexture);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, VesselIntervalSurface[1].ColorTexture);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, VesselIntervalSurface[1].DepthTexture);
            RenderMesh(ScreenQuad);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
        }

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);

        // Raycast against the volume texture:
        glBindFramebuffer(GL_FRAMEBUFFER, RaycastDestination.Fbo);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, RaycastDestination.Width, RaycastDestination.Height);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(RaycastParticleProgram);

        {
            Vector3 scale = divPerElem(Vector3(float(SurfaceVoxels.Width), float(SurfaceVoxels.Height), float(SurfaceVoxels.Depth)), maxCorner - minCorner);
            Vector3 offset = Point3(0)-minCorner;
            SetUniform("VolumeScale", scale);
            SetUniform("VolumeOffset", offset);
        }

        SetUniform("CubeProjection", inverse(voxelProjection).getUpper3x3());
        SetUniform("RayStart", 0);
        SetUniform("RayStop", 1);
        SetUniform("Boundaries", 2);
        SetUniform("ParticleBins", 3);
        SetUniform("FlipInterval", DebugRaycast);
        SetUniform("NumBinColumns", NumBinColumns);
        SetUniform("NumBinRows", NumBinRows);
        SetUniform("MaxParticlesPerBin", MaxParticlesPerBin);
        SetUniform("StepSize", recipPerElem(Vector3(float(voxels.Width), float(voxels.Height), float(voxels.Depth))));
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, TightIntervalSurface[0].ColorTexture);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, TightIntervalSurface[1].ColorTexture);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, voxels.ColorTexture);
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, ParticleBins.ColorTexture);
        RenderMesh(ScreenQuad);

        // Blit the result to the backbuffer:
        glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, 0);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.86f, 0.86f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, cfg.Width, cfg.Height);
        glBindTexture(GL_TEXTURE_2D, RaycastDestination.ColorTexture);
        glGenerateMipmap(GL_TEXTURE_2D);
//glBindTexture(GL_TEXTURE_2D, ParticleBins.ColorTexture);
//glDisable(GL_BLEND);
        glUseProgram(BlitGpuParticlesProgram);
        SetUniform("StepSize", 1.0f/RaycastDestination.Width, 1.0f/RaycastDestination.Height);
        RenderMesh(ScreenQuad);
//glEnable(GL_BLEND);
    }
    else
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(0.86f, 0.86f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, cfg.Width, cfg.Height);
    }

    // Draw the tubes with a nice wireframe:
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(WireframeProgram);
    SetUniform("ModelviewProjection", ModelviewProjection);
    if (ShowWireframe)
    {
        RenderWireframe(PrimaryTube.Mesh);
        RenderWireframe(StentTube.Mesh);
        RenderWireframe(HelixTube.Mesh);
    }

    // Render the text:
    if (ShowHelp)
    {
        TexturePod messageTexture;
        size_t particles = PrimaryParticles.Particles.size() + HelixParticles.Particles.size();

        #define MESSAGE_TEXT \
            "%2.1f fps\n"\
            "%d x %d x %d\n"\
            "%d particles, %d triangles\n"\
            "V - Hide Voxels and Voxelization Time\n"\
            "W - Toggle Wireframe\n"\
            "P - Toggle Particles\n"\
            "B - Toggle Billboard Visualization\n"\
            "C - %s Particle Clipping\n"\
            "S - Toggle Surface Voxelization\n"\
            "? - Toggle Help"

        if (ShowVoxels)
        {
            messageTexture = OverlayTextf("Voxelized in %3.0f microseconds.\n" MESSAGE_TEXT,
                VoxelizationTime, Fips, voxels.Width, voxels.Height, voxels.Depth, particles,
                HelixTube.Mesh.TriangleCount + PrimaryTube.Mesh.TriangleCount + StentTube.Mesh.TriangleCount,
                ClipParticles ? "Disable" : "Enable");
        }
        else
        {
            messageTexture = OverlayTextf(MESSAGE_TEXT,
                Fips, voxels.Width, voxels.Height, voxels.Depth, particles,
                HelixTube.Mesh.TriangleCount + PrimaryTube.Mesh.TriangleCount + StentTube.Mesh.TriangleCount,
                ClipParticles ? "Disable" : "Enable");
        }

        glBindTexture(GL_TEXTURE_2D, messageTexture.Handle);
        glUseProgram(BlitProgram);
        RenderMesh(ScreenQuad);
    }

    // Render the billboards:
    if (ShowParticles && !SpatialBinning)
    {
        // Render two sets of particles to a half-size offscreen surface:
        glBindFramebuffer(GL_FRAMEBUFFER, ParticleSurface.Fbo);
        glViewport(0, 0, ParticleSurface.Width, ParticleSurface.Height);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(SurfaceVoxelization ? SurfaceParticleProgram : SolidParticleProgram);
        glBindTexture(GL_TEXTURE_3D, voxels.ColorTexture);
        SetUniform("PointSize", ParticleSize);
        SetUniform("Time", Time);
        SetUniform("CubeProjection", voxelProjection);
        SetUniform("ModelviewProjection", ModelviewProjection);
        SetUniform("Modelview", ViewMatrix.getUpper3x3());
        SetUniform("ShowBillboards", ShowBillboards);
        SetUniform("Darkness", 0.2f);
        SetUniform("StepSize", recipPerElem(Vector3(float(voxels.Width), float(voxels.Height), float(voxels.Depth))));
        glBlendFunc(GL_ONE, GL_ONE);
        RenderParticles(PrimaryParticles);
        RenderParticles(HelixParticles);

        glBindTexture(GL_TEXTURE_3D, 0);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, cfg.Width, cfg.Height);
        glBindTexture(GL_TEXTURE_2D, ParticleSurface.ColorTexture);
        glGenerateMipmap(GL_TEXTURE_2D);
        glUseProgram(BlitParticlesProgram);
        SetUniform("Darkness", 0.2f);
        SetUniform("StepSize", 1.0f/ParticleSurface.Width, 1.0f/ParticleSurface.Height);
        SetUniform("ClipParticles", ClipParticles);
        RenderMesh(ScreenQuad);
    }
}

void PezUpdate(unsigned int microseconds)
{
    float dt = microseconds * 0.000001f;

    // Dampen fluctuation in the FPS counter:
    float fips = 1.0f / dt;
    float alpha = 0.05f;
    if (Fips < 0)
        Fips++;
    else if (Fips == 0)
        Fips = fips;
    else
        Fips = fips * alpha + Fips * (1.0f - alpha);

    Time += dt;
    Trackball->Update(microseconds);

    PezConfig cfg = PezGetConfig();
    const float W = 0.4f;
    const float H = W * cfg.Height / cfg.Width;
    ProjectionMatrix = Matrix4::frustum(-W, W, -H, H, 2, 500);
    Point3 eye(0, 0, 7);
    Vector3 up(0, 1, 0);
    Point3 target(0, 0, 0);
    Matrix3 spin = Trackball->GetRotation();
    eye = Transform3(spin, Vector3(0,0,0)) * eye;
    up = spin * up;
    ViewMatrix = Matrix4::lookAt(eye, target, up);
    ModelviewProjection = ProjectionMatrix * ViewMatrix;

    AnimateTubes(PrimaryTube, HelixTube, dt);
    AdvectParticles(PrimaryParticles, dt, 3.0f);
    AdvectParticles(HelixParticles, dt, 5.0f);
    ParticleList pList[] = { PrimaryParticles.Particles, HelixParticles.Particles };
    FillGpuParticles(GpuParticles, ParticleSize, &pList[0], 2);

    glUseProgram(TransformParticlesProgram);
    static GLuint TransformedParticles = 0xffffffff;
    if (TransformedParticles == 0xffffffff)
    {
        const char* varyings[] = { "vPosition", "vRadius" };
        glGenBuffers(1, &TransformedParticles);
        glTransformFeedbackVaryings(TransformParticlesProgram, 2, varyings, GL_INTERLEAVED_ATTRIBS);
    }
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, TransformedParticles);
    glBeginTransformFeedback(GL_POINTS);
    RenderGpuParticles(GpuParticles);
    glEndTransformFeedback();
    glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);

    BinParticles(ParticleBins, GpuParticles.Particles, NumBinColumns, NumBinRows, MaxParticlesPerBin, ModelviewProjection);
}

void PezHandleMouse(int x, int y, int action)
{
    switch (action) {
        case PEZ_DOWN:
            Trackball->MouseDown(x, y);
            break;
        case PEZ_UP:
            Trackball->MouseUp(x, y);
            break;
        case PEZ_MOVE:
            Trackball->MouseMove(x, y);
            break;
        case PEZ_DOUBLECLICK:
            Trackball->ReturnHome();
            break;
    }
}

void PezHandleKey(char key)
{
    switch (toupper(key))
    {
        case 'V': ShowVoxels = !ShowVoxels; break;
        case 'P': ShowParticles = !ShowParticles; break;
        case 'W': ShowWireframe = !ShowWireframe; break;
        case '?': ShowHelp = !ShowHelp; break;
        case 'C': ClipParticles = !ClipParticles; break;
        //case 'B': ShowBillboards = !ShowBillboards; break;
        case 'B': SpatialBinning = !SpatialBinning; break;
        case 'S': SurfaceVoxelization = !SurfaceVoxelization; break;
        case 'D': DebugRaycast = !DebugRaycast; break;
    }
}
