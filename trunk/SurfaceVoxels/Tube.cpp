#include "Common.hpp"
#include <cmath>
#include <limits>

using namespace vmath;

void RedistributePath(const TubePath& src, TubePath& dest, float length, size_t lod)
{
    size_t sourceCount = lod;
    size_t destCount = lod;
        
    float sourceCursor = 0;
    float destCursor = 0;
    float destSegment = length / (destCount - 1);
    size_t sourceIndex = 0;

    dest.resize(destCount);
    TubePath::iterator pDest = dest.begin();
    TubePath::const_iterator pSrc = src.begin();
        
    for (size_t i = 0; i < destCount; i++) {
        
        Point3 s0 = (pSrc + sourceIndex)->Position;
        Point3 s1 = (pSrc + ((sourceIndex+1) % sourceCount))->Position;
        Vector3 g0 = (pSrc + sourceIndex)->Guide;
        Vector3 g1 = (pSrc + ((sourceIndex+1) % sourceCount))->Guide;
        float r0 = (pSrc + sourceIndex)->MinorRadius;
        float r1 = (pSrc + ((sourceIndex+1) % sourceCount))->MinorRadius;

        float nextDestCursor = destCursor + destSegment;
        float sourceSegment = vmath::dist(s0, s1);
        float nextSourceCursor = sourceCursor + sourceSegment;
            
        while (nextSourceCursor < nextDestCursor && sourceIndex < sourceCount - 2) {
            sourceCursor = nextSourceCursor;
            sourceIndex = sourceIndex + 1;
                
            s0 = (pSrc + sourceIndex)->Position;
            s1 = (pSrc + ((sourceIndex+1) % sourceCount))->Position;
            g0 = (pSrc + sourceIndex)->Guide;
            g1 = (pSrc + ((sourceIndex+1) % sourceCount))->Guide;
            r0 = (pSrc + sourceIndex)->MinorRadius;
            r1 = (pSrc + ((sourceIndex+1) % sourceCount))->MinorRadius;

            sourceSegment = vmath::dist(s0, s1);
            nextSourceCursor = sourceCursor + sourceSegment;
        }

        float t = (sourceSegment - nextSourceCursor + nextDestCursor) / sourceSegment;
        Point3 d = vmath::lerp(t, s0, s1);
        pDest->Position = d;
        pDest->Guide = vmath::lerp(t, g0, g1);
        pDest->MinorRadius = lerp(t, r0, r1);
        pDest++;
    
        destCursor = nextDestCursor;
        sourceCursor = nextSourceCursor - sourceSegment;
    }
}

static Point3 EvalSuperellipse(float n, float a, float b, float theta)
{
    float c = std::cos(theta);
    float s = std::sin(theta);
    float x = std::pow(std::abs(c), 2.0f / n) * a * sign(c);
    float y = std::pow(std::abs(s), 2.0f / n) * b * sign(s);
    float z = 0;
    return Point3(x, y, z);
}

static void SetPrimaryPath(TubePod& pod, float fraction = 1.0f)
{
    int nodeCount = pod.StackCount + 1;
    pod.Length = 0;
    float majorRadius = 1.0f;
    float theta = fraction < 1.0f ? (-0.5f * fraction * TwoPi) : 0.0f;
    float dtheta = fraction * TwoPi / float(nodeCount - 1);

    TubePath path;
    path.reserve(nodeCount);

    Point3 previous;
    for (int n = 0; n < nodeCount; theta += dtheta, n++) {

        PathNode node;

        if (theta > Pi/2 && theta < 3*Pi/2) {
            float n = 2 + pod.AnimationPercentage * 2;
            node.Position = EvalSuperellipse(n, majorRadius, majorRadius / 2, theta);
        } else
            node.Position = EvalSuperellipse(4, 0.5f, 0.5f, theta);

        node.Guide = Vector3(0, 0, 1);
        node.MinorRadius = 0.05f;

        float AneurysmWidth = pod.AnimationPercentage * 0.05f;
        int AneurysmHeight = 5;
        int shiftedN;
        if (n > nodeCount - AneurysmHeight)
            shiftedN = nodeCount - 1 - n;
        else
            shiftedN = n;

        float theta = shiftedN * Pi / AneurysmHeight;
        if (theta <= Pi) {
            float delta = AneurysmWidth * (cos(theta) + 1.0f);
            node.MinorRadius += delta;
        }

        node.Position[0] += 0.25f;
        path.push_back(node);

        if (n > 0)
            pod.Length += dist(node.Position, previous);

        previous = node.Position;
    }

    RedistributePath(path, pod.Path, pod.Length, path.size());
}

