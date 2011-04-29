#include "Common.hpp"
#include <vmath.hpp>
#include <cmath>
#include <vector>
#include <pez.h>
#include <list>
#include <map>
#include <set>

using namespace vmath;

void AdvectParticles(ParticleSystem& system, float dt, float speed)
{
    system.Time += dt;

    for (ParticleList::iterator p = system.Particles.begin(); p != system.Particles.end(); ++p) {
        
        float tob = p->ToB;

        float linearPosition = fmod(tob + system.Time, speed);
        float nodePosition = linearPosition * system.TravelTube->Path.size() / speed;
        int nodeA = (int) floor(nodePosition);
        int nodeB = ((int) ceil(nodePosition)) % system.TravelTube->Path.size();
        float weight = fract(nodePosition);

        if (!system.TravelTube->Loop) {
            int n = 5;
            int upper = (int) (system.TravelTube->Path.size() - n);
            if (nodeA < n) nodeA = n;
            if (nodeA > upper) nodeA = upper;
            if (nodeB < n) nodeB = n;
            if (nodeB > upper) nodeB = upper;
        }

        Point3 positionA = system.TravelTube->Path[nodeA].Position;
        Point3 positionB = system.TravelTube->Path[nodeB].Position;

        if (!system.TravelTube->Loop && nodeB < nodeA)
            positionA = positionB;

        Point3 position = lerp(weight, positionA, positionB);

        // Compute a hypothetical "previous" position along the tube current deformation:
        linearPosition = fmod(tob + system.Time - dt, speed);
        nodePosition = linearPosition * system.TravelTube->Path.size() / speed;
        nodeA = (int) floor(nodePosition);
        nodeB = ((int) ceil(nodePosition)) % system.TravelTube->Path.size();
        positionA = system.TravelTube->Path[nodeA].Position;
        positionB = system.TravelTube->Path[nodeB].Position;
        weight = fract(nodePosition);
        Point3 previousPosition = lerp(weight, positionA, positionB);

        Vector3 velocity = normalize(position - previousPosition);

        p->Px = position[0]; p->Py = position[1]; p->Pz = position[2];
        p->Vx = velocity[0]; p->Vy = velocity[1]; p->Vz = velocity[2];
    }

    if (system.Particles.empty())
        return;

    glBindBuffer(GL_ARRAY_BUFFER, system.VertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Particle) * system.Particles.size(), &system.Particles[0], GL_STATIC_DRAW);
}

ParticleSystem CreateParticles(int count, TubePod& tube, float spread)
{
    ParticleSystem system;
    system.Particles.resize(count);
    system.Time = 0;
    system.TravelTube = &tube;

    for (ParticleList::iterator i = system.Particles.begin(); i != system.Particles.end(); ++i) {
        i->Px = 1.0f;
        i->Py = 0;
        i->Pz = 0;
        i->ToB = spread * (rand() % 1000) / float(1000);
        i->Vx = 0;
        i->Vy = 1;
        i->Vz = 0;
    }

    glGenBuffers(1, &system.VertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, system.VertexBuffer);

    if (count)
        glBufferData(GL_ARRAY_BUFFER, sizeof(Particle) * system.Particles.size(), &system.Particles[0], GL_STATIC_DRAW);

    return system;
}

void RenderParticles(ParticleSystem& system)
{
    glBindBuffer(GL_ARRAY_BUFFER, system.VertexBuffer);

    GLvoid* offsetBirthTime = (GLvoid*) (sizeof(float) * 3);
    GLvoid* offsetVelocity = (GLvoid*) (sizeof(float) * 4);

    glVertexAttribPointer(SlotPosition, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), 0);
    glVertexAttribPointer(SlotBirthTime, 1, GL_FLOAT, GL_FALSE, sizeof(Particle), offsetBirthTime);
    glVertexAttribPointer(SlotVelocity, 3, GL_FLOAT, GL_FALSE, sizeof(Particle), offsetVelocity);

    glEnableVertexAttribArray(SlotPosition);
    glEnableVertexAttribArray(SlotBirthTime);
    glEnableVertexAttribArray(SlotVelocity);

    glDrawArrays(GL_POINTS, 0, system.Particles.size());

    glDisableVertexAttribArray(SlotPosition);
    glDisableVertexAttribArray(SlotBirthTime);
    glDisableVertexAttribArray(SlotVelocity);
}

#define GetBin(row, col) (row * numBinColumns + col)

