#include <assert.h>
#include <stdio.h>

#include <spdlog/spdlog.h>

#include <Lucky/Graphics/GraphicsDevice.hpp>
#include <Lucky/Graphics/ShaderProgram.hpp>
#include <Lucky/Graphics/Texture.hpp>
#include <Lucky/Utility/FileSystem.hpp>

#include "IncludeOpenGL.h"

namespace Lucky
{
    enum class ShaderParameterType
    {
        Texture = GL_SAMPLER_2D,
        Matrix = GL_FLOAT_MAT4,
        Float = GL_FLOAT,
        Float2 = GL_FLOAT_VEC2,
        Float3 = GL_FLOAT_VEC3,
        Float4 = GL_FLOAT_VEC4,
        Int = GL_INT,
        Int2 = GL_INT_VEC2,
        Int3 = GL_INT_VEC3,
        Int4 = GL_INT_VEC4,
    };

    struct ShaderParameterValue
    {
        ShaderParameterType parameterType;

        union {
            struct
            {
                uint32_t textureId;
                int32_t slot;
            };
            struct
            {
                float matrix[16];
            };
            struct
            {
                float f0, f1, f2, f3;
            };
            struct
            {
                int32_t i0, i1, i2, i3;
            };
        };
    };

    VertexShader::VertexShader(const std::string &fileName)
    {
        std::string vertexShaderString = ReadFile(fileName);
        if (vertexShaderString.empty())
        {
            spdlog::error("Failed to read vertex shader file: {}", fileName);
            throw;
        }

        Initialize(vertexShaderString.c_str(), (uint32_t)vertexShaderString.length());
    }

    VertexShader::VertexShader(const void *source, uint32_t sourceLength)
    {
        Initialize(source, sourceLength);
    }

    VertexShader::~VertexShader()
    {
        glDeleteShader(id);
    }

    void VertexShader::Initialize(const void *source, uint32_t sourceLength)
    {
        assert(source != nullptr);

        id = glCreateShader(GL_VERTEX_SHADER);
        if (id == 0)
        {
            spdlog::error("Failed to create OpenGL vertex shader id.");
            throw;
        }

        glShaderSource(id, 1, (GLchar **)&source, (GLint *)&sourceLength);
        glCompileShader(id);

        int status;
        glGetShaderiv(id, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE)
        {
            char infoLog[1024];
            int logLength;
            glGetShaderInfoLog(id, 1024, &logLength, infoLog);
            glDeleteShader(id);

            spdlog::error("Vertex shader compilation failed:\n{}", infoLog);
            throw;
        }
    }

    FragmentShader::FragmentShader(const std::string &fileName)
    {
        std::string fragmentShaderString = ReadFile(fileName);
        if (fragmentShaderString.empty())
        {
            spdlog::error("Failed to read fragment shader file: {}", fileName);
            throw;
        }

        Initialize(fragmentShaderString.c_str(), (uint32_t)fragmentShaderString.length());
    }

    FragmentShader::FragmentShader(const void *source, uint32_t sourceLength)
    {
        Initialize(source, sourceLength);
    }

    FragmentShader::~FragmentShader()
    {
        glDeleteShader(id);
    }

    void FragmentShader::Initialize(const void *source, uint32_t sourceLength)
    {
        assert(source != nullptr);

        id = glCreateShader(GL_FRAGMENT_SHADER);
        if (id == 0)
        {
            spdlog::error("Failed to create OpenGL fragment shader id.");
            throw;
        }

        glShaderSource(id, 1, (GLchar **)&source, (GLint *)&sourceLength);
        glCompileShader(id);

        int status;
        glGetShaderiv(id, GL_COMPILE_STATUS, &status);
        if (status != GL_TRUE)
        {
            char infoLog[1024];
            int logLength;
            glGetShaderInfoLog(id, 1024, &logLength, infoLog);
            glDeleteShader(id);

            spdlog::error("Fragment shader compilation failed:\n{}", infoLog);
            throw;
        }
    }

