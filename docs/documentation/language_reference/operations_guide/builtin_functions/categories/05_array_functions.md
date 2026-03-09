# Array Functions

[Categories README](./README.md)

## Synopsis

Functions for creating and manipulating arrays.

## Array Creation

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `ARRAY[elem1, elem2, ...]` | Array constructor | `ARRAY[1, 2, 3]` | `{1,2,3}` |
| `array_cat(arr1, arr2)` | Concatenate arrays | `array_cat(ARRAY[1,2], ARRAY[3,4])` | `{1,2,3,4}` |
| `array_append(arr, elem)` | Append element | `array_append(ARRAY[1,2], 3)` | `{1,2,3}` |
| `array_prepend(elem, arr)` | Prepend element | `array_prepend(0, ARRAY[1,2])` | `{0,1,2}` |

## Array Information

| Function | Description | Example | Result |
|----------|-------------|---------|--------|
| `array_dims(arr)` | Dimensions | `array_dims(ARRAY[[1,2],[3,4]])` | `[1:2][1:2]` |
| `array_length(arr, dim)` | Length of dimension | `array_length(ARRAY[1,2,3], 1)` | `3` |
| `array_lower(arr, dim)` | Lower bound | `array_lower(ARRAY[1,2,3], 1)` | `1` |
| `array_upper(arr, dim)` | Upper bound | `array_upper(ARRAY[1,2,3], 1)` | `3` |
| `array_ndims(arr)` | Number of dimensions | `array_ndims(ARRAY[1,2,3])` | `1` |
| `cardinality(arr)` | Total elements | `cardinality(ARRAY[1,2,3])` | `3` |

## Array Conversion

| Function | Description | Example |
|----------|-------------|---------|
| `string_to_array(str, delim)` | String to array | `string_to_array('a,b,c', ',')` |
| `array_to_string(arr, delim)` | Array to string | `array_to_string(ARRAY[1,2,3], ',')` |
| `unnest(arr)` | Expand to rows | `SELECT * FROM unnest(ARRAY[1,2,3])` |

## Array Operations

| Function | Description | Example |
|----------|-------------|---------|
| `arr @> arr2` | Contains | `ARRAY[1,2,3] @> ARRAY[1,2]` |
| `arr <@ arr2` | Contained by | `ARRAY[1,2] <@ ARRAY[1,2,3]` |
| `arr && arr2` | Overlap | `ARRAY[1,2] && ARRAY[2,3]` |

## Examples

```sql
-- Create and access array
SELECT ARRAY['a', 'b', 'c'];

-- Access element
SELECT ARRAY['a', 'b', 'c'][1];  -- 'a'

-- Slice
SELECT ARRAY['a', 'b', 'c', 'd'][2:3];  -- {'b','c'}

-- Update element
UPDATE users SET tags[1] = 'new_tag' WHERE id = 1;

-- Search in array
SELECT * FROM users WHERE 'admin' = ANY(roles);

-- Array aggregation
SELECT array_agg(name) FROM users;
```
