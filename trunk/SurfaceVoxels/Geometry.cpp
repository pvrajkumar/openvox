#include <string>
#include "Common.hpp"

using std::string;

MeshPod CreateQuad(float left, float top, float right, float bottom)
{
    MeshPod pod = {0};
    pod.VertexCount = 4;
    pod.TriangleCount = 2;

    float positions[] = {
        left, top, 0,
        right, top, 0,
        right, bottom, 0,
        left, bottom, 0,
    };

    glGenBuffers(1, &pod.PositionsBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, pod.PositionsBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);

    float texcoords[] = {
        0, 0,
        1, 0,
        1, 1,
        0, 1,
    };

    glGenBuffers(1, &pod.TexCoordsBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, pod.TexCoordsBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords), texcoords, GL_STATIC_DRAW);

    int faces[] = { 3, 2, 1, 1, 0, 3 };

    glGenBuffers(1, &pod.TriangleBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pod.TriangleBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(faces), faces, GL_STATIC_DRAW);

    return pod;
}

MeshPod CreateQuad()
{
    return CreateQuad(-1, +1, +1, -1);
}

void RenderMesh(MeshPod mesh)
{
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error.");
    PezCheckCondition(mesh.TriangleBuffer != 0 && mesh.PositionsBuffer != 0, "Invalid mesh.");

    glBindBuffer(GL_ARRAY_BUFFER, mesh.PositionsBuffer);
    glVertexAttribPointer(SlotPosition, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    glEnableVertexAttribArray(SlotPosition);

    if (mesh.NormalsBuffer) {
        glBindBuffer(GL_ARRAY_BUFFER, mesh.NormalsBuffer);
        glVertexAttribPointer(SlotNormal, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
        glEnableVertexAttribArray(SlotNormal);
    }

    if (mesh.TexCoordsBuffer) {
        glBindBuffer(GL_ARRAY_BUFFER, mesh.TexCoordsBuffer);
        glVertexAttribPointer(SlotTexCoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
        glEnableVertexAttribArray(SlotTexCoord);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.TriangleBuffer);
    glDrawElements(GL_TRIANGLES, mesh.TriangleCount * 3, GL_UNSIGNED_INT, 0);
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error.");

    glDisableVertexAttribArray(SlotPosition);
    glDisableVertexAttribArray(SlotNormal);
    glDisableVertexAttribArray(SlotTexCoord);
}

void RenderMeshInstanced(MeshPod mesh, int instanceCount)
{
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error.");
    PezCheckCondition(mesh.TriangleBuffer != 0 && mesh.PositionsBuffer != 0, "Invalid mesh.");

    glBindBuffer(GL_ARRAY_BUFFER, mesh.PositionsBuffer);
    glVertexAttribPointer(SlotPosition, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    glEnableVertexAttribArray(SlotPosition);

    if (mesh.NormalsBuffer) {
        glBindBuffer(GL_ARRAY_BUFFER, mesh.NormalsBuffer);
        glVertexAttribPointer(SlotNormal, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
        glEnableVertexAttribArray(SlotNormal);
    }

    if (mesh.TexCoordsBuffer) {
        glBindBuffer(GL_ARRAY_BUFFER, mesh.TexCoordsBuffer);
        glVertexAttribPointer(SlotTexCoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
        glEnableVertexAttribArray(SlotTexCoord);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.TriangleBuffer);
    glDrawElementsInstanced(GL_TRIANGLES, 3 * mesh.TriangleCount, GL_UNSIGNED_INT, 0, instanceCount);
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error.");

    glDisableVertexAttribArray(SlotPosition);
    glDisableVertexAttribArray(SlotNormal);
    glDisableVertexAttribArray(SlotTexCoord);
}

MeshPod CreateCube()
{
#define O -1,
#define X +1,
    float positions[] = { O O O O O X O X O O X X X O O X O X X X O X X X };
#undef O
#undef X

    MeshPod mesh;

    unsigned int indices[] = {
        7, 3, 1, 1, 5, 7, // Z+
        0, 2, 6, 6, 4, 0, // Z-
        6, 2, 3, 3, 7, 6, // Y+
        1, 0, 4, 4, 5, 1, // Y-
        3, 2, 0, 0, 1, 3, // X-
        4, 6, 7, 7, 5, 4, // X+
    };

    // Create the VBO for positions:
    {
        GLsizeiptr size = sizeof(positions);
        glGenBuffers(1, &mesh.PositionsBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.PositionsBuffer);
        glBufferData(GL_ARRAY_BUFFER, size, positions, GL_STATIC_DRAW);
    }

    // Create the VBO for indices:
    {
        GLsizeiptr size = sizeof(indices);
        glGenBuffers(1, &mesh.TriangleBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.TriangleBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indices, GL_STATIC_DRAW);
    }

    mesh.NormalsBuffer = 0;
    mesh.TexCoordsBuffer = 0;
    mesh.TriangleCount = (sizeof(indices) / sizeof(indices[0])) / 3;
    mesh.VertexCount = 3 * sizeof(positions) / sizeof(positions[0]);

    return mesh;
}

void RenderWireframe(MeshPod mesh)
{
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error.");
    PezCheckCondition(mesh.LineBuffer != 0 && mesh.PositionsBuffer != 0, "Invalid mesh.");

    glBindBuffer(GL_ARRAY_BUFFER, mesh.PositionsBuffer);
    glVertexAttribPointer(SlotPosition, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    glEnableVertexAttribArray(SlotPosition);

    if (mesh.NormalsBuffer) {
        glBindBuffer(GL_ARRAY_BUFFER, mesh.NormalsBuffer);
        glVertexAttribPointer(SlotNormal, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
        glEnableVertexAttribArray(SlotNormal);
    }

    if (mesh.TexCoordsBuffer) {
        glBindBuffer(GL_ARRAY_BUFFER, mesh.TexCoordsBuffer);
        glVertexAttribPointer(SlotTexCoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
        glEnableVertexAttribArray(SlotTexCoord);
    }

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.LineBuffer);
    glDrawElements(GL_LINES, mesh.LineCount * 2, GL_UNSIGNED_INT, 0);
    PezCheckCondition(glGetError() == GL_NO_ERROR, "OpenGL error.");

    glDisableVertexAttribArray(SlotPosition);
    glDisableVertexAttribArray(SlotNormal);
    glDisableVertexAttribArray(SlotTexCoord);
}
