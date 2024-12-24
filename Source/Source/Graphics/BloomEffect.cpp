#include <Lucky/Graphics/BloomEffect.hpp>

namespace Lucky
{
    // todo: make these configurable
    static const int downSampleLevels = 7;
    static const float blurFilterRadius = 0.003f;

    static const char *vertexShaderSource = // same as default BatchRenderer vert shader
        "attribute vec4 position;\n"
        "attribute vec4 color;\n"
        "attribute vec2 texcoord;\n"
        "varying vec4 v_color;\n"
        "varying vec2 v_texcoord;\n"
        "uniform mat4 ProjectionMatrix;\n"
        //
        "void main()\n"
        "{\n"
        "    gl_Position = ProjectionMatrix * position;\n"
        "    v_color = color;\n"
        "    v_texcoord = texcoord;\n"
        "}\n";

    static const char *downSampleFragmentSource = //
        "varying vec2 v_texcoord;\n"

        "uniform sampler2D TextureSampler;\n"
        "uniform vec2 sourceResolution;\n"
        //
        "void main()\n"
        "{\n"
        "    vec2 sourceTexelSize = 1.0 / sourceResolution;\n"
        "    float x = sourceTexelSize.x;\n"
        "    float y = sourceTexelSize.y;\n"
        //
        "    vec3 a = texture2D(TextureSampler, vec2(v_texcoord.x - 2 * x, v_texcoord.y + 2 * y)).rgb;\n"
        "    vec3 b = texture2D(TextureSampler, vec2(v_texcoord.x, v_texcoord.y + 2 * y)).rgb;\n"
        "    vec3 c = texture2D(TextureSampler, vec2(v_texcoord.x + 2 * x, v_texcoord.y + 2 * y)).rgb;\n"
        //
        "    vec3 d = texture2D(TextureSampler, vec2(v_texcoord.x - 2 * x, v_texcoord.y)).rgb;\n"
        "    vec3 e = texture2D(TextureSampler, vec2(v_texcoord.x, v_texcoord.y)).rgb;\n"
        "    vec3 f = texture2D(TextureSampler, vec2(v_texcoord.x + 2 * x, v_texcoord.y)).rgb;\n"
        //
        "    vec3 g = texture2D(TextureSampler, vec2(v_texcoord.x - 2 * x, v_texcoord.y - 2 * y)).rgb;\n"
        "    vec3 h = texture2D(TextureSampler, vec2(v_texcoord.x, v_texcoord.y - 2 * y)).rgb;\n"
        "    vec3 i = texture2D(TextureSampler, vec2(v_texcoord.x + 2 * x, v_texcoord.y - 2 * y)).rgb;\n"
        //
        "    vec3 j = texture2D(TextureSampler, vec2(v_texcoord.x - x, v_texcoord.y + y)).rgb;\n"
        "    vec3 k = texture2D(TextureSampler, vec2(v_texcoord.x + x, v_texcoord.y + y)).rgb;\n"
        "    vec3 l = texture2D(TextureSampler, vec2(v_texcoord.x - x, v_texcoord.y - y)).rgb;\n"
        "    vec3 m = texture2D(TextureSampler, vec2(v_texcoord.x + x, v_texcoord.y - y)).rgb;\n"
        //
        "    vec3 downsample = e * 0.125;\n"
        "    downsample += (a + c + g + i) * 0.03125;\n"
        "    downsample += (b + d + f + h) * 0.0625;\n"
        "    downsample += (j + k + l + m) * 0.125;\n"
        "    gl_FragColor = vec4(downsample, 1.0);\n"
        "}\n";

    static const char *upSampleBlurFragmentSource = //
        "varying vec2 v_texcoord;\n"
        "uniform sampler2D TextureSampler;\n"
        "uniform float filterRadius;\n"
        //
        "void main()\n"
        "{\n"
        "    float x = filterRadius;\n"
        "    float y = filterRadius;\n"
        //
        "    vec3 a = texture2D(TextureSampler, vec2(v_texcoord.x - x, v_texcoord.y + y)).rgb;\n"
        "    vec3 b = texture2D(TextureSampler, vec2(v_texcoord.x, v_texcoord.y + y)).rgb;\n"
        "    vec3 c = texture2D(TextureSampler, vec2(v_texcoord.x + x, v_texcoord.y + y)).rgb;\n"
        //
        "    vec3 d = texture2D(TextureSampler, vec2(v_texcoord.x - x, v_texcoord.y)).rgb;\n"
        "    vec3 e = texture2D(TextureSampler, vec2(v_texcoord.x, v_texcoord.y)).rgb;\n"
        "    vec3 f = texture2D(TextureSampler, vec2(v_texcoord.x + x, v_texcoord.y)).rgb;\n"
        //
        "    vec3 g = texture2D(TextureSampler, vec2(v_texcoord.x - x, v_texcoord.y - y)).rgb;\n"
        "    vec3 h = texture2D(TextureSampler, vec2(v_texcoord.x, v_texcoord.y - y)).rgb;\n"
        "    vec3 i = texture2D(TextureSampler, vec2(v_texcoord.x + x, v_texcoord.y - y)).rgb;\n"
        //
        "    vec3 upsample = e*4.0;\n"
        "    upsample += (b+d+f+h)*2.0;\n"
        "    upsample += (a+c+g+i);\n"
        "    upsample *= 1.0 / 16.0;\n"
        //
        "    gl_FragColor = vec4(upsample, 1.0);\n"
        "}\n";

