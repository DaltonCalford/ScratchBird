/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>
#include "scratchbird/spatial/multi_geometry.h"
#include "scratchbird/spatial/wkt_parser.h"
#include "scratchbird/spatial/wkb.h"
#include "scratchbird/core/types.h"
#include <cmath>

using namespace scratchbird::core;
using namespace scratchbird::spatial;

// ===== MULTIPOINT Tests =====

TEST(MultiGeometryTest, MultiPointCreation)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);
    EXPECT_EQ(mp.type(), MultiGeometryType::MULTIPOINT);
    EXPECT_EQ(mp.size(), 0);
    EXPECT_TRUE(mp.empty());
}

TEST(MultiGeometryTest, MultiPointAddGeometry)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);

    auto pt1 = TypedValue::makePoint(0.0, 0.0);
    auto pt2 = TypedValue::makePoint(1.0, 1.0);
    auto pt3 = TypedValue::makePoint(2.0, 2.0);

    EXPECT_TRUE(mp.addGeometry(pt1));
    EXPECT_TRUE(mp.addGeometry(pt2));
    EXPECT_TRUE(mp.addGeometry(pt3));

    EXPECT_EQ(mp.size(), 3);
    EXPECT_FALSE(mp.empty());
}

TEST(MultiGeometryTest, MultiPointTypeValidation)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);

    auto pt = TypedValue::makePoint(0.0, 0.0);
    auto line = TypedValue::makeLineString(LineString({Point(0, 0), Point(1, 1)}));

    EXPECT_TRUE(mp.addGeometry(pt));
    EXPECT_FALSE(mp.addGeometry(line)); // Should reject non-point
}

TEST(MultiGeometryTest, MultiPointToWKT)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);
    mp.addGeometry(TypedValue::makePoint(0.0, 0.0));
    mp.addGeometry(TypedValue::makePoint(1.0, 1.0));
    mp.addGeometry(TypedValue::makePoint(2.0, 2.0));

    std::string wkt = mp.toWKT();
    EXPECT_EQ(wkt, "MULTIPOINT((0 0), (1 1), (2 2))");
}

TEST(MultiGeometryTest, MultiPointFromWKT)
{
    std::string wkt = "MULTIPOINT((0 0), (1 1), (2 2))";
    auto mp_opt = MultiGeometry::fromWKT(wkt);

    ASSERT_TRUE(mp_opt.has_value());
    auto& mp = *mp_opt;

    EXPECT_EQ(mp.type(), MultiGeometryType::MULTIPOINT);
    EXPECT_EQ(mp.size(), 3);

    auto pt0 = mp.getGeometry(0).getPoint();
    EXPECT_DOUBLE_EQ(pt0.x, 0.0);
    EXPECT_DOUBLE_EQ(pt0.y, 0.0);

    auto pt2 = mp.getGeometry(2).getPoint();
    EXPECT_DOUBLE_EQ(pt2.x, 2.0);
    EXPECT_DOUBLE_EQ(pt2.y, 2.0);
}

TEST(MultiGeometryTest, MultiPointWKTRoundTrip)
{
    std::string original_wkt = "MULTIPOINT((0 0), (1 1), (2 2))";
    auto mp_opt = MultiGeometry::fromWKT(original_wkt);
    ASSERT_TRUE(mp_opt.has_value());

    std::string regenerated_wkt = mp_opt->toWKT();
    EXPECT_EQ(regenerated_wkt, original_wkt);
}

TEST(MultiGeometryTest, MultiPointBoundingBox)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);
    mp.addGeometry(TypedValue::makePoint(-5.0, -10.0));
    mp.addGeometry(TypedValue::makePoint(10.0, 20.0));
    mp.addGeometry(TypedValue::makePoint(3.0, 5.0));

    BoundingBox bbox = mp.boundingBox();
    EXPECT_DOUBLE_EQ(bbox.min_x, -5.0);
    EXPECT_DOUBLE_EQ(bbox.min_y, -10.0);
    EXPECT_DOUBLE_EQ(bbox.max_x, 10.0);
    EXPECT_DOUBLE_EQ(bbox.max_y, 20.0);
    EXPECT_TRUE(bbox.isValid());
}

