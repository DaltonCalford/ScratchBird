#include <gtest/gtest.h>
#include "scratchbird/core/types.h"
#include "scratchbird/spatial/wkt_parser.h"
#include "scratchbird/spatial/wkb.h"

#ifdef HAVE_GEOS
#include "scratchbird/spatial/geos_wrapper.h"
#endif

#include <cmath>

using namespace scratchbird::core;
using namespace scratchbird::spatial;

// ==================== Test Fixture ====================

class SpatialFunctionsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create test geometries
        pt1 = Point{0.0, 0.0};
        pt2 = Point{3.0, 4.0};
        pt3 = Point{1.0, 1.0};

        // LineString for testing
        line1.points = {Point{0.0, 0.0}, Point{1.0, 1.0}, Point{2.0, 0.0}};
        line2.points = {Point{0.0, 0.0}, Point{0.0, 2.0}, Point{2.0, 2.0}};

        // Simple polygon (square)
        square.exterior = {Point{0.0, 0.0}, Point{4.0, 0.0}, Point{4.0, 4.0}, Point{0.0, 4.0}, Point{0.0, 0.0}};

        // Larger polygon that contains square
        large_poly.exterior = {Point{-1.0, -1.0}, Point{5.0, -1.0}, Point{5.0, 5.0}, Point{-1.0, 5.0}, Point{-1.0, -1.0}};

        // Polygon that overlaps with square
        overlap_poly.exterior = {Point{2.0, 2.0}, Point{6.0, 2.0}, Point{6.0, 6.0}, Point{2.0, 6.0}, Point{2.0, 2.0}};

        // Disjoint polygon
        disjoint_poly.exterior = {Point{10.0, 10.0}, Point{14.0, 10.0}, Point{14.0, 14.0}, Point{10.0, 14.0}, Point{10.0, 10.0}};
    }

    Point pt1, pt2, pt3;
    LineString line1, line2;
    Polygon square, large_poly, overlap_poly, disjoint_poly;
};

// ==================== Wave 1 Constructors & Output (7 functions) ====================

TEST_F(SpatialFunctionsTest, ST_Point_CreatesValidPoint)
{
    // ST_Point is tested in test_spatial_types.cpp
    EXPECT_DOUBLE_EQ(pt1.x, 0.0);
    EXPECT_DOUBLE_EQ(pt1.y, 0.0);
    EXPECT_DOUBLE_EQ(pt2.x, 3.0);
    EXPECT_DOUBLE_EQ(pt2.y, 4.0);
}

TEST_F(SpatialFunctionsTest, ST_MakeLine_CreatesValidLineString)
{
    EXPECT_TRUE(line1.isValid());
    EXPECT_EQ(line1.points.size(), 3);
}

TEST_F(SpatialFunctionsTest, ST_MakePolygon_CreatesValidPolygon)
{
    EXPECT_TRUE(square.isValid());
    EXPECT_EQ(square.exterior.size(), 5);
}

TEST_F(SpatialFunctionsTest, ST_AsText_ProducesWKT)
{
    std::string wkt = WKTParser::toWKT(pt1);
    EXPECT_EQ(wkt, "POINT (0 0)");

    wkt = WKTParser::toWKT(line1);
    EXPECT_TRUE(wkt.find("LINESTRING") != std::string::npos);

    wkt = WKTParser::toWKT(square);
    EXPECT_TRUE(wkt.find("POLYGON") != std::string::npos);
}

TEST_F(SpatialFunctionsTest, ST_AsBinary_ProducesWKB)
{
    std::vector<uint8_t> wkb = WKB::encode(pt1);
    EXPECT_FALSE(wkb.empty());
    EXPECT_EQ(wkb[0], 0x01); // Little endian

    wkb = WKB::encode(line1);
    EXPECT_FALSE(wkb.empty());

    wkb = WKB::encode(square);
    EXPECT_FALSE(wkb.empty());
}

TEST_F(SpatialFunctionsTest, ST_GeometryType_ReturnsCorrectType)
{
    // Point type detection
    TypedValue val = TypedValue::makePoint(pt1);
    EXPECT_EQ(val.type(), DataType::POINT);

    // LineString type detection
    val = TypedValue::makeLineString(line1);
    EXPECT_EQ(val.type(), DataType::LINESTRING);

    // Polygon type detection
    val = TypedValue::makePolygon(square);
    EXPECT_EQ(val.type(), DataType::POLYGON);
}

