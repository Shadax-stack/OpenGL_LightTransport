#include "Mesh.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <glm/gtx/norm.hpp>

#include "Vertex.h"
#include "Triangle.h"
#include "TriangleIndexing.h"

#include <vector>
#include <iostream>

template<typename T>
size_t GetVectorSizeBytes(const std::vector<T>& Vector) {
    return Vector.size() * sizeof(T);
}

void Mesh::LoadMesh(const char* File) {
    Assimp::Importer Importer;

    // Turn off smooth normals for path tracing to prevent "broken" BRDFs and energy loss. 
    const aiScene* Scene = Importer.ReadFile(File, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    // TODO: memory reservation

    std::vector          <Vertex> Vertices;
    std::vector<TriangleIndexData> Indices;

    for (uint32_t MeshIndex = 0; MeshIndex < Scene->mNumMeshes; MeshIndex++) {
        const aiMesh* Mesh = Scene->mMeshes[MeshIndex];

        Vertices.reserve(Vertices.size() + Mesh->mNumVertices);
        for (uint32_t VertexIndex = 0; VertexIndex < Mesh->mNumVertices; VertexIndex++) {
            Vertex CurrentVertex;

            CurrentVertex.Position = glm::vec3(Mesh->mVertices[VertexIndex].x, Mesh->mVertices[VertexIndex].y, Mesh->mVertices[VertexIndex].z);
            CurrentVertex.Normal   = glm::vec3(Mesh->mNormals[VertexIndex].x , Mesh->mNormals[VertexIndex].y , Mesh->mNormals[VertexIndex].z );
            if(Mesh->mTextureCoords[0])
                CurrentVertex.TextureCoordinates = glm::vec2(Mesh->mTextureCoords[0][VertexIndex].x, Mesh->mTextureCoords[0][VertexIndex].y);

            Vertices.push_back(CurrentVertex);
        }

        Indices.reserve(Indices.size() + Mesh->mNumFaces);
        for (uint32_t FaceIndex = 0; FaceIndex < Mesh->mNumFaces; FaceIndex++) {
            const aiFace& Face = Mesh->mFaces[FaceIndex];

            TriangleIndexData CurrentIndexData;
            for (uint32_t ElementIndex = 0; ElementIndex < Face.mNumIndices; ElementIndex++) {
                CurrentIndexData[ElementIndex] = Face.mIndices[ElementIndex];
            }

            Indices.push_back(CurrentIndexData);
        }
    }

    Importer.FreeScene();

    LoadMesh(Vertices, Indices);
}

void Mesh::LoadMesh(const std::vector<Vertex> Vertices, const std::vector<TriangleIndexData> Indices) {
    //std::cout << "Prim Counter: " << Indices.size() << std::endl;

    VertexBuffer.CreateBinding(BUFFER_TARGET_ARRAY);
    VertexBuffer.UploadData(GetVectorSizeBytes(Vertices), Vertices.data());

    ElementBuffer.CreateBinding(BUFFER_TARGET_ARRAY);
    ElementBuffer.UploadData(GetVectorSizeBytes(Indices), Indices.data());

    BufferTexture.Vertices.CreateBinding();
    BufferTexture.Vertices.SelectBuffer(&VertexBuffer, GL_RGBA32F);

    BufferTexture.Indices.CreateBinding();
    BufferTexture.Indices.SelectBuffer(&ElementBuffer, GL_RGB32UI);

    BVH.ConstructAccelerationStructure(Vertices, Indices);
}

void Mesh::LoadTexture(const char* Path) {
    if (Material.Diffuse.AttemptPreload(Path)) {
        Material.Diffuse.CreateBinding(); // Bind just in case to not break future code. I will remove this once I rewrite everything
    }
    else {
        Material.Diffuse.CreateBinding();
        Material.Diffuse.LoadTexture(Path);
    }
}

void Mesh::SetColor(const glm::vec3& Color) {
    Material.Diffuse.CreateBinding();
    Material.Diffuse.LoadData(GL_RGB32F, GL_RGB, GL_FLOAT, 1, 1, (void*)&Color);
}