static void SetHelixPath(TubePod& pod, const TubePod& primary)
{
    int spiralCount = 20;
    int nodeCount = pod.StackCount + 1;
    pod.Length = 0;

    float majorRadius = 1.0f;
    float majorTheta = Pi / 3;
    float dmajorTheta = 4 * Pi / (3 * float(nodeCount - 1));

    float minorRadius = 0.1f;
    float minorTheta = 0;
    float dminorTheta = spiralCount * TwoPi / float(nodeCount - 1);

    TubePath path;
    path.reserve(nodeCount);

    Point3 previous;
    for (int n = 0; n < nodeCount; majorTheta += dmajorTheta, minorTheta += dminorTheta, n++) {
        
        size_t pN = size_t(majorTheta * primary.Path.size() / TwoPi);
        float t = fract(majorTheta * primary.Path.size() / TwoPi);

        Point3 spinePosition = lerp(t, primary.Path[pN].Position, primary.Path[pN + 1].Position);
        Vector3 guide = normalize(lerp(t, primary.Path[pN].Guide, primary.Path[pN + 1].Guide));
        Vector3 direction = normalize(primary.Path[pN + 1].Position - primary.Path[pN].Position);
        Vector3 binormal = cross(guide, direction);

        Matrix3 basis(guide, direction, binormal);

        float x = minorRadius * std::cos(minorTheta);
        float y = 0;
        float z = minorRadius * std::sin(minorTheta);

        PathNode minorNode;
        minorNode.Position = spinePosition + basis * Vector3(x, y, z);
        minorNode.Guide = direction;
        minorNode.MinorRadius = 0.02f;

        path.push_back(minorNode);
        if (n > 0)
            pod.Length += dist(minorNode.Position, previous);

        previous = minorNode.Position;
    }

    RedistributePath(path, pod.Path, pod.Length, path.size());
}

static void InitializeConnectivity(TubePod& pod)
{
    std::vector<GLuint> faces(pod.Mesh.TriangleCount * 3);
    GLuint* pDest = &faces.front();
    GLuint sliceStart = 0;
    for (GLuint nStack = 0; nStack < pod.StackCount; ++nStack)
    {
        for (GLuint nSlice = 0; nSlice < pod.SliceCount; ++nSlice)
        {
            GLuint nNextSlice = ((nSlice + 1) % pod.SliceCount);

            //   C -- o
            //   | \  |
            //   A -- B
            *pDest++ = sliceStart + nSlice;
            *pDest++ = sliceStart + nNextSlice;
            *pDest++ = sliceStart + nSlice + pod.SliceCount;

            //   A -- C
            //   | \  |
            //   o -- B
            *pDest++ = sliceStart + nNextSlice + pod.SliceCount;
            *pDest++ = sliceStart + nSlice + pod.SliceCount;
            *pDest++ = sliceStart + nNextSlice;
        }
        sliceStart += pod.SliceCount;
    }

    std::vector<GLuint> lines(pod.Mesh.LineCount * 2);
    pDest = &lines.front();
    sliceStart = 0;
    for (GLuint nStack = 0; nStack < pod.StackCount; ++nStack)
    {
        for (GLuint nSlice = 0; nSlice < pod.SliceCount; ++nSlice)
        {
            GLuint nNextSlice = ((nSlice + 1) % pod.SliceCount);

            //   C -- o
            //   |    |
            //   A -- B

            // AB
            *pDest++ = sliceStart + nSlice;
            *pDest++ = sliceStart + nNextSlice;

            // AC
            *pDest++ = sliceStart + nSlice;
            *pDest++ = sliceStart + nSlice + pod.SliceCount;
        }
        sliceStart += pod.SliceCount;
    }

    if (!pod.Loop)
    {
        for (GLuint nSlice = 0; nSlice < pod.SliceCount; ++nSlice)
        {
            GLuint nNextSlice = ((nSlice + 1) % pod.SliceCount);

            // AB
            *pDest++ = sliceStart + nSlice;
            *pDest++ = sliceStart + nNextSlice;
        }
    }

    glGenBuffers(1, &pod.Mesh.TriangleBuffer);
    glGenBuffers(1, &pod.Mesh.LineBuffer);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pod.Mesh.TriangleBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * faces.size(), &faces[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pod.Mesh.LineBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * lines.size(), &lines[0], GL_STATIC_DRAW);
}