    static const char *thresholdExtractFragmentSource = //
        "varying vec2 v_texcoord;\n"
        "uniform sampler2D TextureSampler;\n"
        "uniform float Threshold;\n"
        //
        "float luminance(vec3 c)\n"
        "{\n"
        "    return c.r * 0.299 + c.b * 0.587 + c.g * 0.114;\n"
        "}\n"
        //
        "void main()\n"
        "{\n"
        "    vec4 color = texture2D(TextureSampler, v_texcoord);\n"
        "    float l = luminance(color.rgb);\n"
        //
        "    gl_FragColor = (luminance(color.rgb) > Threshold) ? color : vec4(0., 0., 0., 1.);\n"
        "}\n";

    BloomEffect::BloomEffect(int width, int height, std::shared_ptr<GraphicsDevice> graphicsDevice)
        : graphicsDevice(graphicsDevice)
    {
        thresholdExtractTexture = std::make_shared<Texture>(
            TextureType::RenderTarget, width, height, nullptr, 0, TextureFilter::Linear, TextureFormat::HDR);

        for (int level = 0; level < downSampleLevels; level++)
        {
            width /= 2;
            height /= 2;

            downSampleTextures.emplace_back(std::make_shared<Texture>(
                TextureType::RenderTarget, width, height, nullptr, 0, TextureFilter::Linear, TextureFormat::HDR));

            if (width == 1 || height == 1)
            {
                break;
            }
        }

        Lucky::VertexShader defaultVertexShader(vertexShaderSource, (uint32_t)strlen(vertexShaderSource));
        Lucky::FragmentShader downSampleFragmentShader(
            downSampleFragmentSource, (uint32_t)strlen(downSampleFragmentSource));
        Lucky::FragmentShader upSampleBlurFragmentShader(
            upSampleBlurFragmentSource, (uint32_t)strlen(upSampleBlurFragmentSource));
        Lucky::FragmentShader thresholdExtractFragmentShader(
            thresholdExtractFragmentSource, (uint32_t)strlen(thresholdExtractFragmentSource));

        downSampleShader =
            std::make_unique<ShaderProgram>(graphicsDevice, defaultVertexShader, downSampleFragmentShader);
        upSampleBlurShader =
            std::make_unique<ShaderProgram>(graphicsDevice, defaultVertexShader, upSampleBlurFragmentShader);
        thresholdExtractShader =
            std::make_unique<ShaderProgram>(graphicsDevice, defaultVertexShader, thresholdExtractFragmentShader);
    }

    BloomEffect::~BloomEffect()
    {
    }

    void BloomEffect::ApplyEffect(float brightnessThreshold, std::shared_ptr<Lucky::Texture> input,
        std::shared_ptr<Lucky::Texture> output, Lucky::BatchRenderer &batchRenderer)
    {
        // todo: verify that width/height match what we expect?
        // todo: verify input/output are rendertargets and matching width/height?

        float outputWidth = (float)output->GetWidth();
        float outputHeight = (float)output->GetHeight();

        graphicsDevice->BindRenderTarget(*thresholdExtractTexture);
        batchRenderer.Begin(Lucky::BlendMode::None, input, thresholdExtractShader);
        thresholdExtractShader->SetParameter("Threshold", brightnessThreshold);
        batchRenderer.BatchQuadUV(glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 0.0f),
            glm::vec2(outputWidth, outputHeight), Lucky::Color::White);
        batchRenderer.End();

        auto currentTexture = thresholdExtractTexture;

        for (auto &texture : downSampleTextures)
        {
            float inWidth = (float)currentTexture->GetWidth();
            float inHeight = (float)currentTexture->GetHeight();
            float outWidth = (float)texture->GetWidth();
            float outHeight = (float)texture->GetHeight();

            graphicsDevice->BindRenderTarget(*texture);
            batchRenderer.Begin(Lucky::BlendMode::None, currentTexture, downSampleShader);
            downSampleShader->SetParameter("sourceResolution", inWidth, inHeight);
            batchRenderer.BatchQuadUV(glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 0.0f),
                glm::vec2(outWidth, outHeight), Color::White);
            batchRenderer.End();
            graphicsDevice->UnbindRenderTarget();

            currentTexture = texture;
        }

        for (int level = (int)downSampleTextures.size() - 2; level > 0; level--)
        {
            auto sourceTexture = downSampleTextures[level];
            auto destTexture = downSampleTextures[level - 1];

            float destWidth = (float)destTexture->GetWidth();
            float destHeight = (float)destTexture->GetHeight();

            graphicsDevice->BindRenderTarget(*destTexture);

            batchRenderer.Begin(Lucky::BlendMode::Additive, sourceTexture, upSampleBlurShader);
            upSampleBlurShader->SetParameter("filterRadius", blurFilterRadius);
            batchRenderer.BatchQuadUV(glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 0.0f),
                glm::vec2(destWidth, destHeight), Color::White);
            batchRenderer.End();
        }

        graphicsDevice->BindRenderTarget(*output);
        graphicsDevice->ClearScreen(Color::Black);

        batchRenderer.Begin(Lucky::BlendMode::Additive, input);
        batchRenderer.BatchQuad(nullptr, glm::vec2(0.0f, 0.0f), 0, glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 0.0f),
            UVMode::Normal, Color::White);
        batchRenderer.End();
        batchRenderer.Begin(Lucky::BlendMode::Additive, downSampleTextures[0], upSampleBlurShader);
        upSampleBlurShader->SetParameter("filterRadius", blurFilterRadius);
        batchRenderer.BatchQuadUV(glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 0.0f),
            glm::vec2(outputWidth, outputHeight), Color::White);
        batchRenderer.End();

        graphicsDevice->UnbindRenderTarget();
    }
} // namespace Lucky