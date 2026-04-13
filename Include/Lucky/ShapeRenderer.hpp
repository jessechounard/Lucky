#pragma once

#include <vector>

#include <glm/glm.hpp>

#include <Lucky/BatchRenderer.hpp>
#include <Lucky/BlendState.hpp>
#include <Lucky/Color.hpp>
#include <Lucky/Shader.hpp>

namespace Lucky {

/**
 * Renders antialiased 2D shapes by dispatching to an SDF fragment shader.
 *
 * Each shape draws a single textured quad whose fragment shader computes the
 * shape's signed distance field and turns it into a smooth outline. Outlines
 * are stroked at `thickness` pixels wide and lines are drawn at `thickness`
 * pixels total width.
 *
 * Construct one ShapeRenderer per BatchRenderer you want to draw shapes
 * through, call Begin() with the SDF shader, then call any number of Draw*()
 * methods, then End().
 */
struct ShapeRenderer {
    ShapeRenderer(BatchRenderer &batchRenderer);

    void Begin(BlendMode blendMode, Shader &shader);
    void End();

    void DrawLine(glm::vec2 start, glm::vec2 end, Color color, float thickness = 1.0f);
    void DrawCircle(glm::vec2 center, float radius, Color color, float thickness = 1.0f);
    void DrawRectangle(glm::vec2 center, glm::vec2 dimensions, float rotation, Color color,
        float thickness = 1.0f);
    void DrawTriangle(glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, float rotation, Color color,
        float thickness = 1.0f);
    void DrawDiamond(glm::vec2 center, float width, float height, float rotation, Color color,
        float thickness = 1.0f);
    void DrawRegularPolygon(glm::vec2 center, float radius, int sides, float rotation, Color color,
        float thickness = 1.0f);
    void DrawStar(glm::vec2 center, float outerRadius, float innerRadius, int points,
        float rotation, Color color, float thickness = 1.0f);

    static std::vector<glm::vec2> GetRectangleVertices(
        glm::vec2 center, glm::vec2 dimensions, float rotation);
    static std::vector<glm::vec2> GetTriangleVertices(
        glm::vec2 p0, glm::vec2 p1, glm::vec2 p2, float rotation);
    static std::vector<glm::vec2> GetDiamondVertices(
        glm::vec2 center, float width, float height, float rotation);
    static std::vector<glm::vec2> GetRegularPolygonVertices(
        glm::vec2 center, float radius, int sides, float rotation);
    static std::vector<glm::vec2> GetStarVertices(
        glm::vec2 center, float outerRadius, float innerRadius, int points, float rotation);

  private:
    // The SDF shader's constant buffer layout. shapeType selects which SDF
    // function to use; the other fields are the parameters that shape needs.
    // For lines, tri[0] and tri[1] hold the two endpoints in normalized
    // [-1, 1] quad space.
    struct SdfParams {
        uint32_t shapeType;
        float halfThickness;
        uint32_t count;
        float starRatio;
        glm::vec2 shapeSize;
        glm::vec2 tri[3];
    };

    void EmitQuad(glm::vec2 center, float quadHalf, float rotation, Color color,
        const SdfParams &params);

    BatchRenderer &batchRenderer;
    BlendMode blendMode = BlendMode::Additive;
    Shader *shader = nullptr;
};

} // namespace Lucky