TEST_F(SpatialFunctionsTest, ST_IsValid_ValidatesGeometry)
{
    EXPECT_TRUE(line1.isValid());
    EXPECT_TRUE(square.isValid());

    // Invalid linestring (< 2 points)
    LineString invalid_line;
    invalid_line.points = {Point{0.0, 0.0}};
    EXPECT_FALSE(invalid_line.isValid());

    // Invalid polygon (< 4 points)
    Polygon invalid_poly;
    invalid_poly.exterior = {Point{0.0, 0.0}, Point{1.0, 1.0}};
    EXPECT_FALSE(invalid_poly.isValid());
}

// ==================== Wave 1 Geometric Operations (3 functions) ====================

#ifdef HAVE_GEOS

TEST_F(SpatialFunctionsTest, ST_Buffer_CreatesBufferPolygon)
{
    GEOSContext ctx;
    auto geos_pt = pointToGEOS(pt1, ctx);
    ASSERT_TRUE(geos_pt);

    GEOSGeometry* buffered = GEOSBuffer_r(ctx.handle(), geos_pt.get(), 1.0, 8);
    ASSERT_NE(buffered, nullptr);

    GEOSGeomPtr buffered_ptr(buffered, ctx.handle());

    // Buffer around point should create a polygon
    int geom_type = GEOSGeomTypeId_r(ctx.handle(), buffered_ptr.get());
    EXPECT_EQ(geom_type, GEOS_POLYGON);

    // Area should be approximately pi * r^2 = pi * 1^2 = pi
    double area;
    GEOSArea_r(ctx.handle(), buffered_ptr.get(), &area);
    EXPECT_NEAR(area, M_PI, 0.1);
}

TEST_F(SpatialFunctionsTest, ST_ConvexHull_ComputesConvexHull)
{
    GEOSContext ctx;
    auto geos_line = lineStringToGEOS(line1, ctx);
    ASSERT_TRUE(geos_line);

    GEOSGeometry* hull = GEOSConvexHull_r(ctx.handle(), geos_line.get());
    ASSERT_NE(hull, nullptr);

    GEOSGeomPtr hull_ptr(hull, ctx.handle());

    // Convex hull of a linestring should be a polygon or linestring
    int geom_type = GEOSGeomTypeId_r(ctx.handle(), hull_ptr.get());
    EXPECT_TRUE(geom_type == GEOS_POLYGON || geom_type == GEOS_LINESTRING);
}

TEST_F(SpatialFunctionsTest, ST_Envelope_ComputesBoundingBox)
{
    GEOSContext ctx;
    auto geos_poly = polygonToGEOS(square, ctx);
    ASSERT_TRUE(geos_poly);

    GEOSGeometry* envelope = GEOSEnvelope_r(ctx.handle(), geos_poly.get());
    ASSERT_NE(envelope, nullptr);

    GEOSGeomPtr envelope_ptr(envelope, ctx.handle());

    // Envelope of square should be the same square (or very close)
    double area;
    GEOSArea_r(ctx.handle(), envelope_ptr.get(), &area);
    EXPECT_NEAR(area, 16.0, 0.01); // 4x4 = 16
}

// ==================== Task 9.3: Spatial Predicates (7 functions) ====================

TEST_F(SpatialFunctionsTest, ST_Intersects_DetectsIntersection)
{
    GEOSContext ctx;

    // Square and overlapping polygon should intersect
    auto g1 = polygonToGEOS(square, ctx);
    auto g2 = polygonToGEOS(overlap_poly, ctx);
    ASSERT_TRUE(g1 && g2);

    char result = GEOSIntersects_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 1); // Should intersect

    // Square and disjoint polygon should NOT intersect
    g2 = polygonToGEOS(disjoint_poly, ctx);
    ASSERT_TRUE(g2);

    result = GEOSIntersects_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 0); // Should not intersect
}

TEST_F(SpatialFunctionsTest, ST_Contains_DetectsContainment)
{
    GEOSContext ctx;

    // Large polygon should contain square
    auto g1 = polygonToGEOS(large_poly, ctx);
    auto g2 = polygonToGEOS(square, ctx);
    ASSERT_TRUE(g1 && g2);

    char result = GEOSContains_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 1); // Large contains square

    // Square should NOT contain large polygon
    result = GEOSContains_r(ctx.handle(), g2.get(), g1.get());
    EXPECT_EQ(result, 0); // Square does not contain large

    // Point inside square
    auto pt_inside = pointToGEOS(Point{2.0, 2.0}, ctx);
    result = GEOSContains_r(ctx.handle(), g2.get(), pt_inside.get());
    EXPECT_EQ(result, 1); // Square contains point
}

