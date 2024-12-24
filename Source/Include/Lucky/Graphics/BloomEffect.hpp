#include <memory>
#include <vector>

#include <Lucky/Graphics/BatchRenderer.hpp>
#include <Lucky/Graphics/GraphicsDevice.hpp>
#include <Lucky/Graphics/ShaderProgram.hpp>
#include <Lucky/Graphics/Texture.hpp>

namespace Lucky
{
    class BloomEffect
    {
      public:
        BloomEffect(int width, int height, std::shared_ptr<GraphicsDevice> graphicsDevice);
        ~BloomEffect();

        void ApplyEffect(float brightnessThreshold, std::shared_ptr<Lucky::Texture> input,
            std::shared_ptr<Lucky::Texture> output, Lucky::BatchRenderer &batchRenderer);

      private:
        std::shared_ptr<GraphicsDevice> graphicsDevice;
        std::shared_ptr<Texture> thresholdExtractTexture;
        std::vector<std::shared_ptr<Texture>> downSampleTextures;
        std::shared_ptr<ShaderProgram> downSampleShader;
        std::shared_ptr<ShaderProgram> upSampleBlurShader;
        std::shared_ptr<ShaderProgram> thresholdExtractShader;
    };
} // namespace Lucky