#include <assert.h>

#include <Lucky/Graphics/ShaderProgram.hpp>
#include <Lucky/Graphics/VertexBuffer.hpp>
#include <Lucky/Math/Vertex.hpp>

#include "IncludeOpenGL.h"

namespace Lucky
{
    VertexBuffer::VertexBuffer(VertexBufferType vertexBufferType, uint32_t maximumVertices)
    {
        assert(maximumVertices > 0);

        glGenVertexArrays(1, &vertexArrayId);
        glBindVertexArray(vertexArrayId);
        glEnableVertexAttribArray(vertexArrayId);
        glGenBuffers(1, &vertexBufferId);

        GLenum bufferUsage = (vertexBufferType == VertexBufferType::Static) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;

        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);
        glBufferData(GL_ARRAY_BUFFER, maximumVertices * sizeof(Vertex), nullptr, bufferUsage);
    }

    VertexBuffer::~VertexBuffer()
    {
        glDeleteBuffers(1, &vertexBufferId);
        glDeleteVertexArrays(1, &vertexArrayId);
    }

    void VertexBuffer::SetVertexData(ShaderProgram &shaderProgram, Vertex *vertices, uint32_t vertexCount)
    {
        assert(vertices != nullptr);
        assert(vertexCount > 0);

        glBindVertexArray(vertexArrayId);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferId);

        glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(Vertex), vertices);

        auto positionLocation = shaderProgram.GetAttributeLocation("position");
        if (positionLocation != -1)
        {
            glVertexAttribPointer(positionLocation, 2, GL_FLOAT, 0, sizeof(Vertex), 0);
            glEnableVertexAttribArray(positionLocation);
        }

        auto colorLocation = shaderProgram.GetAttributeLocation("color");
        if (colorLocation != -1)
        {
            glVertexAttribPointer(colorLocation, 4, GL_FLOAT, 0, sizeof(Vertex), (void *)(sizeof(float) * 4));
            glEnableVertexAttribArray(colorLocation);
        }

        auto texcoordLocation = shaderProgram.GetAttributeLocation("texcoord");
        if (texcoordLocation != -1)
        {
            glVertexAttribPointer(texcoordLocation, 2, GL_FLOAT, 0, sizeof(Vertex), (void *)(sizeof(float) * 2));
            glEnableVertexAttribArray(texcoordLocation);
        }
    }
} // namespace Lucky