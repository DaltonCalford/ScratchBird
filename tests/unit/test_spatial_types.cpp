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
#include "scratchbird/core/types.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/spatial/wkt_parser.h"
#include "scratchbird/spatial/wkb.h"
#include <cmath>

using namespace scratchbird::core;
using namespace scratchbird::spatial;

// ===== Point Tests =====

TEST(SpatialTypesTest, PointCreation)
{
    Point pt(1.5, 2.5);
    EXPECT_DOUBLE_EQ(pt.x, 1.5);
    EXPECT_DOUBLE_EQ(pt.y, 2.5);
}

TEST(SpatialTypesTest, PointEquality)
{
    Point pt1(1.5, 2.5);
    Point pt2(1.5, 2.5);
    Point pt3(2.5, 1.5);

    EXPECT_EQ(pt1, pt2);
    EXPECT_NE(pt1, pt3);
}

TEST(SpatialTypesTest, PointTypedValue)
{
    Point pt_obj(3.0, 4.0);
    auto val = TypedValue::makePoint(pt_obj);
    EXPECT_EQ(val.type(), DataType::POINT);
    EXPECT_FALSE(val.isNull());

    auto pt = val.getPoint();
    EXPECT_DOUBLE_EQ(pt.x, 3.0);
    EXPECT_DOUBLE_EQ(pt.y, 4.0);
}

TEST(SpatialTypesTest, PointToString)
{
    Point pt_obj(1.5, 2.5);
    auto val = TypedValue::makePoint(pt_obj);
    std::string str = val.toString();
    EXPECT_TRUE(str.find("POINT") != std::string::npos);
    EXPECT_TRUE(str.find("1.5") != std::string::npos);
    EXPECT_TRUE(str.find("2.5") != std::string::npos);
}

// ===== LineString Tests =====

TEST(SpatialTypesTest, LineStringCreation)
{
    std::vector<Point> points = {
        Point(0.0, 0.0),
        Point(1.0, 1.0),
        Point(2.0, 2.0)
    };
    LineString line(points);

    EXPECT_EQ(line.points.size(), 3);
    EXPECT_TRUE(line.isValid());
}

TEST(SpatialTypesTest, LineStringValidation)
{
    // Valid line (2+ points)
    std::vector<Point> valid_points = {Point(0.0, 0.0), Point(1.0, 1.0)};
    LineString valid_line(valid_points);
    EXPECT_TRUE(valid_line.isValid());

    // Invalid line (< 2 points)
    std::vector<Point> invalid_points = {Point(0.0, 0.0)};
    LineString invalid_line(invalid_points);
    EXPECT_FALSE(invalid_line.isValid());
}

TEST(SpatialTypesTest, LineStringTypedValue)
{
    std::vector<Point> points = {
        Point(0.0, 0.0),
        Point(1.0, 1.0),
        Point(2.0, 2.0)
    };
    LineString line_obj(points);
    auto val = TypedValue::makeLineString(line_obj);

    EXPECT_EQ(val.type(), DataType::LINESTRING);
    auto line = val.getLineString();
    EXPECT_EQ(line.points.size(), 3);
}

TEST(SpatialTypesTest, LineStringToString)
{
    std::vector<Point> points = {Point(0.0, 0.0), Point(1.0, 1.0)};
    LineString line_obj(points);
    auto val = TypedValue::makeLineString(line_obj);
    std::string str = val.toString();
    EXPECT_TRUE(str.find("LINESTRING") != std::string::npos);
}

// ===== Polygon Tests =====

TEST(SpatialTypesTest, PolygonCreation)
{
    std::vector<Point> exterior = {
        Point(0.0, 0.0),
        Point(4.0, 0.0),
        Point(4.0, 4.0),
        Point(0.0, 4.0),
        Point(0.0, 0.0)  // Closed ring
    };
    Polygon poly({exterior});

    EXPECT_EQ(poly.rings.size(), 1);
    EXPECT_TRUE(poly.isValid());
}

TEST(SpatialTypesTest, PolygonValidation)
{
    // Valid polygon (closed ring with 4+ points)
    std::vector<Point> valid_ring = {
        Point(0.0, 0.0),
        Point(1.0, 0.0),
        Point(1.0, 1.0),
        Point(0.0, 1.0),
        Point(0.0, 0.0)
    };
    Polygon valid_poly({valid_ring});
    EXPECT_TRUE(valid_poly.isValid());

    // Invalid polygon (not closed)
    std::vector<Point> unclosed_ring = {
        Point(0.0, 0.0),
        Point(1.0, 0.0),
        Point(1.0, 1.0),
        Point(0.0, 1.0)
    };
    Polygon invalid_poly({unclosed_ring});
    EXPECT_FALSE(invalid_poly.isValid());

    // Invalid polygon (< 4 points)
    std::vector<Point> short_ring = {
        Point(0.0, 0.0),
        Point(1.0, 0.0),
        Point(0.0, 0.0)
    };
    Polygon invalid_poly2({short_ring});
    EXPECT_FALSE(invalid_poly2.isValid());
}

