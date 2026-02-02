/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/spgist_index.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace scratchbird::core
{

/**
 * 2D Point structure
 */
struct Point2D
{
    double x;
    double y;

    Point2D() : x(0), y(0) {}
    Point2D(double x_, double y_) : x(x_), y(y_) {}

    std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> result(sizeof(Point2D));
        std::memcpy(result.data(), this, sizeof(Point2D));
        return result;
    }

    static Point2D deserialize(const std::vector<uint8_t>& data)
    {
        if (data.size() < sizeof(Point2D))
            return Point2D();

        Point2D point;
        std::memcpy(&point, data.data(), sizeof(Point2D));
        return point;
    }

    double distanceTo(const Point2D& other) const
    {
        double dx = x - other.x;
        double dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

/**
 * Quadrant enumeration for quad-tree
 */
enum class Quadrant : uint8_t
{
    NW = 0,  // Northwest (x < center_x, y >= center_y)
    NE = 1,  // Northeast (x >= center_x, y >= center_y)
    SW = 2,  // Southwest (x < center_x, y < center_y)
    SE = 3   // Southeast (x >= center_x, y < center_y)
};

/**
 * Quad-tree centroid (stored as prefix in inner nodes)
 */
struct QuadCentroid
{
    double center_x;
    double center_y;

    QuadCentroid() : center_x(0), center_y(0) {}
    QuadCentroid(double x, double y) : center_x(x), center_y(y) {}

    std::vector<uint8_t> serialize() const
    {
        std::vector<uint8_t> result(sizeof(QuadCentroid));
        std::memcpy(result.data(), this, sizeof(QuadCentroid));
        return result;
    }

    static QuadCentroid deserialize(const std::vector<uint8_t>& data)
    {
        if (data.size() < sizeof(QuadCentroid))
            return QuadCentroid();

        QuadCentroid centroid;
        std::memcpy(&centroid, data.data(), sizeof(QuadCentroid));
        return centroid;
    }

    Quadrant getQuadrant(const Point2D& point) const
    {
        bool east = (point.x >= center_x);
        bool north = (point.y >= center_y);

        if (north)
            return east ? Quadrant::NE : Quadrant::NW;
        else
            return east ? Quadrant::SE : Quadrant::SW;
    }
};

/**
 * SP-GiST operator class for 2D points using quad-tree partitioning
 *
 * Implements quad-tree: each inner node divides space into 4 quadrants
 * based on a centroid point. Supports point location and range queries.
 */
class SPGiSTQuadOperatorClass : public SPGiSTOperatorClass
{
public:
    static constexpr uint32_t OPCLASS_ID = 1;
    static constexpr const char* OPCLASS_NAME = "quad_ops";

    uint32_t getOpClassId() const override { return OPCLASS_ID; }
    std::string getOpClassName() const override { return OPCLASS_NAME; }

    Config config() const override
    {
        Config cfg;
        cfg.canReturnData = true;  // Supports index-only scans
        cfg.labelSize = sizeof(uint8_t);  // Quadrant label (1 byte)
        cfg.maxInnerNodes = 4;     // Always 4 quadrants
        return cfg;
    }

    SPGiSTTraversal choose(
        const std::vector<uint8_t>& innerPrefix,
        const std::vector<SPGiSTNodeLabel>& nodeLabels,
        const std::vector<uint8_t>& query) const override
    {
        QuadCentroid centroid = QuadCentroid::deserialize(innerPrefix);
        Point2D point = Point2D::deserialize(query);

        // Determine which quadrant the point belongs to
        Quadrant quad = centroid.getQuadrant(point);

        SPGiSTTraversal result;

        // Find matching child node
        for (size_t i = 0; i < nodeLabels.size(); ++i)
        {
            if (nodeLabels[i].data.size() > 0)
            {
                Quadrant node_quad = static_cast<Quadrant>(nodeLabels[i].data[0]);
                if (node_quad == quad)
                {
                    result.match_type = SPGiSTMatchType::MATCH_NODE;
                    result.node_index = i;
                    return result;
                }
            }
        }

        // No matching child found, need to add one
        result.match_type = SPGiSTMatchType::MATCH_ADD_NODE;
        result.prefix.push_back(static_cast<uint8_t>(quad));
        return result;
    }

    void pickSplit(
        const std::vector<std::vector<uint8_t>>& values,
        std::vector<uint8_t>& prefix,
        std::vector<std::vector<uint8_t>>& labels,
        std::vector<size_t>& assignments) const override
    {
        if (values.empty())
            return;

        // Calculate centroid from all points
        double sum_x = 0.0, sum_y = 0.0;
        for (const auto& val : values)
        {
            Point2D point = Point2D::deserialize(val);
            sum_x += point.x;
            sum_y += point.y;
        }

        QuadCentroid centroid(sum_x / values.size(), sum_y / values.size());
        prefix = centroid.serialize();

        // Create 4 quadrant labels
        labels.resize(4);
        labels[0].push_back(static_cast<uint8_t>(Quadrant::NW));
        labels[1].push_back(static_cast<uint8_t>(Quadrant::NE));
        labels[2].push_back(static_cast<uint8_t>(Quadrant::SW));
        labels[3].push_back(static_cast<uint8_t>(Quadrant::SE));

        // Assign each point to its quadrant
        assignments.resize(values.size());
        for (size_t i = 0; i < values.size(); ++i)
        {
            Point2D point = Point2D::deserialize(values[i]);
            Quadrant quad = centroid.getQuadrant(point);
            assignments[i] = static_cast<size_t>(quad);
        }
    }

    bool innerConsistent(
        const std::vector<uint8_t>& innerPrefix,
        const std::vector<uint8_t>& nodeLabel,
        const std::vector<uint8_t>& query) const override
    {
        // For point location queries, check if the quadrant could contain the point
        QuadCentroid centroid = QuadCentroid::deserialize(innerPrefix);
        Point2D query_point = Point2D::deserialize(query);

        if (nodeLabel.empty())
            return false;

        Quadrant node_quad = static_cast<Quadrant>(nodeLabel[0]);
        Quadrant query_quad = centroid.getQuadrant(query_point);

        return node_quad == query_quad;
    }

    bool leafConsistent(
        const std::vector<uint8_t>& leafValue,
        const std::vector<uint8_t>& query) const override
    {
        // For point location: exact match
        Point2D leaf_point = Point2D::deserialize(leafValue);
        Point2D query_point = Point2D::deserialize(query);

        // Allow small epsilon for floating point comparison
        constexpr double EPSILON = 1e-9;
        return std::abs(leaf_point.x - query_point.x) < EPSILON &&
               std::abs(leaf_point.y - query_point.y) < EPSILON;
    }
};

} // namespace scratchbird::core
