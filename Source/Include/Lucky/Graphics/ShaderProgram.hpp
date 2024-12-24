#pragma once

#include <map>
#include <memory>
#include <stdint.h>
#include <string>

#include <glm/glm.hpp>

namespace Lucky
{
    // Shaders can have almost any input parameters, but there are a few that
    // we have predefined names for with BatchRenderer
    //
    // These three are passed in the vertex data (one per vertex)
    // position: vec4
    // color: vec4
    // texcoord: vec2
    //
    // These are passed with the draw call (one for all vertices)
    // ProjectionMatrix: mat4
    // TextureSampler: sampler2D (in slot 0)

    struct GraphicsDevice;
    struct Texture;

    struct VertexShader
    {
      public:
        VertexShader(const std::string &fileName);
        VertexShader(const void *source, uint32_t sourceLength);
        VertexShader(const VertexShader &) = delete;
        ~VertexShader();

        void Initialize(const void *source, uint32_t sourceLength);

        VertexShader &operator=(const VertexShader &) = delete;

        uint32_t GetShaderId() const
        {
            return id;
        }

      private:
        uint32_t id;
    };

    struct FragmentShader
    {
      public:
        FragmentShader(const std::string &fileName);
        FragmentShader(const void *source, uint32_t sourceLength);
        FragmentShader(const FragmentShader &) = delete;
        ~FragmentShader();

        void Initialize(const void *source, uint32_t sourceLength);

        FragmentShader &operator=(const FragmentShader &) = delete;

        uint32_t GetShaderId() const
        {
            return id;
        }

      private:
        uint32_t id;
    };

    struct ShaderParameterValue;

    struct ShaderProgram
    {
      public:
        ShaderProgram(std::shared_ptr<GraphicsDevice> graphicsDevice, const VertexShader &vertexShader,
            const FragmentShader &fragmentShader);
        ShaderProgram(const ShaderProgram &) = delete;
        ~ShaderProgram();

        ShaderProgram &operator=(const ShaderProgram &) = delete;

        void SetParameter(const std::string &name, const Texture &texture, int slotNumber);
        void SetParameter(const std::string &name, const glm::mat4 &matrix);
        void SetParameter(const std::string &name, const float value);
        void SetParameter(const std::string &name, const float value0, const float value1);
        void SetParameter(const std::string &name, const float value0, const float value1, const float value2);
        void SetParameter(
            const std::string &name, const float value0, const float value1, const float value2, const float value3);
        void SetParameter(const std::string &name, const glm::vec2 &value);
        void SetParameter(const std::string &name, const glm::vec3 &value);
        void SetParameter(const std::string &name, const glm::vec4 &value);
        void SetParameter(const std::string &name, const int value);
        void SetParameter(const std::string &name, const int value0, const int value1);
        void SetParameter(const std::string &name, const int value0, const int value1, const int value2);
        void SetParameter(
            const std::string &name, const int value0, const int value1, const int value2, const int value3);
        //  matrices?
        //  samplers 1d, 3d, cube? arrays?

        // Clear the local copy of a parameter, this will not change the opengl shader
        void ClearParameter(const std::string &name);
        void ApplyParameters();

        uint32_t GetShaderId() const
        {
            return id;
        }

        int32_t GetParameterLocation(const std::string &parameter);
        uint32_t GetParameterType(const std::string &parameter);

        int32_t GetAttributeLocation(const std::string &attribute);
        uint32_t GetAttributeType(const std::string &attribute);

      private:
        struct ShaderParameter
        {
            int32_t location;
            uint32_t type;
        };

        struct ShaderAttribute
        {
            int32_t location;
            uint32_t type;
        };

        std::shared_ptr<GraphicsDevice> graphicsDevice;
        std::map<std::string, ShaderAttribute> attributes;
        std::map<std::string, ShaderParameter> parameters;
        std::map<std::string, ShaderParameterValue> parameterValues;
        uint32_t id;
    };
} // namespace Lucky