TEST(MultiGeometryTest, MultiPointWKB)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);
    mp.addGeometry(TypedValue::makePoint(0.0, 0.0));
    mp.addGeometry(TypedValue::makePoint(1.0, 1.0));

    std::vector<uint8_t> wkb = mp.toWKB();

    // Verify WKB structure
    ASSERT_GT(wkb.size(), 0);
    EXPECT_EQ(wkb[0], WKBSerializer::WKB_NDR); // Little-endian

    // Verify type code (bytes 1-4, little-endian)
    uint32_t type = static_cast<uint32_t>(wkb[1]) |
                    (static_cast<uint32_t>(wkb[2]) << 8) |
                    (static_cast<uint32_t>(wkb[3]) << 16) |
                    (static_cast<uint32_t>(wkb[4]) << 24);
    EXPECT_EQ(type, WKBSerializer::WKB_MULTIPOINT);

    // Verify count (bytes 5-8, little-endian)
    uint32_t count = static_cast<uint32_t>(wkb[5]) |
                     (static_cast<uint32_t>(wkb[6]) << 8) |
                     (static_cast<uint32_t>(wkb[7]) << 16) |
                     (static_cast<uint32_t>(wkb[8]) << 24);
    EXPECT_EQ(count, 2);
}

// ===== MULTILINESTRING Tests =====

TEST(MultiGeometryTest, MultiLineStringCreation)
{
    MultiGeometry mls(MultiGeometryType::MULTILINESTRING);
    EXPECT_EQ(mls.type(), MultiGeometryType::MULTILINESTRING);
    EXPECT_EQ(mls.size(), 0);
}

TEST(MultiGeometryTest, MultiLineStringAddGeometry)
{
    MultiGeometry mls(MultiGeometryType::MULTILINESTRING);

    LineString line1({Point(0, 0), Point(1, 1)});
    LineString line2({Point(2, 2), Point(3, 3)});

    auto val1 = TypedValue::makeLineString(std::move(line1));
    auto val2 = TypedValue::makeLineString(std::move(line2));

    EXPECT_TRUE(mls.addGeometry(val1));
    EXPECT_TRUE(mls.addGeometry(val2));
    EXPECT_EQ(mls.size(), 2);
}

TEST(MultiGeometryTest, MultiLineStringToWKT)
{
    MultiGeometry mls(MultiGeometryType::MULTILINESTRING);

    LineString line1({Point(0, 0), Point(1, 1)});
    LineString line2({Point(2, 2), Point(3, 3)});

    mls.addGeometry(TypedValue::makeLineString(std::move(line1)));
    mls.addGeometry(TypedValue::makeLineString(std::move(line2)));

    std::string wkt = mls.toWKT();
    EXPECT_EQ(wkt, "MULTILINESTRING((0 0, 1 1), (2 2, 3 3))");
}

TEST(MultiGeometryTest, MultiLineStringFromWKT)
{
    std::string wkt = "MULTILINESTRING((0 0, 1 1), (2 2, 3 3))";
    auto mls_opt = MultiGeometry::fromWKT(wkt);

    ASSERT_TRUE(mls_opt.has_value());
    auto& mls = *mls_opt;

    EXPECT_EQ(mls.type(), MultiGeometryType::MULTILINESTRING);
    EXPECT_EQ(mls.size(), 2);

    auto line0 = mls.getGeometry(0).getLineString();
    EXPECT_EQ(line0.points.size(), 2);
    EXPECT_DOUBLE_EQ(line0.points[0].x, 0.0);
    EXPECT_DOUBLE_EQ(line0.points[1].y, 1.0);
}

TEST(MultiGeometryTest, MultiLineStringWKTRoundTrip)
{
    std::string original_wkt = "MULTILINESTRING((0 0, 1 1), (2 2, 3 3))";
    auto mls_opt = MultiGeometry::fromWKT(original_wkt);
    ASSERT_TRUE(mls_opt.has_value());

    std::string regenerated_wkt = mls_opt->toWKT();
    EXPECT_EQ(regenerated_wkt, original_wkt);
}

TEST(MultiGeometryTest, MultiLineStringBoundingBox)
{
    MultiGeometry mls(MultiGeometryType::MULTILINESTRING);

    LineString line1({Point(-10, -20), Point(5, 10)});
    LineString line2({Point(0, 0), Point(15, 25)});

    mls.addGeometry(TypedValue::makeLineString(std::move(line1)));
    mls.addGeometry(TypedValue::makeLineString(std::move(line2)));

    BoundingBox bbox = mls.boundingBox();
    EXPECT_DOUBLE_EQ(bbox.min_x, -10.0);
    EXPECT_DOUBLE_EQ(bbox.min_y, -20.0);
    EXPECT_DOUBLE_EQ(bbox.max_x, 15.0);
    EXPECT_DOUBLE_EQ(bbox.max_y, 25.0);
}

// ===== MULTIPOLYGON Tests =====