void BinParticles(const SurfacePod& binningSurface, const GpuParticleList& particles, int numBinColumns, int numBinRows, size_t maxParticlesPerBin, Matrix4 mvp)
{
    typedef std::list<GpuParticle> Bin;
    typedef std::map<int, Bin> BinMap;
    BinMap binMap;

    for (GpuParticleList::const_iterator pParticle = particles.begin(); pParticle != particles.end(); ++pParticle)
    {
        Point3 center(pParticle->Px, pParticle->Py, pParticle->Pz);
        float r = pParticle->Radius;

        // Build the AABB for this particle:
        Vector3 i(r, 0, 0), j(0, r, 0), k(0, 0, r);
        Point3 corners[8] =
        {
            center + i + j + k,
            center + i + j - k,
            center - i + j - k,
            center - i + j + k,
            center + i - j + k,
            center + i - j - k,
            center - i - j - k,
            center - i - j + k,
        };

        int minX = 1000;
        int maxX = -1000;
        int minY = 1000;
        int maxY = -1000;

        // Find the screen-space 2D AABB of the post-transformed 3D AABB:
        for (int cornerIndex = 0; cornerIndex < sizeof(corners) / sizeof(corners[0]); ++cornerIndex)
        {
            Vector4 ndcCenter = mvp * corners[cornerIndex];

            float centerX = 0.5f * (1 + ndcCenter[0] / ndcCenter[3]);
            float centerY = 0.5f * (1 + ndcCenter[1] / ndcCenter[3]);

            int i = int(centerX * numBinColumns);
            int j = int(centerY * numBinRows);

            minX = std::min(minX, i); minY = std::min(minY, j);
            maxX = std::max(maxX, i); maxY = std::max(maxY, j);
        }

        //minX = 0; maxX = NumBinColumns - 1;
        //minY = 0; maxY = NumBinRows - 1;

        // Fill every bin that intersects with the 2D AABB:
        std::set<int> cornerBins;
        for (int i = minX; i <= maxX; i++)
        {
            for (int j = minY; j <= maxY; j++)
            {
                if (i >= 0 && i < numBinColumns && j >= 0 && j < numBinRows)
                {
                    int binIndex = GetBin(j, i);
                    if (cornerBins.find(binIndex) == cornerBins.end())
                    {
                        cornerBins.insert(binIndex);
                        if (binMap[binIndex].size() < maxParticlesPerBin)
                            binMap[binIndex].push_back(*pParticle);
                    }
                }
            }
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, binningSurface.Pbo);
    float* mappedSurface = (float*) glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);

    float* pRow = mappedSurface;
    for (int row = 0; row < numBinRows; ++row)
    {
        float* pCol = pRow;
        for (int col = 0; col < numBinColumns; ++col)
        {
            float* pDestParticle = pCol;
            BinMap::const_iterator pBin = binMap.find(GetBin(row, col));
            if (pBin != binMap.end())
            {
                Bin::const_iterator pSrcParticle = pBin->second.begin();
                for (; pSrcParticle != pBin->second.end(); ++pSrcParticle)
                {
                    *pDestParticle++ = pSrcParticle->Px;
                    *pDestParticle++ = pSrcParticle->Py;
                    *pDestParticle++ = pSrcParticle->Pz;
                    *pDestParticle++ = pSrcParticle->Radius;
                }
            }

            *pDestParticle++ = 0; *pDestParticle++ = 0; *pDestParticle++ = 0; *pDestParticle++ = 0;

#define DEBUG_BINS 0
#if DEBUG_BINS
            while (pDestParticle < pCol + 4 * (MaxParticlesPerBin + 1))
            {
                *pDestParticle++ = float(row) / NumBinRows;
                *pDestParticle++ = float(col) / NumBinColumns;
                *pDestParticle++ = 0.25f;
                *pDestParticle++ = 1;
            }
#endif

            pCol += 4 * (maxParticlesPerBin + 1);
        }
        pRow += 4 * numBinColumns * (maxParticlesPerBin + 1);
    }

    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindTexture(GL_TEXTURE_2D, binningSurface.ColorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, binningSurface.Width, binningSurface.Height, 0, GL_RGBA, GL_FLOAT, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

void RenderGpuParticles(GpuParticleSystem& system)
{
    glBindBuffer(GL_ARRAY_BUFFER, system.VertexBuffer);

    GLvoid* offsetRadius = (GLvoid*) (sizeof(float) * 3);

    glVertexAttribPointer(SlotPosition, 3, GL_FLOAT, GL_FALSE, sizeof(GpuParticle), 0);
    glVertexAttribPointer(SlotRadius, 1, GL_FLOAT, GL_FALSE, sizeof(GpuParticle), offsetRadius);

    glEnableVertexAttribArray(SlotPosition);
    glEnableVertexAttribArray(SlotRadius);

    glDrawArrays(GL_POINTS, 0, system.Particles.size());

    glDisableVertexAttribArray(SlotPosition);
    glDisableVertexAttribArray(SlotRadius);
}

GpuParticleSystem CreateGpuParticles(size_t count)
{
    GpuParticleSystem system;
    system.Particles.resize(count);
    system.Time = 0;
    glGenBuffers(1, &system.VertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, system.VertexBuffer);

    if (count)
    {
        glBufferData(GL_ARRAY_BUFFER, sizeof(GpuParticle) * count, &system.Particles[0], GL_STATIC_DRAW);
    }

    return system;
}

void FillGpuParticles(GpuParticleSystem& dest, float radius, ParticleList* source, size_t count)
{
    GpuParticleList::iterator pDestParticle = dest.Particles.begin();
    while (count--)
    {
        for (ParticleList::const_iterator pParticle = source->begin(); pParticle != source->end(); ++pParticle, ++pDestParticle)
        {
            pDestParticle->Px = pParticle->Px;
            pDestParticle->Py = pParticle->Py;
            pDestParticle->Pz = pParticle->Pz;
            pDestParticle->Radius = radius;
        }
        ++source;
    }
}