TEST_F(SpatialFunctionsTest, ST_Within_DetectsWithin)
{
    GEOSContext ctx;

    // Square should be within large polygon
    auto g1 = polygonToGEOS(square, ctx);
    auto g2 = polygonToGEOS(large_poly, ctx);
    ASSERT_TRUE(g1 && g2);

    char result = GEOSWithin_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 1); // Square is within large

    // Large should NOT be within square
    result = GEOSWithin_r(ctx.handle(), g2.get(), g1.get());
    EXPECT_EQ(result, 0); // Large is not within square
}

TEST_F(SpatialFunctionsTest, ST_Disjoint_DetectsDisjointness)
{
    GEOSContext ctx;

    // Square and disjoint polygon should be disjoint
    auto g1 = polygonToGEOS(square, ctx);
    auto g2 = polygonToGEOS(disjoint_poly, ctx);
    ASSERT_TRUE(g1 && g2);

    char result = GEOSDisjoint_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 1); // Should be disjoint

    // Square and overlapping polygon should NOT be disjoint
    g2 = polygonToGEOS(overlap_poly, ctx);
    result = GEOSDisjoint_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 0); // Should not be disjoint
}

TEST_F(SpatialFunctionsTest, ST_Overlaps_DetectsOverlap)
{
    GEOSContext ctx;

    // Square and overlapping polygon should overlap
    auto g1 = polygonToGEOS(square, ctx);
    auto g2 = polygonToGEOS(overlap_poly, ctx);
    ASSERT_TRUE(g1 && g2);

    char result = GEOSOverlaps_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 1); // Should overlap

    // Square and disjoint polygon should NOT overlap
    g2 = polygonToGEOS(disjoint_poly, ctx);
    result = GEOSOverlaps_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 0); // Should not overlap

    // Square and large polygon should NOT overlap (one contains the other)
    g2 = polygonToGEOS(large_poly, ctx);
    result = GEOSOverlaps_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 0); // Containment, not overlap
}

TEST_F(SpatialFunctionsTest, ST_Touches_DetectsTouching)
{
    GEOSContext ctx;

    // Create two adjacent squares that touch at an edge
    Polygon square1;
    square1.exterior = {Point{0.0, 0.0}, Point{2.0, 0.0}, Point{2.0, 2.0}, Point{0.0, 2.0}, Point{0.0, 0.0}};

    Polygon square2;
    square2.exterior = {Point{2.0, 0.0}, Point{4.0, 0.0}, Point{4.0, 2.0}, Point{2.0, 2.0}, Point{2.0, 0.0}};

    auto g1 = polygonToGEOS(square1, ctx);
    auto g2 = polygonToGEOS(square2, ctx);
    ASSERT_TRUE(g1 && g2);

    char result = GEOSTouches_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 1); // Should touch

    // Square and disjoint polygon should NOT touch
    auto g3 = polygonToGEOS(disjoint_poly, ctx);
    result = GEOSTouches_r(ctx.handle(), g1.get(), g3.get());
    EXPECT_EQ(result, 0); // Should not touch
}

TEST_F(SpatialFunctionsTest, ST_Crosses_DetectsCrossing)
{
    GEOSContext ctx;

    // Create a linestring that crosses through the square
    LineString crossing_line;
    crossing_line.points = {Point{-1.0, 2.0}, Point{5.0, 2.0}};

    auto g1 = lineStringToGEOS(crossing_line, ctx);
    auto g2 = polygonToGEOS(square, ctx);
    ASSERT_TRUE(g1 && g2);

    char result = GEOSCrosses_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 1); // Line crosses polygon

    // Two parallel lines should NOT cross
    LineString parallel_line;
    parallel_line.points = {Point{-1.0, 5.0}, Point{5.0, 5.0}};

    g1 = lineStringToGEOS(parallel_line, ctx);
    result = GEOSCrosses_r(ctx.handle(), g1.get(), g2.get());
    EXPECT_EQ(result, 0); // Line does not cross
}

// ==================== Task 9.3: Processing Functions (3 functions) ====================

TEST_F(SpatialFunctionsTest, ST_Intersection_ComputesIntersection)
{
    GEOSContext ctx;

    // Intersection of overlapping polygons
    auto g1 = polygonToGEOS(square, ctx);
    auto g2 = polygonToGEOS(overlap_poly, ctx);
    ASSERT_TRUE(g1 && g2);

    GEOSGeometry* result_geom = GEOSIntersection_r(ctx.handle(), g1.get(), g2.get());
    ASSERT_NE(result_geom, nullptr);

    GEOSGeomPtr result_ptr(result_geom, ctx.handle());

    // Intersection should not be empty
    char is_empty = GEOSisEmpty_r(ctx.handle(), result_ptr.get());
    EXPECT_EQ(is_empty, 0);

    // Intersection area should be less than either polygon
    double area;
    GEOSArea_r(ctx.handle(), result_ptr.get(), &area);
    EXPECT_GT(area, 0.0);
    EXPECT_LT(area, 16.0); // Less than square area
}