    ShaderProgram::ShaderProgram(std::shared_ptr<GraphicsDevice> graphicsDevice, const VertexShader &vertexShader,
        const FragmentShader &fragmentShader)
        : graphicsDevice(graphicsDevice)
    {
        assert(graphicsDevice);

        id = glCreateProgram();
        if (id == 0)
        {
            spdlog::error("Failed to create OpenGL shader program id.");
            throw;
        }

        glAttachShader(id, vertexShader.GetShaderId());
        glAttachShader(id, fragmentShader.GetShaderId());

        glLinkProgram(id);

        int status;
        glGetProgramiv(id, GL_LINK_STATUS, &status);
        if (status != GL_TRUE)
        {
            char infoLog[1024];
            int logLength;
            glGetProgramInfoLog(id, 1024, &logLength, infoLog);
            glDeleteProgram(id);

            spdlog::error("Shader program linking failed:\n{}", infoLog);
            throw;
        }

        int uniformCount;
        glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &uniformCount);

        for (int i = 0; i < uniformCount; i++)
        {
            int size;
            GLenum type;
            GLchar name[256];

            glGetActiveUniform(id, i, 256, nullptr, &size, &type, name);
            parameters[name] = {glGetUniformLocation(id, name), type};
        }

        int attributeCount;
        glGetProgramiv(id, GL_ACTIVE_ATTRIBUTES, &attributeCount);

        for (int i = 0; i < attributeCount; i++)
        {
            int size;
            GLenum type;
            GLchar name[256];

            glGetActiveAttrib(id, i, 256, nullptr, &size, &type, name);
            attributes[name] = {glGetAttribLocation(id, name), type};
        }

        auto positionType = GetAttributeType("position");
        if (positionType != 0 && positionType != GL_FLOAT_VEC4)
        {
            spdlog::error("Shader program has an invalid type for attribute: position\n");
        }

        auto colorType = GetAttributeType("color");
        if (colorType != 0 && colorType != GL_FLOAT_VEC4)
        {
            spdlog::error("Shader program has an invalid type for attribute: color\n");
        }

        auto texcoordType = GetAttributeType("texcoord");
        if (texcoordType != 0 && texcoordType != GL_FLOAT_VEC2)
        {
            spdlog::error("Shader program has an invalid type for attribute: texcoord\n");
        }

