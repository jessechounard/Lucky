#pragma once

#include <stdint.h>

namespace Lucky
{
    enum class VertexBufferType
    {
        Static,
        Dynamic,
    };

    struct ShaderProgram;
    struct Vertex;

    struct VertexBuffer
    {
      public:
        VertexBuffer(VertexBufferType vertexBufferType, uint32_t maximumVertices);
        VertexBuffer(const VertexBuffer &) = delete;
        ~VertexBuffer();

        VertexBuffer &operator=(const VertexBuffer &) = delete;

        void SetVertexData(ShaderProgram &shaderProgram, Vertex *vertices, uint32_t vertexCount);

        uint32_t GetArrayId() const
        {
            return vertexArrayId;
        }

        uint32_t GetBufferId() const
        {
            return vertexBufferId;
        }

      private:
        uint32_t vertexArrayId;
        uint32_t vertexBufferId;
    };
} // namespace Lucky