TEST(MultiGeometryTest, MultiPolygonCreation)
{
    MultiGeometry mpoly(MultiGeometryType::MULTIPOLYGON);
    EXPECT_EQ(mpoly.type(), MultiGeometryType::MULTIPOLYGON);
    EXPECT_EQ(mpoly.size(), 0);
}

TEST(MultiGeometryTest, MultiPolygonAddGeometry)
{
    MultiGeometry mpoly(MultiGeometryType::MULTIPOLYGON);

    Polygon poly1({Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10), Point(0, 0)});
    Polygon poly2({Point(20, 20), Point(30, 20), Point(30, 30), Point(20, 30), Point(20, 20)});

    EXPECT_TRUE(mpoly.addGeometry(TypedValue::makePolygon(std::move(poly1))));
    EXPECT_TRUE(mpoly.addGeometry(TypedValue::makePolygon(std::move(poly2))));
    EXPECT_EQ(mpoly.size(), 2);
}

TEST(MultiGeometryTest, MultiPolygonToWKT)
{
    MultiGeometry mpoly(MultiGeometryType::MULTIPOLYGON);

    Polygon poly1({Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10), Point(0, 0)});
    Polygon poly2({Point(20, 20), Point(30, 20), Point(30, 30), Point(20, 30), Point(20, 20)});

    mpoly.addGeometry(TypedValue::makePolygon(std::move(poly1)));
    mpoly.addGeometry(TypedValue::makePolygon(std::move(poly2)));

    std::string wkt = mpoly.toWKT();
    EXPECT_EQ(wkt, "MULTIPOLYGON(((0 0, 10 0, 10 10, 0 10, 0 0)), ((20 20, 30 20, 30 30, 20 30, 20 20)))");
}

TEST(MultiGeometryTest, MultiPolygonFromWKT)
{
    std::string wkt = "MULTIPOLYGON(((0 0, 10 0, 10 10, 0 10, 0 0)), ((20 20, 30 20, 30 30, 20 30, 20 20)))";
    auto mpoly_opt = MultiGeometry::fromWKT(wkt);

    ASSERT_TRUE(mpoly_opt.has_value());
    auto& mpoly = *mpoly_opt;

    EXPECT_EQ(mpoly.type(), MultiGeometryType::MULTIPOLYGON);
    EXPECT_EQ(mpoly.size(), 2);

    auto poly0 = mpoly.getGeometry(0).getPolygon();
    EXPECT_EQ(poly0.rings.size(), 1);
    EXPECT_EQ(poly0.rings[0].size(), 5);
}

TEST(MultiGeometryTest, MultiPolygonWKTRoundTrip)
{
    std::string original_wkt = "MULTIPOLYGON(((0 0, 10 0, 10 10, 0 10, 0 0)), ((20 20, 30 20, 30 30, 20 30, 20 20)))";
    auto mpoly_opt = MultiGeometry::fromWKT(original_wkt);
    ASSERT_TRUE(mpoly_opt.has_value());

    std::string regenerated_wkt = mpoly_opt->toWKT();
    EXPECT_EQ(regenerated_wkt, original_wkt);
}

TEST(MultiGeometryTest, MultiPolygonWithHoles)
{
    MultiGeometry mpoly(MultiGeometryType::MULTIPOLYGON);

    // Polygon with a hole
    Polygon poly1;
    poly1.rings.push_back({Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10), Point(0, 0)}); // Exterior
    poly1.rings.push_back({Point(2, 2), Point(8, 2), Point(8, 8), Point(2, 8), Point(2, 2)}); // Hole

    mpoly.addGeometry(TypedValue::makePolygon(std::move(poly1)));

    std::string wkt = mpoly.toWKT();
    EXPECT_TRUE(wkt.find("((0 0, 10 0, 10 10, 0 10, 0 0), (2 2, 8 2, 8 8, 2 8, 2 2))") != std::string::npos);
}

TEST(MultiGeometryTest, MultiPolygonBoundingBox)
{
    MultiGeometry mpoly(MultiGeometryType::MULTIPOLYGON);

    Polygon poly1({Point(-5, -5), Point(5, -5), Point(5, 5), Point(-5, 5), Point(-5, -5)});
    Polygon poly2({Point(10, 10), Point(20, 10), Point(20, 20), Point(10, 20), Point(10, 10)});

    mpoly.addGeometry(TypedValue::makePolygon(std::move(poly1)));
    mpoly.addGeometry(TypedValue::makePolygon(std::move(poly2)));

    BoundingBox bbox = mpoly.boundingBox();
    EXPECT_DOUBLE_EQ(bbox.min_x, -5.0);
    EXPECT_DOUBLE_EQ(bbox.min_y, -5.0);
    EXPECT_DOUBLE_EQ(bbox.max_x, 20.0);
    EXPECT_DOUBLE_EQ(bbox.max_y, 20.0);
}

