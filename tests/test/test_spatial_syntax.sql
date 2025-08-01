-- ScratchBird Spatial Index Syntax Test
-- This file contains test cases for the new RTREE index syntax

-- Basic RTREE index creation
CREATE INDEX idx_basic_spatial ON test_table USING RTREE (geometry_column);

-- RTREE index with SRID option
CREATE INDEX idx_srid_spatial ON locations USING RTREE (point_geom) (SRID = 4326);

-- RTREE index with split strategy option
CREATE INDEX idx_strategy_spatial ON polygons USING RTREE (boundary) (SPLIT_STRATEGY = 'QUADRATIC');

-- RTREE index with multiple options
CREATE INDEX idx_full_spatial ON spatial_data 
USING RTREE (geom) 
(SRID = 3857, SPLIT_STRATEGY = 'RSTAR');

-- Unique spatial index (rare but syntactically valid)
CREATE UNIQUE INDEX idx_unique_spatial ON survey_points 
USING RTREE (location) 
(SRID = 4326);

-- Test IF NOT EXISTS syntax
CREATE INDEX IF NOT EXISTS idx_conditional_spatial ON features
USING RTREE (shape)
(SRID = 4269, SPLIT_STRATEGY = 'LINEAR');

-- Test with qualified table names
CREATE INDEX idx_schema_spatial ON myschema.geotable 
USING RTREE (geometry) 
(SRID = 2154);

-- Test mixed case and quoted identifiers
CREATE INDEX "Spatial_Index_Test" ON "GeoData" 
USING RTREE ("GeometryColumn") 
(SRID = 4326, SPLIT_STRATEGY = 'QUADRATIC');