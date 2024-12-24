#include <algorithm>
#include <limits>

#include <Lucky/Math/Collision.hpp>
#include <Lucky/Math/MathHelpers.hpp>

namespace Lucky
{
    bool FindCollision(const glm::vec2 &point, const Collider &collider, const glm::vec2 &position)
    {
        for (const auto &axis : collider.collisionAxes)
        {
            float min, max;
            CalculateProjectedInterval(collider.vertices, position, axis, min, max);
            float p = glm::dot(axis, point);

            if (p < min || p > max)
                return false;
        }

        return true;
    }

    bool FindIntervalIntersection(const Collider &colliderA, const Collider &colliderB,
        const glm::vec2 &relativePosition, const glm::vec2 &relativeVelocity, const glm::vec2 &axis, float &time)
    {
        float min0, max0, min1, max1;
        CalculateProjectedInterval(colliderA.vertices, relativePosition, axis, min0, max0);
        CalculateProjectedInterval(colliderB.vertices, axis, min1, max1);

        float d0 = min0 - max1;
        float d1 = min1 - max0;

        if (ApproximatelyZero(d0))
            d0 = 0;
        if (ApproximatelyZero(d1))
            d1 = 0;

        if (d0 < 0 && d1 < 0)
        {
            time = std::max(d0, d1);
            return true;
        }

        float v = glm::dot(relativeVelocity, axis);
        if (ApproximatelyZero(v))
        {
            time = -std::numeric_limits<float>::infinity();
            return false;
        }

        float t0 = -d0 / v;
        float t1 = d1 / v;

        if (t0 <= 0 && t1 <= 0)
        {
            time = -std::numeric_limits<float>::infinity();
            return false;
        }

        if (t0 > t1)
        {
            float temp = t0;
            t0 = t1;
            t1 = temp;
        }

        time = (t0 >= 0) ? t0 : t1;

        return true;
    }

    bool FindCollision(const Collider &colliderA, const Collider &colliderB, const glm::vec2 &relativePosition,
        const glm::vec2 &relativeVelocity, glm::vec2 &axis, float &time)
    {
        std::vector<glm::vec2> collisionAxes;
        collisionAxes.reserve(colliderA.collisionAxes.size() + colliderB.collisionAxes.size() + 1);

        collisionAxes.insert(
            collisionAxes.end(), std::begin(colliderA.collisionAxes), std::end(colliderA.collisionAxes));
        collisionAxes.insert(
            collisionAxes.end(), std::begin(colliderB.collisionAxes), std::end(colliderB.collisionAxes));

        if (!ApproximatelyZero(glm::length(relativeVelocity)))
        {
            glm::vec2 velocityNormal = glm::normalize(glm::vec2(relativeVelocity.y, -relativeVelocity.x));
            collisionAxes.push_back(velocityNormal);
        }

        float maxNegativeTime = -std::numeric_limits<float>::infinity();
        float maxPositiveTime = -std::numeric_limits<float>::infinity();
        glm::vec2 maxNegativeAxis = {0.0f, 0.0f};
        glm::vec2 maxPositiveAxis = {0.0f, 0.0f};
        bool futureCollision = false;

        for (const auto &testAxis : collisionAxes)
        {
            float tempTime;
            if (!FindIntervalIntersection(colliderA, colliderB, relativePosition, relativeVelocity, testAxis, tempTime))
            {
                return false;
            }

            if (tempTime >= 0)
            {
                if (tempTime > maxPositiveTime)
                {
                    maxPositiveTime = tempTime;
                    maxPositiveAxis = testAxis;
                }

                futureCollision = true;
            }
            else
            {
                if (tempTime > maxNegativeTime)
                {
                    maxNegativeTime = tempTime;
                    maxNegativeAxis = testAxis;
                }
            }
        }

        if (futureCollision)
        {
            time = maxPositiveTime;
            axis = maxPositiveAxis;
        }
        else
        {
            time = maxNegativeTime;
            axis = maxNegativeAxis;
        }

        if (time > 1.0f)
            return false;

        glm::vec2 relativeCenter = colliderA.center + relativePosition - colliderB.center;
        if (glm::dot(relativeCenter, axis) < 0.0f)
        {
            axis *= -1.0f;
        }

        if (colliderA.isOneWay && (time < 0 || glm::dot(colliderA.oneWayDirection, -axis) >= 0))
            return false;

        if (colliderB.isOneWay && (time < 0 || glm::dot(colliderB.oneWayDirection, axis) >= 0))
            return false;

        return true;
    }

    PolygonComponent GetMaximumPolygonComponent(
        const std::vector<glm::vec2> &vertices, const glm::vec2 &direction, int &vertexIndexA, int &vertexIndexB)
    {
        float max = glm::dot(direction, vertices[0]);
        vertexIndexA = 0;
        PolygonComponent pc = PolygonComponent::Vertex;

        for (size_t i = 1; i < vertices.size(); i++)
        {
            const auto &v = vertices[i];
            auto temp = glm::dot(direction, v);

            if (ApproximatelyEqual(temp, max))
            {
                vertexIndexB = (int)i;
                pc = PolygonComponent::Edge;
            }
            else if (temp > max)
            {
                max = temp;
                vertexIndexA = (int)i;
                pc = PolygonComponent::Vertex;
            }
        }

        return pc;
    }

    void CalculateProjectedInterval(
        const std::vector<glm::vec2> &vertices, const glm::vec2 &axis, float &min, float &max)
    {
        min = std::numeric_limits<float>::infinity();
        max = -std::numeric_limits<float>::infinity();

        for (const auto &v : vertices)
        {
            auto p = glm::dot(axis, v);
            min = std::min(min, p);
            max = std::max(max, p);
        }
    }

    void CalculateProjectedInterval(
        const std::vector<glm::vec2> &vertices, const glm::vec2 &offset, const glm::vec2 &axis, float &min, float &max)
    {
        min = std::numeric_limits<float>::infinity();
        max = -std::numeric_limits<float>::infinity();

        for (const auto &v : vertices)
        {
            auto p = glm::dot(axis, v + offset);
            min = std::min(min, p);
            max = std::max(max, p);
        }
    }
} // namespace Lucky