TEST(SpatialTypesTest, PolygonWithHoles)
{
    std::vector<Point> exterior = {
        Point(0.0, 0.0),
        Point(10.0, 0.0),
        Point(10.0, 10.0),
        Point(0.0, 10.0),
        Point(0.0, 0.0)
    };
    std::vector<Point> hole = {
        Point(2.0, 2.0),
        Point(8.0, 2.0),
        Point(8.0, 8.0),
        Point(2.0, 8.0),
        Point(2.0, 2.0)
    };
    std::vector<std::vector<Point>> rings = {exterior, hole};
    Polygon poly(rings);

    EXPECT_EQ(poly.rings.size(), 2);
    EXPECT_TRUE(poly.isValid());
    EXPECT_TRUE(poly.hasHoles());  // Should have 1 interior ring (hole)
}

TEST(SpatialTypesTest, PolygonTypedValue)
{
    std::vector<Point> exterior = {
        Point(0.0, 0.0),
        Point(4.0, 0.0),
        Point(4.0, 4.0),
        Point(0.0, 4.0),
        Point(0.0, 0.0)
    };
    Polygon poly_obj({exterior});
    auto val = TypedValue::makePolygon(poly_obj);

    EXPECT_EQ(val.type(), DataType::POLYGON);
    auto poly = val.getPolygon();
    EXPECT_EQ(poly.rings.size(), 1);
    EXPECT_TRUE(poly.isValid());
}

TEST(SpatialTypesTest, PolygonToString)
{
    std::vector<Point> exterior = {
        Point(0.0, 0.0),
        Point(1.0, 0.0),
        Point(1.0, 1.0),
        Point(0.0, 1.0),
        Point(0.0, 0.0)
    };
    Polygon poly_obj({exterior});
    auto val = TypedValue::makePolygon(poly_obj);
    std::string str = val.toString();
    EXPECT_TRUE(str.find("POLYGON") != std::string::npos);
}

// ===== WKT Parser Tests =====

TEST(WKTParserTest, ParsePointBasic)
{
    auto pt = WKTParser::parsePoint("POINT(1.5 2.5)");
    ASSERT_TRUE(pt.has_value());
    EXPECT_DOUBLE_EQ(pt->x, 1.5);
    EXPECT_DOUBLE_EQ(pt->y, 2.5);
}

TEST(WKTParserTest, ParsePointWithSpaces)
{
    auto pt = WKTParser::parsePoint("POINT ( 1.5  2.5 )");
    ASSERT_TRUE(pt.has_value());
    EXPECT_DOUBLE_EQ(pt->x, 1.5);
    EXPECT_DOUBLE_EQ(pt->y, 2.5);
}

TEST(WKTParserTest, ParsePointCaseInsensitive)
{
    auto pt = WKTParser::parsePoint("point(1.5 2.5)");
    ASSERT_TRUE(pt.has_value());
    EXPECT_DOUBLE_EQ(pt->x, 1.5);
    EXPECT_DOUBLE_EQ(pt->y, 2.5);
}

TEST(WKTParserTest, ParsePointNegativeCoords)
{
    auto pt = WKTParser::parsePoint("POINT(-1.5 -2.5)");
    ASSERT_TRUE(pt.has_value());
    EXPECT_DOUBLE_EQ(pt->x, -1.5);
    EXPECT_DOUBLE_EQ(pt->y, -2.5);
}

TEST(WKTParserTest, ParsePointInvalid)
{
    ErrorContext ctx;
    auto pt = WKTParser::parsePoint("POINT(1.5)", &ctx);
    EXPECT_FALSE(pt.has_value());
}

TEST(WKTParserTest, ParseLineStringBasic)
{
    auto line = WKTParser::parseLineString("LINESTRING(0 0, 1 1, 2 2)");
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(line->points.size(), 3);
    EXPECT_TRUE(line->isValid());
}

TEST(WKTParserTest, ParseLineStringTwoPoints)
{
    auto line = WKTParser::parseLineString("LINESTRING(0 0, 1 1)");
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(line->points.size(), 2);
    EXPECT_TRUE(line->isValid());
}

TEST(WKTParserTest, ParseLineStringInvalid)
{
    ErrorContext ctx;
    // Only one point
    auto line = WKTParser::parseLineString("LINESTRING(0 0)", &ctx);
    EXPECT_FALSE(line.has_value());
}