static void TesselatePath(TubePod& tube)
{
    std::vector<Vector3> normals(tube.SliceCount);
    {
        float dtheta = 2.0f * Pi / tube.SliceCount;
        float theta = 0;
        std::vector<Vector3>::iterator pNormal = normals.begin();
        for (; pNormal != normals.end(); ++pNormal, theta += dtheta) {
            pNormal->setX(std::cos(theta));
            pNormal->setY(0);
            pNormal->setZ(std::sin(theta));
        }
    }

    tube.Verts.resize(tube.SliceCount * (tube.StackCount + 1));

    float* pFloatDest = &tube.Verts.front().Px;
    Matrix3 basis;
    for (GLuint nStack = 0; nStack < tube.StackCount + 1; ++nStack) {
        Point3 thisCenter = tube.Path[nStack].Position;

        if (nStack < tube.StackCount) {
            // Gather orientation vectors for this stack:
            Point3 nextCenter = tube.Path[nStack + 1].Position;
            Vector3 pathDirection = normalize(nextCenter - thisCenter);
            Vector3 pathNormal = tube.Path[nStack].Guide;

            // Form the change-of-basis matrix:
            Vector3 a = pathDirection;
            Vector3 b = pathNormal;
            Vector3 c = cross(a, b);
            basis = Matrix3(b, a, c);

        } else if (tube.Loop) {
            // Gather orientation vectors for this stack:
            Point3 nextCenter = tube.Path[1].Position;
            Vector3 pathDirection = normalize(nextCenter - thisCenter);
            Vector3 pathNormal = tube.Path[1].Guide;

            // Form the change-of-basis matrix:
            Vector3 a = pathDirection;
            Vector3 b = pathNormal;
            Vector3 c = cross(a, b);
            basis = Matrix3(b, a, c);
        }

        for (GLuint nSlice = 0; nSlice < tube.SliceCount; ++nSlice) {
            // Transform the position and normal:
            Vector3 normal = basis * normals[nSlice];
            Point3 position = thisCenter + tube.Path[nStack].MinorRadius * normal;

            tube.Mesh.MinCorner = minPerElem(tube.Mesh.MinCorner, position);
            tube.Mesh.MaxCorner = maxPerElem(tube.Mesh.MaxCorner, position);

            // Write out the position, normal, and length-so-far:
            *pFloatDest++ = position.getX();
            *pFloatDest++ = position.getY();
            *pFloatDest++ = position.getZ();
        }
    }

    // If it's circular, make sure the last stack meets up with the first stack:
    if (tube.Loop) {
        for (GLuint nSlice = 0; nSlice < tube.SliceCount; ++nSlice) {
            tube.Verts[tube.SliceCount * (tube.StackCount) + nSlice].Px = tube.Verts[nSlice].Px;
            tube.Verts[tube.SliceCount * (tube.StackCount) + nSlice].Py = tube.Verts[nSlice].Py;
            tube.Verts[tube.SliceCount * (tube.StackCount) + nSlice].Pz = tube.Verts[nSlice].Pz;
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, tube.Mesh.PositionsBuffer);
    glBufferData(GL_ARRAY_BUFFER, tube.Verts.size() * sizeof(TubeVertex), &tube.Verts[0].Px, GL_STATIC_DRAW);
}

static TubePod InitTube(TubePod& pod)
{
    pod.ElapsedTime = 0;
    pod.AnimationDuration = 0.3f;
    pod.AnimationPercentage = 0;
    pod.Mesh.TriangleBuffer = 0;
    pod.Mesh.NormalsBuffer = 0;
    pod.Mesh.TexCoordsBuffer = 0;
    pod.Mesh.MinCorner = Point3(std::numeric_limits<float>::max());
    pod.Mesh.MaxCorner = Point3(-std::numeric_limits<float>::max());
    pod.Mesh.VertexCount = pod.SliceCount * (pod.StackCount + 1);
    pod.Mesh.TriangleCount = pod.SliceCount * pod.StackCount * 2;
    pod.Mesh.LineCount = pod.StackCount * pod.SliceCount * 2;

    if (!pod.Loop)
        pod.Mesh.LineCount += pod.SliceCount;

    glGenBuffers(1, &pod.Mesh.PositionsBuffer);
    return pod;
}

TubePod CreatePrimary(int lod)
{
    TubePod pod;
    pod.Loop = true;
    pod.SliceCount = lod / 6;
    pod.StackCount = lod * 4 / 3 - 1;
    InitTube(pod);
    SetPrimaryPath(pod);
    InitializeConnectivity(pod);
    TesselatePath(pod);
    return pod;
}

TubePod CreateHelix(int lod, const TubePod& primary)
{
    TubePod pod;
    pod.Loop = false;
    pod.SliceCount = lod / 6;
    pod.StackCount = lod * 4 - 1;
    InitTube(pod);
    SetHelixPath(pod, primary);
    InitializeConnectivity(pod);
    TesselatePath(pod);
    return pod;

}

TubePod CreateStent(int lod)
{
    TubePod pod;
    pod.Loop = false;
    pod.SliceCount = lod / 6;
    pod.StackCount = lod * 4 / 3 - 1;
    InitTube(pod);
    SetPrimaryPath(pod, 0.1f);
    InitializeConnectivity(pod);
    TesselatePath(pod);
    return pod;
}

static void Animate(TubePod& pod, float dt)
{
    pod.ElapsedTime += dt;
    pod.AnimationPercentage = fmod(pod.ElapsedTime, 2.0f * pod.AnimationDuration) / pod.AnimationDuration;
    if (pod.AnimationPercentage > 1.0f)
        pod.AnimationPercentage = 1.0f - (pod.AnimationPercentage - 1.0f);
}

void AnimateTubes(TubePod& primary, TubePod& helix, float dt)
{
    Animate(primary, dt);
    Animate(helix, dt);

    SetPrimaryPath(primary);
    SetHelixPath(helix, primary);
    TesselatePath(primary);
    TesselatePath(helix);
}