// ===== GEOMETRYCOLLECTION Tests =====

TEST(MultiGeometryTest, GeometryCollectionCreation)
{
    MultiGeometry gc(MultiGeometryType::GEOMETRYCOLLECTION);
    EXPECT_EQ(gc.type(), MultiGeometryType::GEOMETRYCOLLECTION);
    EXPECT_EQ(gc.size(), 0);
}

TEST(MultiGeometryTest, GeometryCollectionMixedTypes)
{
    MultiGeometry gc(MultiGeometryType::GEOMETRYCOLLECTION);

    auto pt = TypedValue::makePoint(0.0, 0.0);
    auto line = TypedValue::makeLineString(LineString({Point(0, 0), Point(1, 1)}));
    auto poly = TypedValue::makePolygon(Polygon({Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1), Point(0, 0)}));

    EXPECT_TRUE(gc.addGeometry(pt));
    EXPECT_TRUE(gc.addGeometry(line));
    EXPECT_TRUE(gc.addGeometry(poly));
    EXPECT_EQ(gc.size(), 3);
}

TEST(MultiGeometryTest, GeometryCollectionToWKT)
{
    MultiGeometry gc(MultiGeometryType::GEOMETRYCOLLECTION);

    gc.addGeometry(TypedValue::makePoint(0.0, 0.0));
    gc.addGeometry(TypedValue::makeLineString(LineString({Point(0, 0), Point(1, 1)})));
    gc.addGeometry(TypedValue::makePolygon(Polygon({Point(0, 0), Point(1, 0), Point(1, 1), Point(0, 1), Point(0, 0)})));

    std::string wkt = gc.toWKT();
    EXPECT_EQ(wkt, "GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0, 1 1), POLYGON((0 0, 1 0, 1 1, 0 1, 0 0)))");
}

TEST(MultiGeometryTest, GeometryCollectionFromWKT)
{
    std::string wkt = "GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0, 1 1), POLYGON((0 0, 1 0, 1 1, 0 1, 0 0)))";
    auto gc_opt = MultiGeometry::fromWKT(wkt);

    ASSERT_TRUE(gc_opt.has_value());
    auto& gc = *gc_opt;

    EXPECT_EQ(gc.type(), MultiGeometryType::GEOMETRYCOLLECTION);
    EXPECT_EQ(gc.size(), 3);

    EXPECT_EQ(gc.getGeometry(0).type(), DataType::POINT);
    EXPECT_EQ(gc.getGeometry(1).type(), DataType::LINESTRING);
    EXPECT_EQ(gc.getGeometry(2).type(), DataType::POLYGON);
}

TEST(MultiGeometryTest, GeometryCollectionWKTRoundTrip)
{
    std::string original_wkt = "GEOMETRYCOLLECTION(POINT(0 0), LINESTRING(0 0, 1 1), POLYGON((0 0, 1 0, 1 1, 0 1, 0 0)))";
    auto gc_opt = MultiGeometry::fromWKT(original_wkt);
    ASSERT_TRUE(gc_opt.has_value());

    std::string regenerated_wkt = gc_opt->toWKT();
    EXPECT_EQ(regenerated_wkt, original_wkt);
}

TEST(MultiGeometryTest, GeometryCollectionBoundingBox)
{
    MultiGeometry gc(MultiGeometryType::GEOMETRYCOLLECTION);

    gc.addGeometry(TypedValue::makePoint(-10.0, -20.0));
    gc.addGeometry(TypedValue::makeLineString(LineString({Point(0, 0), Point(15, 25)})));
    gc.addGeometry(TypedValue::makePolygon(Polygon({Point(5, 5), Point(8, 5), Point(8, 8), Point(5, 8), Point(5, 5)})));

    BoundingBox bbox = gc.boundingBox();
    EXPECT_DOUBLE_EQ(bbox.min_x, -10.0);
    EXPECT_DOUBLE_EQ(bbox.min_y, -20.0);
    EXPECT_DOUBLE_EQ(bbox.max_x, 15.0);
    EXPECT_DOUBLE_EQ(bbox.max_y, 25.0);
}

// ===== BoundingBox Tests =====

TEST(BoundingBoxTest, DefaultConstruction)
{
    BoundingBox bbox;
    // Default bbox should be "empty" (min > max)
    EXPECT_GT(bbox.min_x, bbox.max_x);
    EXPECT_GT(bbox.min_y, bbox.max_y);
    EXPECT_FALSE(bbox.isValid());
}