TEST(WKTParserTest, ParsePolygonBasic)
{
    auto poly = WKTParser::parsePolygon("POLYGON((0 0, 4 0, 4 4, 0 4, 0 0))");
    ASSERT_TRUE(poly.has_value());
    EXPECT_EQ(poly->rings.size(), 1);
    EXPECT_EQ(poly->rings[0].size(), 5);
    EXPECT_TRUE(poly->isValid());
}

TEST(WKTParserTest, ParsePolygonWithHole)
{
    auto poly = WKTParser::parsePolygon(
        "POLYGON((0 0, 10 0, 10 10, 0 10, 0 0), (2 2, 8 2, 8 8, 2 8, 2 2))"
    );
    ASSERT_TRUE(poly.has_value());
    EXPECT_EQ(poly->rings.size(), 2);
    EXPECT_TRUE(poly->isValid());
    EXPECT_TRUE(poly->hasHoles());  // numInteriorRings() → hasHoles()
}

TEST(WKTParserTest, ParsePolygonInvalidUnclosed)
{
    ErrorContext ctx;
    auto poly = WKTParser::parsePolygon("POLYGON((0 0, 4 0, 4 4, 0 4))", &ctx);
    EXPECT_FALSE(poly.has_value());
}

TEST(WKTParserTest, PointToWKT)
{
    Point pt(1.5, 2.5);
    std::string wkt = WKTParser::pointToWKT(pt);
    EXPECT_EQ(wkt, "POINT(1.5 2.5)");
}

TEST(WKTParserTest, LineStringToWKT)
{
    LineString line(std::vector<Point>{Point(0, 0), Point(1, 1), Point(2, 2)});
    std::string wkt = WKTParser::lineStringToWKT(line);
    EXPECT_TRUE(wkt.find("LINESTRING") != std::string::npos);
    EXPECT_TRUE(wkt.find("0") != std::string::npos);
}

TEST(WKTParserTest, PolygonToWKT)
{
    std::vector<Point> ring = {
        Point(0, 0),
        Point(4, 0),
        Point(4, 4),
        Point(0, 4),
        Point(0, 0)
    };
    std::vector<std::vector<Point>> rings_vec = {ring};
    Polygon poly(rings_vec);
    std::string wkt = WKTParser::polygonToWKT(poly);
    EXPECT_TRUE(wkt.find("POLYGON") != std::string::npos);
}

TEST(WKTParserTest, WKTRoundTripPoint)
{
    Point original(1.5, 2.5);
    std::string wkt = WKTParser::pointToWKT(original);
    auto parsed = WKTParser::parsePoint(wkt);
    ASSERT_TRUE(parsed.has_value());
    EXPECT_DOUBLE_EQ(parsed->x, original.x);
    EXPECT_DOUBLE_EQ(parsed->y, original.y);
}

// ===== WKB Serialization Tests =====

TEST(WKBSerializerTest, SerializePointBasic)
{
    Point pt(1.5, 2.5);
    auto wkb = WKBSerializer::serializePoint(pt);
    EXPECT_FALSE(wkb.empty());
    EXPECT_EQ(wkb.size(), 21); // 1 byte order + 4 type + 16 doubles
}

TEST(WKBSerializerTest, DeserializePointBasic)
{
    Point original(1.5, 2.5);
    auto wkb = WKBSerializer::serializePoint(original);
    auto pt = WKBSerializer::deserializePoint(wkb);
    ASSERT_TRUE(pt.has_value());
    EXPECT_DOUBLE_EQ(pt->x, original.x);
    EXPECT_DOUBLE_EQ(pt->y, original.y);
}

TEST(WKBSerializerTest, SerializeLineStringBasic)
{
    LineString line(std::vector<Point>{Point(0, 0), Point(1, 1), Point(2, 2)});
    auto wkb = WKBSerializer::serializeLineString(line);
    EXPECT_FALSE(wkb.empty());
}

TEST(WKBSerializerTest, DeserializeLineStringBasic)
{
    LineString original(std::vector<Point>{Point(0, 0), Point(1, 1), Point(2, 2)});
    auto wkb = WKBSerializer::serializeLineString(original);
    auto line = WKBSerializer::deserializeLineString(wkb);
    ASSERT_TRUE(line.has_value());
    EXPECT_EQ(line->points.size(), original.points.size());
}

TEST(WKBSerializerTest, SerializePolygonBasic)
{
    std::vector<Point> ring = {
        Point(0, 0),
        Point(4, 0),
        Point(4, 4),
        Point(0, 4),
        Point(0, 0)
    };
    std::vector<std::vector<Point>> rings_vec = {ring};
    Polygon poly(rings_vec);
    auto wkb = WKBSerializer::serializePolygon(poly);
    EXPECT_FALSE(wkb.empty());
}