        auto projectionMatrixType = GetParameterType("ProjectionMatrix");
        if (projectionMatrixType != 0 && projectionMatrixType != GL_FLOAT_MAT4)
        {
            spdlog::error("Shader program has an invalid type for uniform: ProjectionMatrix\n");
        }
    }

    ShaderProgram::~ShaderProgram()
    {
        glDeleteShader(id);
    }

    void ShaderProgram::SetParameter(const std::string &name, const Texture &texture, int slotNumber)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Texture;
        spv.textureId = texture.GetTextureId();
        spv.slot = slotNumber;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(const std::string &name, const glm::mat4 &matrix)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Matrix;
        memcpy(spv.matrix, &matrix[0][0], sizeof(float) * 16);
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(const std::string &name, const float value)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Float;
        spv.f0 = value;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(const std::string &name, const float value0, const float value1)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Float2;
        spv.f0 = value0;
        spv.f1 = value1;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(
        const std::string &name, const float value0, const float value1, const float value2)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Float3;
        spv.f0 = value0;
        spv.f1 = value1;
        spv.f2 = value2;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(
        const std::string &name, const float value0, const float value1, const float value2, const float value3)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Float4;
        spv.f0 = value0;
        spv.f1 = value1;
        spv.f2 = value2;
        spv.f3 = value3;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(const std::string &name, const glm::vec2 &value)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Float2;
        spv.f0 = value.x;
        spv.f1 = value.y;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(const std::string &name, const glm::vec3 &value)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Float3;
        spv.f0 = value.x;
        spv.f1 = value.y;
        spv.f2 = value.z;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(const std::string &name, const glm::vec4 &value)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Float4;
        spv.f0 = value.x;
        spv.f1 = value.y;
        spv.f2 = value.z;
        spv.f3 = value.w;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(const std::string &name, const int value)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Int;
        spv.i0 = value;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(const std::string &name, const int value0, const int value1)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Int2;
        spv.i0 = value0;
        spv.i1 = value1;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(const std::string &name, const int value0, const int value1, const int value2)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Int3;
        spv.i0 = value0;
        spv.i1 = value1;
        spv.i2 = value2;
        parameterValues[name] = spv;
    }

    void ShaderProgram::SetParameter(
        const std::string &name, const int value0, const int value1, const int value2, const int value3)
    {
        ShaderParameterValue spv;
        spv.parameterType = ShaderParameterType::Int4;
        spv.i0 = value0;
        spv.i1 = value1;
        spv.i2 = value2;
        spv.i3 = value3;
        parameterValues[name] = spv;
    }

    void ShaderProgram::ClearParameter(const std::string &name)
    {
        const auto findResult = parameterValues.find(name);
        if (findResult != parameterValues.end())
        {
            parameters.erase(name);
        }
    }

    void ShaderProgram::ApplyParameters()
    {
        for (const auto &kvp : parameterValues)
        {
            auto &name = kvp.first;
            auto &parameterValue = kvp.second;

            auto findResult = parameters.find(name);

            if (findResult == parameters.end())
            {
                continue;
            }

            ShaderParameter &parameter = findResult->second;

            if (parameter.type != (uint32_t)parameterValue.parameterType)
            {
                spdlog::error("Shader program parameter type mismatch: {}", name);
                throw;
            }

            switch (parameterValue.parameterType)
            {
            case ShaderParameterType::Texture:
                glActiveTexture(GL_TEXTURE0 + parameterValue.slot);
                glBindTexture(GL_TEXTURE_2D, parameterValue.textureId);
                glUniform1i(parameter.location, parameterValue.slot);
                break;

            case ShaderParameterType::Matrix:
                glUniformMatrix4fv(parameter.location, 1, GL_FALSE, parameterValue.matrix);
                break;

            case ShaderParameterType::Float:
                glUniform1f(parameter.location, parameterValue.f0);
                break;

            case ShaderParameterType::Float2:
                glUniform2f(parameter.location, parameterValue.f0, parameterValue.f1);
                break;

            case ShaderParameterType::Float3:
                glUniform3f(parameter.location, parameterValue.f0, parameterValue.f1, parameterValue.f2);
                break;

            case ShaderParameterType::Float4:
                glUniform4f(
                    parameter.location, parameterValue.f0, parameterValue.f1, parameterValue.f2, parameterValue.f3);
                break;

            case ShaderParameterType::Int:
                glUniform1i(parameter.location, parameterValue.i0);
                break;

            case ShaderParameterType::Int2:
                glUniform2i(parameter.location, parameterValue.i0, parameterValue.i1);
                break;

            case ShaderParameterType::Int3:
                glUniform3i(parameter.location, parameterValue.i0, parameterValue.i1, parameterValue.i2);
                break;

            case ShaderParameterType::Int4:
                glUniform4i(
                    parameter.location, parameterValue.i0, parameterValue.i1, parameterValue.i2, parameterValue.i3);
                break;
            }
        }
    }

    int32_t ShaderProgram::GetParameterLocation(const std::string &name)
    {
        return (parameters.find(name) == parameters.end()) ? -1 : parameters[name].location;
    }

    uint32_t ShaderProgram::GetParameterType(const std::string &name)
    {
        return (parameters.find(name) == parameters.end()) ? 0 : parameters[name].type;
    }

    int32_t ShaderProgram::GetAttributeLocation(const std::string &name)
    {
        return (attributes.find(name) == attributes.end()) ? -1 : attributes[name].location;
    }

    uint32_t ShaderProgram::GetAttributeType(const std::string &name)
    {
        return (attributes.find(name) == attributes.end()) ? 0 : attributes[name].type;
    }
} // namespace Lucky