TEST(BoundingBoxTest, ExplicitConstruction)
{
    BoundingBox bbox(0.0, 0.0, 10.0, 10.0);
    EXPECT_DOUBLE_EQ(bbox.min_x, 0.0);
    EXPECT_DOUBLE_EQ(bbox.min_y, 0.0);
    EXPECT_DOUBLE_EQ(bbox.max_x, 10.0);
    EXPECT_DOUBLE_EQ(bbox.max_y, 10.0);
    EXPECT_TRUE(bbox.isValid());
}

TEST(BoundingBoxTest, ExpandWithPoint)
{
    BoundingBox bbox;
    bbox.expand(Point(5.0, 5.0));
    bbox.expand(Point(-3.0, 10.0));

    EXPECT_DOUBLE_EQ(bbox.min_x, -3.0);
    EXPECT_DOUBLE_EQ(bbox.min_y, 5.0);
    EXPECT_DOUBLE_EQ(bbox.max_x, 5.0);
    EXPECT_DOUBLE_EQ(bbox.max_y, 10.0);
}

TEST(BoundingBoxTest, ExpandWithBoundingBox)
{
    BoundingBox bbox1(0.0, 0.0, 5.0, 5.0);
    BoundingBox bbox2(3.0, 3.0, 10.0, 10.0);

    bbox1.expand(bbox2);

    EXPECT_DOUBLE_EQ(bbox1.min_x, 0.0);
    EXPECT_DOUBLE_EQ(bbox1.min_y, 0.0);
    EXPECT_DOUBLE_EQ(bbox1.max_x, 10.0);
    EXPECT_DOUBLE_EQ(bbox1.max_y, 10.0);
}

TEST(BoundingBoxTest, Intersects)
{
    BoundingBox bbox1(0.0, 0.0, 5.0, 5.0);
    BoundingBox bbox2(3.0, 3.0, 10.0, 10.0);
    BoundingBox bbox3(10.0, 10.0, 15.0, 15.0);

    EXPECT_TRUE(bbox1.intersects(bbox2));
    EXPECT_TRUE(bbox2.intersects(bbox1));
    EXPECT_FALSE(bbox1.intersects(bbox3));
    EXPECT_FALSE(bbox3.intersects(bbox1));
}

TEST(BoundingBoxTest, ContainsPoint)
{
    BoundingBox bbox(0.0, 0.0, 10.0, 10.0);

    EXPECT_TRUE(bbox.contains(Point(5.0, 5.0)));
    EXPECT_TRUE(bbox.contains(Point(0.0, 0.0)));
    EXPECT_TRUE(bbox.contains(Point(10.0, 10.0)));
    EXPECT_FALSE(bbox.contains(Point(-1.0, 5.0)));
    EXPECT_FALSE(bbox.contains(Point(5.0, 11.0)));
}

// ===== Iterator Tests =====

TEST(MultiGeometryTest, IteratorSupport)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);
    mp.addGeometry(TypedValue::makePoint(0.0, 0.0));
    mp.addGeometry(TypedValue::makePoint(1.0, 1.0));
    mp.addGeometry(TypedValue::makePoint(2.0, 2.0));

    int count = 0;
    for (const auto& geom : mp) {
        EXPECT_EQ(geom.type(), DataType::POINT);
        ++count;
    }
    EXPECT_EQ(count, 3);
}

// ===== Validation Tests =====

TEST(MultiGeometryTest, IsValidEmpty)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);
    EXPECT_FALSE(mp.isValid()); // Empty multi-geometry is invalid
}

TEST(MultiGeometryTest, IsValidWithGeometries)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);
    mp.addGeometry(TypedValue::makePoint(0.0, 0.0));
    EXPECT_TRUE(mp.isValid());
}

TEST(MultiGeometryTest, ClearGeometries)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);
    mp.addGeometry(TypedValue::makePoint(0.0, 0.0));
    mp.addGeometry(TypedValue::makePoint(1.0, 1.0));

    EXPECT_EQ(mp.size(), 2);
    mp.clear();
    EXPECT_EQ(mp.size(), 0);
    EXPECT_TRUE(mp.empty());
}

// ===== Error Handling Tests =====

TEST(MultiGeometryTest, InvalidWKT)
{
    ErrorContext ctx;

    auto result = MultiGeometry::fromWKT("INVALID WKT", &ctx);
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(ctx.message.empty());
}

TEST(MultiGeometryTest, OutOfRangeAccess)
{
    MultiGeometry mp(MultiGeometryType::MULTIPOINT);
    mp.addGeometry(TypedValue::makePoint(0.0, 0.0));

    EXPECT_THROW(mp.getGeometry(1), std::out_of_range);
}