TEST(WKBSerializerTest, DeserializePolygonBasic)
{
    std::vector<Point> ring = {
        Point(0, 0),
        Point(4, 0),
        Point(4, 4),
        Point(0, 4),
        Point(0, 0)
    };
    std::vector<std::vector<Point>> rings_vec = {ring};
    Polygon original(rings_vec);
    auto wkb = WKBSerializer::serializePolygon(original);
    auto poly = WKBSerializer::deserializePolygon(wkb);
    ASSERT_TRUE(poly.has_value());
    EXPECT_EQ(poly->rings.size(), original.rings.size());
    EXPECT_TRUE(poly->isValid());
}

TEST(WKBSerializerTest, WKBRoundTripPoint)
{
    Point original(1.5, 2.5);
    auto wkb = WKBSerializer::serializePoint(original);
    auto result = WKBSerializer::deserializePoint(wkb);
    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->x, original.x);
    EXPECT_DOUBLE_EQ(result->y, original.y);
}

TEST(WKBSerializerTest, WKBRoundTripLineString)
{
    LineString original(std::vector<Point>{Point(0, 0), Point(1, 1), Point(2, 2)});
    auto wkb = WKBSerializer::serializeLineString(original);
    auto result = WKBSerializer::deserializeLineString(wkb);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->points.size(), original.points.size());
    for (size_t i = 0; i < original.points.size(); ++i) {
        EXPECT_DOUBLE_EQ(result->points[i].x, original.points[i].x);
        EXPECT_DOUBLE_EQ(result->points[i].y, original.points[i].y);
    }
}

TEST(WKBSerializerTest, WKBRoundTripPolygon)
{
    std::vector<Point> ring = {
        Point(0, 0),
        Point(4, 0),
        Point(4, 4),
        Point(0, 4),
        Point(0, 0)
    };
    std::vector<std::vector<Point>> rings_vec = {ring};
    Polygon original(rings_vec);
    auto wkb = WKBSerializer::serializePolygon(original);
    auto result = WKBSerializer::deserializePolygon(wkb);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->rings.size(), original.rings.size());
    EXPECT_EQ(result->rings[0].size(), original.rings[0].size());
}

TEST(WKBSerializerTest, WKBRoundTripPolygonWithHole)
{
    std::vector<Point> exterior = {
        Point(0, 0),
        Point(10, 0),
        Point(10, 10),
        Point(0, 10),
        Point(0, 0)
    };
    std::vector<Point> hole = {
        Point(2, 2),
        Point(8, 2),
        Point(8, 8),
        Point(2, 8),
        Point(2, 2)
    };
    std::vector<std::vector<Point>> rings = {exterior, hole};
    Polygon original(rings);

    auto wkb = WKBSerializer::serializePolygon(original);
    auto result = WKBSerializer::deserializePolygon(wkb);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->rings.size(), 2);
    EXPECT_TRUE(result->hasHoles());  // numInteriorRings() → hasHoles()
}

// ===== Type System Tests =====

TEST(TypeSystemTest, SpatialTypeNames)
{
    EXPECT_EQ(TypeSystem::getTypeName(DataType::POINT), "POINT");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::LINESTRING), "LINESTRING");
    EXPECT_EQ(TypeSystem::getTypeName(DataType::POLYGON), "POLYGON");
}

// NOTE: TypeSystem::parseTypeName() was removed from the API
// Use getTypeName() to get type names instead

// ===== Edge Cases and Error Handling =====

TEST(SpatialTypesTest, EmptyLineString)
{
    LineString empty;
    EXPECT_FALSE(empty.isValid());
}

TEST(SpatialTypesTest, EmptyPolygon)
{
    Polygon empty;
    EXPECT_TRUE(empty.isValid());
}

TEST(WKTParserTest, MalformedWKT)
{
    ErrorContext ctx;
    EXPECT_FALSE(WKTParser::parsePoint("NOT_A_POINT(1 2)", &ctx).has_value());
    EXPECT_FALSE(WKTParser::parseLineString("LINESTRING", &ctx).has_value());
    EXPECT_FALSE(WKTParser::parsePolygon("POLYGON()", &ctx).has_value());
}

TEST(WKBSerializerTest, InvalidWKBData)
{
    ErrorContext ctx;
    std::vector<uint8_t> invalid = {0x01, 0x02, 0x03}; // Too short
    EXPECT_FALSE(WKBSerializer::deserializePoint(invalid, &ctx).has_value());
}
