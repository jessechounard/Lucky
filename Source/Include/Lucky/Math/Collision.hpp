#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace Lucky
{
    enum class PolygonComponent
    {
        Vertex,
        Edge,
    };

    struct Collider
    {
        std::vector<glm::vec2> vertices;
        std::vector<glm::vec2> collisionAxes;
        glm::vec2 center;

        bool isOneWay;
        glm::vec2 oneWayDirection;
    };

    bool FindCollision(const glm::vec2 &point, const Collider &collider, const glm::vec2 &position);

    // relative position and velocity is A - B
    bool FindCollision(const Collider &colliderA, const Collider &colliderB, const glm::vec2 &relativePosition,
        const glm::vec2 &relativeVelocity, glm::vec2 &axis, float &time);

    PolygonComponent GetMaximumPolygonComponent(
        const std::vector<glm::vec2> &vertices, const glm::vec2 &direction, int &vertexIndexA, int &vertexIndexB);

    void CalculateProjectedInterval(
        const std::vector<glm::vec2> &vertices, const glm::vec2 &axis, float &min, float &max);

    void CalculateProjectedInterval(
        const std::vector<glm::vec2> &vertices, const glm::vec2 &offset, const glm::vec2 &axis, float &min, float &max);
} // namespace Lucky