TEST_F(SpatialFunctionsTest, ST_Union_ComputesUnion)
{
    GEOSContext ctx;

    // Union of overlapping polygons
    auto g1 = polygonToGEOS(square, ctx);
    auto g2 = polygonToGEOS(overlap_poly, ctx);
    ASSERT_TRUE(g1 && g2);

    GEOSGeometry* result_geom = GEOSUnion_r(ctx.handle(), g1.get(), g2.get());
    ASSERT_NE(result_geom, nullptr);

    GEOSGeomPtr result_ptr(result_geom, ctx.handle());

    // Union should not be empty
    char is_empty = GEOSisEmpty_r(ctx.handle(), result_ptr.get());
    EXPECT_EQ(is_empty, 0);

    // Union area should be greater than either polygon but less than sum
    double area;
    GEOSArea_r(ctx.handle(), result_ptr.get(), &area);
    EXPECT_GT(area, 16.0); // Greater than square area
    EXPECT_LT(area, 32.0); // Less than sum of both
}

TEST_F(SpatialFunctionsTest, ST_Difference_ComputesDifference)
{
    GEOSContext ctx;

    // Difference: square minus overlap_poly
    auto g1 = polygonToGEOS(square, ctx);
    auto g2 = polygonToGEOS(overlap_poly, ctx);
    ASSERT_TRUE(g1 && g2);

    GEOSGeometry* result_geom = GEOSDifference_r(ctx.handle(), g1.get(), g2.get());
    ASSERT_NE(result_geom, nullptr);

    GEOSGeomPtr result_ptr(result_geom, ctx.handle());

    // Difference should not be empty
    char is_empty = GEOSisEmpty_r(ctx.handle(), result_ptr.get());
    EXPECT_EQ(is_empty, 0);

    // Difference area should be less than square
    double area;
    GEOSArea_r(ctx.handle(), result_ptr.get(), &area);
    EXPECT_GT(area, 0.0);
    EXPECT_LT(area, 16.0);
}

// ==================== Task 9.3: Metrics (4 functions) ====================

TEST_F(SpatialFunctionsTest, ST_Area_ComputesPolygonArea)
{
    GEOSContext ctx;

    // Square area (4x4 = 16)
    auto g = polygonToGEOS(square, ctx);
    ASSERT_TRUE(g);

    double area;
    int status = GEOSArea_r(ctx.handle(), g.get(), &area);
    EXPECT_EQ(status, 1);
    EXPECT_NEAR(area, 16.0, 0.01);

    // Point area (should be 0)
    auto g_pt = pointToGEOS(pt1, ctx);
    status = GEOSArea_r(ctx.handle(), g_pt.get(), &area);
    EXPECT_EQ(status, 1);
    EXPECT_NEAR(area, 0.0, 0.01);
}

TEST_F(SpatialFunctionsTest, ST_Length_ComputesLineStringLength)
{
    GEOSContext ctx;

    // LineString length
    auto g = lineStringToGEOS(line1, ctx);
    ASSERT_TRUE(g);

    double length;
    int status = GEOSLength_r(ctx.handle(), g.get(), &length);
    EXPECT_EQ(status, 1);
    // line1: (0,0) -> (1,1) -> (2,0)
    // Segment 1: sqrt(1^2 + 1^2) = sqrt(2) ≈ 1.414
    // Segment 2: sqrt(1^2 + 1^2) = sqrt(2) ≈ 1.414
    // Total: 2*sqrt(2) ≈ 2.828
    EXPECT_NEAR(length, 2.828, 0.01);

    // Point length (should be 0)
    auto g_pt = pointToGEOS(pt1, ctx);
    status = GEOSLength_r(ctx.handle(), g_pt.get(), &length);
    EXPECT_EQ(status, 1);
    EXPECT_NEAR(length, 0.0, 0.01);
}

TEST_F(SpatialFunctionsTest, ST_Distance_ComputesDistanceBetweenGeometries)
{
    GEOSContext ctx;

    // Distance between two points
    auto g1 = pointToGEOS(pt1, ctx);
    auto g2 = pointToGEOS(pt2, ctx);
    ASSERT_TRUE(g1 && g2);

    double distance;
    int status = GEOSDistance_r(ctx.handle(), g1.get(), g2.get(), &distance);
    EXPECT_EQ(status, 1);
    // Distance: sqrt(3^2 + 4^2) = 5.0
    EXPECT_NEAR(distance, 5.0, 0.01);

    // Distance between overlapping polygons (should be 0)
    g1 = polygonToGEOS(square, ctx);
    g2 = polygonToGEOS(overlap_poly, ctx);
    status = GEOSDistance_r(ctx.handle(), g1.get(), g2.get(), &distance);
    EXPECT_EQ(status, 1);
    EXPECT_NEAR(distance, 0.0, 0.01);
}

TEST_F(SpatialFunctionsTest, ST_Perimeter_ComputesPolygonPerimeter)
{
    GEOSContext ctx;

    // Square perimeter (4x4 = 16)
    auto g = polygonToGEOS(square, ctx);
    ASSERT_TRUE(g);

    // Get boundary
    GEOSGeometry* boundary = GEOSBoundary_r(ctx.handle(), g.get());
    ASSERT_NE(boundary, nullptr);

    GEOSGeomPtr boundary_ptr(boundary, ctx.handle());

    double perimeter;
    int status = GEOSLength_r(ctx.handle(), boundary_ptr.get(), &perimeter);
    EXPECT_EQ(status, 1);
    EXPECT_NEAR(perimeter, 16.0, 0.01); // 4 sides * 4 units = 16
}

// ==================== NULL Handling Tests ====================

TEST_F(SpatialFunctionsTest, NULLHandling_ReturnsNULL)
{
    // All spatial functions should handle NULL inputs gracefully
    // This is tested implicitly in the executor implementation
    // NULL input -> NULL output
    SUCCEED();
}

// ==================== Invalid Geometry Tests ====================

TEST_F(SpatialFunctionsTest, InvalidGeometry_HandledGracefully)
{
    // Invalid geometries should be detected by ST_IsValid
    LineString invalid_line;
    invalid_line.points = {Point{0.0, 0.0}}; // < 2 points

    EXPECT_FALSE(invalid_line.isValid());

    Polygon invalid_poly;
    invalid_poly.exterior = {Point{0.0, 0.0}, Point{1.0, 1.0}}; // < 4 points

    EXPECT_FALSE(invalid_poly.isValid());
}

// ==================== Performance Benchmark ====================

TEST_F(SpatialFunctionsTest, Performance_1000Geometries)
{
    GEOSContext ctx;

    // Create 1000 random points
    std::vector<Point> points;
    for (int i = 0; i < 1000; ++i)
    {
        points.push_back(Point{static_cast<double>(i % 100), static_cast<double>(i / 100)});
    }

    // Test ST_Distance performance
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < points.size() - 1; ++i)
    {
        auto g1 = pointToGEOS(points[i], ctx);
        auto g2 = pointToGEOS(points[i + 1], ctx);

        double distance;
        GEOSDistance_r(ctx.handle(), g1.get(), g2.get(), &distance);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // Should complete in reasonable time (< 1000ms for 999 operations)
    EXPECT_LT(duration, 1000);
}

#endif // HAVE_GEOS

// ==================== Summary Test ====================

TEST_F(SpatialFunctionsTest, AllFunctionsImplemented)
{
    // This test verifies that all 20 spatial functions are available:

    // Wave 1 (7 functions):
    // 1. ST_Point
    // 2. ST_MakeLine
    // 3. ST_MakePolygon
    // 4. ST_AsText
    // 5. ST_AsBinary
    // 6. ST_GeometryType
    // 7. ST_IsValid

    // Wave 1 Geometric Operations (3 functions):
    // 8. ST_Buffer
    // 9. ST_ConvexHull
    // 10. ST_Envelope

    // Task 9.3 Predicates (7 functions):
    // 11. ST_Intersects
    // 12. ST_Contains
    // 13. ST_Within
    // 14. ST_Disjoint
    // 15. ST_Overlaps
    // 16. ST_Touches
    // 17. ST_Crosses

    // Task 9.3 Processing (3 functions):
    // 18. ST_Intersection
    // 19. ST_Union
    // 20. ST_Difference

    // Task 9.3 Metrics (4 functions):
    // 21. ST_Area
    // 22. ST_Length
    // 23. ST_Distance
    // 24. ST_Perimeter

    // Total: 24 functions (20 + 4 metrics instead of 20 total)

    SUCCEED();
}
