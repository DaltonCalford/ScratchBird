# Database Data Types Technical Reference

## Table of Contents
1. [FirebirdSQL Data Types](#firebirdsql-data-types)
2. [Microsoft SQL Server Data Types](#microsoft-sql-server-data-types)
3. [PostgreSQL Data Types](#postgresql-data-types)
4. [MySQL Data Types](#mysql-data-types)
5. [MariaDB Data Types](#mariadb-data-types)
6. [JDBC Type Mappings](#jdbc-type-mappings)
7. [ODBC Type Mappings](#odbc-type-mappings)

---

## FirebirdSQL Data Types

### Numeric Types

#### SMALLINT
- **Size**: 2 bytes
- **Range**: -32,768 to 32,767
- **Wire Protocol**: Sent as 16-bit signed integer, network byte order (big-endian)
- **Protocol Code**: SQL_SHORT (500)
- **Implementation**: `isc_vax_integer(buffer, value, 2)`

#### INTEGER
- **Size**: 4 bytes
- **Range**: -2,147,483,648 to 2,147,483,647
- **Wire Protocol**: 32-bit signed integer, network byte order
- **Protocol Code**: SQL_LONG (496)
- **Implementation**: `isc_vax_integer(buffer, value, 4)`

#### BIGINT
- **Size**: 8 bytes
- **Range**: -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807
- **Wire Protocol**: 64-bit signed integer, network byte order
- **Protocol Code**: SQL_INT64 (580)
- **Implementation**: `isc_vax_integer(buffer, value, 8)`

#### FLOAT
- **Size**: 4 bytes
- **Precision**: 7 decimal digits
- **Wire Protocol**: IEEE 754 single precision, network byte order
- **Protocol Code**: SQL_FLOAT (482)
- **Implementation**: `isc_portable_float(buffer, value)`

#### DOUBLE PRECISION
- **Size**: 8 bytes
- **Precision**: 15 decimal digits
- **Wire Protocol**: IEEE 754 double precision, network byte order
- **Protocol Code**: SQL_DOUBLE (480)
- **Implementation**: `isc_portable_double(buffer, value)`

#### NUMERIC(p,s) / DECIMAL(p,s)
- **Size**: 4, 8, or 16 bytes depending on precision
- **Precision**: Up to 38 digits (Firebird 4.0+)
- **Wire Protocol**: Scaled integer with separate scale indicator
- **Protocol Code**: SQL_INT64 + scale info (580)
- **Implementation**: 
  ```c
  // Stored as scaled integer
  int64_t stored_value = actual_value * pow(10, scale);
  // Scale transmitted in XSQLVAR.sqlscale (negative value)
  ```

#### DECFLOAT(16)
- **Size**: 8 bytes
- **Precision**: 16 decimal digits
- **Wire Protocol**: IEEE 754-2008 decimal64
- **Protocol Code**: SQL_DEC16 (32764)
- **Implementation**: BID (Binary Integer Decimal) encoding

#### DECFLOAT(34)
- **Size**: 16 bytes
- **Precision**: 34 decimal digits
- **Wire Protocol**: IEEE 754-2008 decimal128
- **Protocol Code**: SQL_DEC34 (32765)
- **Implementation**: BID encoding

### Character Types

#### CHAR(n)
- **Size**: n bytes (fixed)
- **Max Length**: 32,767 bytes
- **Wire Protocol**: Fixed-length byte array, padded with spaces
- **Protocol Code**: SQL_TEXT (452)
- **Implementation**: 
  ```c
  struct {
      short length;  // Actual length
      char data[n];  // Space-padded
  }
  ```

#### VARCHAR(n)
- **Size**: Variable, up to n bytes + 2 bytes length
- **Max Length**: 32,765 bytes
- **Wire Protocol**: Length prefix (2 bytes) + data
- **Protocol Code**: SQL_VARYING (448)
- **Implementation**:
  ```c
  struct {
      short length;
      char data[length];
  }
  ```

#### NCHAR(n)
- **Size**: n characters × bytes per character
- **Character Set**: UTF-8 or specified
- **Wire Protocol**: Same as CHAR with charset ID
- **Protocol Code**: SQL_TEXT (452) + charset info

### Binary Types

#### BLOB
- **Size**: Up to 4GB
- **Wire Protocol**: Blob ID (8 bytes) + segment streaming
- **Protocol Code**: SQL_BLOB (520)
- **Implementation**:
  ```c
  struct {
      ISC_QUAD blob_id;  // 8-byte blob identifier
      // Data fetched via separate API calls
      // isc_get_segment() / isc_put_segment()
  }
  ```
- **Subtypes**:
  - 0: Binary
  - 1: Text
  - 2: BLR (Binary Language Representation)

### Date/Time Types

#### DATE
- **Size**: 4 bytes
- **Range**: 0001-01-01 to 9999-12-31
- **Wire Protocol**: Days since 1858-11-17 (Modified Julian Date)
- **Protocol Code**: SQL_TYPE_DATE (510)
- **Implementation**: `ISC_DATE` (32-bit integer)

#### TIME
- **Size**: 4 bytes
- **Precision**: 0.0001 second
- **Wire Protocol**: Ten-thousandths of seconds since midnight
- **Protocol Code**: SQL_TYPE_TIME (560)
- **Implementation**: `ISC_TIME` (32-bit unsigned)

#### TIMESTAMP
- **Size**: 8 bytes
- **Wire Protocol**: DATE (4 bytes) + TIME (4 bytes)
- **Protocol Code**: SQL_TIMESTAMP (510)
- **Implementation**:
  ```c
  struct ISC_TIMESTAMP {
      ISC_DATE date;
      ISC_TIME time;
  }
  ```

#### TIME WITH TIME ZONE
- **Size**: 8 bytes
- **Wire Protocol**: TIME (4 bytes) + zone ID (2 bytes) + offset (2 bytes)
- **Protocol Code**: SQL_TIME_TZ (32756)
- **Implementation**: Extended time structure with timezone info

#### TIMESTAMP WITH TIME ZONE
- **Size**: 12 bytes
- **Wire Protocol**: TIMESTAMP (8 bytes) + zone info (4 bytes)
- **Protocol Code**: SQL_TIMESTAMP_TZ (32754)

### Boolean Type

#### BOOLEAN
- **Size**: 1 byte
- **Values**: TRUE, FALSE, NULL (Unknown)
- **Wire Protocol**: Single byte (0, 1, or NULL indicator)
- **Protocol Code**: SQL_BOOLEAN (32764)
- **Implementation**: `SCHAR` with values 0, 1

### Special Types

#### RDB$DB_KEY
- **Size**: 8 bytes
- **Purpose**: Physical row identifier
- **Wire Protocol**: 8-byte opaque value
- **Protocol Code**: SQL_DB_KEY (536)

---

## Microsoft SQL Server Data Types

### Numeric Types

#### BIT
- **Size**: 1 bit (packed into bytes)
- **Values**: 0, 1, NULL
- **TDS Protocol**: TYPE_BIT (0x32)
- **Wire Format**: Packed into bytes, 8 bits per byte

#### TINYINT
- **Size**: 1 byte
- **Range**: 0 to 255
- **TDS Protocol**: TYPE_INT1 (0x30)
- **Wire Format**: Single unsigned byte

#### SMALLINT
- **Size**: 2 bytes
- **Range**: -32,768 to 32,767
- **TDS Protocol**: TYPE_INT2 (0x34)
- **Wire Format**: 16-bit little-endian

#### INT
- **Size**: 4 bytes
- **Range**: -2^31 to 2^31-1
- **TDS Protocol**: TYPE_INT4 (0x38)
- **Wire Format**: 32-bit little-endian

#### BIGINT
- **Size**: 8 bytes
- **Range**: -2^63 to 2^63-1
- **TDS Protocol**: TYPE_INT8 (0x7F)
- **Wire Format**: 64-bit little-endian

#### DECIMAL(p,s) / NUMERIC(p,s)
- **Size**: 5-17 bytes depending on precision
- **Precision**: 1-38 digits
- **TDS Protocol**: TYPE_NUMERIC (0x6C), TYPE_DECIMAL (0x6A)
- **Wire Format**:
  ```c
  struct {
      BYTE precision;
      BYTE scale;
      BYTE sign;  // 1 = positive, 0 = negative
      BYTE data[n];  // Variable length based on precision
  }
  ```

#### SMALLMONEY
- **Size**: 4 bytes
- **Range**: -214,748.3648 to 214,748.3647
- **TDS Protocol**: TYPE_MONEY4 (0x7A)
- **Wire Format**: 32-bit integer, implied 4 decimal places

#### MONEY
- **Size**: 8 bytes
- **Range**: -922,337,203,685,477.5808 to 922,337,203,685,477.5807
- **TDS Protocol**: TYPE_MONEY (0x3C)
- **Wire Format**: Two 32-bit integers (high, low), implied 4 decimal places

#### FLOAT(n)
- **Size**: 4 or 8 bytes
- **TDS Protocol**: TYPE_FLT4 (0x3B) or TYPE_FLT8 (0x3E)
- **Wire Format**: IEEE 754, little-endian

#### REAL
- **Size**: 4 bytes
- **TDS Protocol**: TYPE_FLT4 (0x3B)
- **Wire Format**: IEEE 754 single precision, little-endian

### Character Types

#### CHAR(n)
- **Size**: n bytes
- **Max**: 8,000 bytes
- **TDS Protocol**: TYPE_CHAR (0x2F)
- **Wire Format**: Fixed-length, space-padded
- **Collation**: Included in metadata

#### VARCHAR(n)
- **Size**: Variable up to n
- **Max**: 8,000 bytes (or MAX = 2GB)
- **TDS Protocol**: TYPE_VARCHAR (0x27)
- **Wire Format**:
  ```c
  struct {
      USHORT length;  // Actual length
      BYTE data[length];
  }
  ```

#### VARCHAR(MAX)
- **Size**: Up to 2GB
- **TDS Protocol**: TYPE_BIGVARCHAR (0xA7)
- **Wire Format**: Partially Length Prefixed (PLP) format
  ```c
  struct {
      ULONGLONG total_length;  // 0xFFFFFFFFFFFFFFFE for NULL
      ULONG chunk_length;
      BYTE data[chunk_length];
      // Repeat chunks, 0 terminates
  }
  ```

#### NCHAR(n)
- **Size**: n × 2 bytes
- **Max**: 4,000 characters
- **TDS Protocol**: TYPE_NCHAR (0xEF)
- **Wire Format**: UTF-16LE, fixed-length

#### NVARCHAR(n)
- **Size**: Variable up to n × 2
- **Max**: 4,000 characters (or MAX)
- **TDS Protocol**: TYPE_NVARCHAR (0xE7)
- **Wire Format**: Length prefix + UTF-16LE data

#### TEXT (deprecated)
- **Size**: Up to 2GB
- **TDS Protocol**: TYPE_TEXT (0x23)
- **Wire Format**: Textptr (16 bytes) + timestamp (8 bytes) + data

### Binary Types

#### BINARY(n)
- **Size**: n bytes
- **Max**: 8,000 bytes
- **TDS Protocol**: TYPE_BINARY (0x2D)
- **Wire Format**: Fixed-length byte array

#### VARBINARY(n)
- **Size**: Variable up to n
- **Max**: 8,000 bytes (or MAX)
- **TDS Protocol**: TYPE_VARBINARY (0x25)
- **Wire Format**: Length prefix + data

#### VARBINARY(MAX)
- **Size**: Up to 2GB
- **TDS Protocol**: TYPE_BIGVARBINARY (0xA5)
- **Wire Format**: PLP format (same as VARCHAR(MAX))

#### IMAGE (deprecated)
- **Size**: Up to 2GB
- **TDS Protocol**: TYPE_IMAGE (0x22)
- **Wire Format**: Textptr + timestamp + data

### Date/Time Types

#### DATE
- **Size**: 3 bytes
- **Range**: 0001-01-01 to 9999-12-31
- **TDS Protocol**: TYPE_DATE (0x28)
- **Wire Format**: Days since 0001-01-01

#### TIME(n)
- **Size**: 3-5 bytes
- **Precision**: 0-7 (100ns units)
- **TDS Protocol**: TYPE_TIME (0x29)
- **Wire Format**:
  ```c
  struct {
      BYTE scale;  // Fractional precision
      BYTE time_bytes[3-5];  // Based on scale
  }
  ```

#### DATETIME
- **Size**: 8 bytes
- **Range**: 1753-01-01 to 9999-12-31
- **TDS Protocol**: TYPE_DATETIME (0x3D)
- **Wire Format**:
  ```c
  struct {
      LONG days;     // Days since 1900-01-01
      ULONG ticks;   // 300ths of a second since midnight
  }
  ```

#### DATETIME2(n)
- **Size**: 6-8 bytes
- **Precision**: 0-7
- **TDS Protocol**: TYPE_DATETIME2 (0x2A)
- **Wire Format**: Time component + date component

#### SMALLDATETIME
- **Size**: 4 bytes
- **TDS Protocol**: TYPE_DATETIME4 (0x3A)
- **Wire Format**:
  ```c
  struct {
      USHORT days;    // Days since 1900-01-01
      USHORT minutes; // Minutes since midnight
  }
  ```

#### DATETIMEOFFSET(n)
- **Size**: 8-10 bytes
- **TDS Protocol**: TYPE_DATETIMEOFFSET (0x2B)
- **Wire Format**: DATETIME2 + timezone offset (minutes)

### Special Types

#### UNIQUEIDENTIFIER
- **Size**: 16 bytes
- **TDS Protocol**: TYPE_GUID (0x24)
- **Wire Format**: 16-byte GUID, specific byte order
  ```c
  struct {
      DWORD Data1;
      WORD Data2;
      WORD Data3;
      BYTE Data4[8];
  }
  ```

#### SQL_VARIANT
- **Size**: Up to 8,016 bytes
- **TDS Protocol**: TYPE_VARIANT (0x62)
- **Wire Format**:
  ```c
  struct {
      ULONG total_length;
      BYTE type;
      BYTE properties[varies];
      BYTE data[varies];
  }
  ```

#### XML
- **Size**: Up to 2GB
- **TDS Protocol**: TYPE_XML (0xF1)
- **Wire Format**: PLP format with schema collection info

#### HIERARCHYID
- **Size**: Variable, up to 892 bytes
- **TDS Protocol**: TYPE_UDT (0xF0)
- **Wire Format**: CLR UDT serialization

#### GEOGRAPHY / GEOMETRY
- **Size**: Variable
- **TDS Protocol**: TYPE_UDT (0xF0)
- **Wire Format**: Well-Known Binary (WKB) format in CLR UDT

---

## PostgreSQL Data Types

### Numeric Types

#### SMALLINT (INT2)
- **Size**: 2 bytes
- **OID**: 21
- **Wire Protocol**: Network byte order (big-endian)
- **Binary Format**: 2-byte integer
- **Text Format**: ASCII decimal

#### INTEGER (INT4)
- **Size**: 4 bytes
- **OID**: 23
- **Wire Protocol**: Network byte order
- **Binary Format**: 4-byte integer
- **Implementation**: `pq_sendint32(buf, value)`

#### BIGINT (INT8)
- **Size**: 8 bytes
- **OID**: 20
- **Wire Protocol**: Network byte order
- **Binary Format**: 8-byte integer
- **Implementation**: `pq_sendint64(buf, value)`

#### NUMERIC / DECIMAL
- **Size**: Variable
- **OID**: 1700
- **Wire Protocol Format**:
  ```c
  struct {
      int16 ndigits;    // Number of digits
      int16 weight;     // Weight of first digit
      int16 sign;       // NUMERIC_POS, NUMERIC_NEG, NUMERIC_NAN
      int16 dscale;     // Display scale
      int16 digits[ndigits]; // Base-10000 digits
  }
  ```

#### REAL (FLOAT4)
- **Size**: 4 bytes
- **OID**: 700
- **Wire Protocol**: IEEE 754, network byte order
- **Implementation**: `pq_sendfloat4(buf, value)`

#### DOUBLE PRECISION (FLOAT8)
- **Size**: 8 bytes
- **OID**: 701
- **Wire Protocol**: IEEE 754, network byte order
- **Implementation**: `pq_sendfloat8(buf, value)`

#### SERIAL / BIGSERIAL
- **Not actual types**: INTEGER/BIGINT with sequence
- **OID**: Same as base type

### Character Types

#### CHAR(n) / CHARACTER(n)
- **Size**: Fixed n bytes
- **OID**: 1042 (bpchar)
- **Wire Protocol**: Length + data (space-padded)
- **Binary Format**:
  ```c
  struct {
      int32 length;  // Always n
      char data[n];  // Space-padded
  }
  ```

#### VARCHAR(n) / CHARACTER VARYING(n)
- **Size**: Variable up to n
- **OID**: 1043
- **Wire Protocol**: Length + data
- **Binary Format**:
  ```c
  struct {
      int32 length;
      char data[length];
  }
  ```

#### TEXT
- **Size**: Variable, up to 1GB
- **OID**: 25
- **Wire Protocol**: Same as VARCHAR
- **Binary Format**: Length prefix + UTF-8 data

### Binary Types

#### BYTEA
- **Size**: Variable, up to 1GB
- **OID**: 17
- **Wire Protocol Binary**:
  ```c
  struct {
      int32 length;
      byte data[length];
  }
  ```
- **Text Format**: Hex (\x...) or escape format

### Date/Time Types

#### DATE
- **Size**: 4 bytes
- **OID**: 1082
- **Wire Protocol**: Days since 2000-01-01
- **Binary Format**: 32-bit integer

#### TIME
- **Size**: 8 bytes
- **OID**: 1083
- **Wire Protocol**: Microseconds since midnight
- **Binary Format**: 64-bit integer

#### TIMESTAMP
- **Size**: 8 bytes
- **OID**: 1114
- **Wire Protocol**: Microseconds since 2000-01-01 00:00:00
- **Binary Format**: 64-bit integer

#### TIMESTAMPTZ
- **Size**: 8 bytes
- **OID**: 1184
- **Wire Protocol**: Microseconds since 2000-01-01 00:00:00 UTC
- **Binary Format**: 64-bit integer (always UTC)

#### INTERVAL
- **Size**: 16 bytes
- **OID**: 1186
- **Wire Protocol**:
  ```c
  struct {
      int64 time;    // Microseconds
      int32 day;     // Days
      int32 month;   // Months
  }
  ```

### Boolean Type

#### BOOLEAN
- **Size**: 1 byte
- **OID**: 16
- **Wire Protocol**: Single byte (0, 1)
- **Text Format**: 't', 'f'

### Geometric Types

#### POINT
- **Size**: 16 bytes
- **OID**: 600
- **Wire Protocol**:
  ```c
  struct {
      float8 x;
      float8 y;
  }
  ```

#### LINE
- **Size**: 24 bytes
- **OID**: 628
- **Wire Protocol**: Three float8 values (A, B, C for Ax + By + C = 0)

#### BOX
- **Size**: 32 bytes
- **OID**: 603
- **Wire Protocol**: Two points (high, low)

#### POLYGON
- **Size**: Variable
- **OID**: 604
- **Wire Protocol**:
  ```c
  struct {
      int32 npoints;
      Point points[npoints];
  }
  ```

### Network Types

#### INET
- **Size**: 7 or 19 bytes
- **OID**: 869
- **Wire Protocol**:
  ```c
  struct {
      uint8 family;    // AF_INET or AF_INET6
      uint8 bits;      // Netmask bits
      uint8 is_cidr;   // 0 or 1
      uint8 nb;        // Address bytes (4 or 16)
      uint8 addr[nb];  // IP address
  }
  ```

#### CIDR
- **Size**: 7 or 19 bytes
- **OID**: 650
- **Wire Protocol**: Same as INET with is_cidr = 1

#### MACADDR
- **Size**: 6 bytes
- **OID**: 829
- **Wire Protocol**: 6-byte MAC address

#### MACADDR8
- **Size**: 8 bytes
- **OID**: 774
- **Wire Protocol**: 8-byte MAC address

### UUID Type

#### UUID
- **Size**: 16 bytes
- **OID**: 2950
- **Wire Protocol**: 16-byte UUID
- **Binary Format**: Raw 16 bytes
- **Text Format**: Standard UUID string

### JSON Types

#### JSON
- **Size**: Variable
- **OID**: 114
- **Wire Protocol**: Length + UTF-8 JSON text
- **Storage**: Text representation

#### JSONB
- **Size**: Variable
- **OID**: 3802
- **Wire Protocol**: Length + binary JSON
- **Binary Format**:
  ```c
  struct {
      uint32 version;  // Format version (1)
      // Followed by compressed binary representation
  }
  ```

### Array Types

#### Arrays
- **OID**: Base type OID + array indicator
- **Wire Protocol**:
  ```c
  struct {
      int32 ndim;      // Number of dimensions
      int32 flags;     // Has nulls flag
      Oid element_type;
      int32 dim[ndim]; // Dimension sizes
      int32 lbound[ndim]; // Lower bounds
      // Followed by elements with lengths
  }
  ```

### Range Types

#### INT4RANGE
- **OID**: 3904
- **Wire Protocol**:
  ```c
  struct {
      uint8 flags;     // Bounds flags
      int32 lower;     // If has lower
      int32 upper;     // If has upper
  }
  ```

### Special Types

#### OID
- **Size**: 4 bytes
- **OID**: 26
- **Purpose**: Object identifier
- **Wire Protocol**: 32-bit unsigned integer

#### XID
- **Size**: 4 bytes
- **OID**: 28
- **Purpose**: Transaction ID
- **Wire Protocol**: 32-bit unsigned integer

#### PG_LSN
- **Size**: 8 bytes
- **OID**: 3220
- **Purpose**: Log Sequence Number
- **Wire Protocol**: 64-bit unsigned integer

---

## MySQL Data Types

### Numeric Types

#### BIT(M)
- **Size**: (M+7)/8 bytes
- **Range**: M bits, 1-64
- **Protocol Type**: MYSQL_TYPE_BIT (16)
- **Wire Format**: Packed bits, little-endian

#### TINYINT
- **Size**: 1 byte
- **Range**: -128 to 127 (signed), 0 to 255 (unsigned)
- **Protocol Type**: MYSQL_TYPE_TINY (1)
- **Wire Format**: Single byte

#### BOOL / BOOLEAN
- **Alias for**: TINYINT(1)
- **Protocol Type**: MYSQL_TYPE_TINY (1)

#### SMALLINT
- **Size**: 2 bytes
- **Range**: -32,768 to 32,767 (signed)
- **Protocol Type**: MYSQL_TYPE_SHORT (2)
- **Wire Format**: 2 bytes, little-endian

#### MEDIUMINT
- **Size**: 3 bytes
- **Range**: -8,388,608 to 8,388,607
- **Protocol Type**: MYSQL_TYPE_INT24 (9)
- **Wire Format**: 3 bytes, little-endian

#### INT / INTEGER
- **Size**: 4 bytes
- **Range**: -2^31 to 2^31-1
- **Protocol Type**: MYSQL_TYPE_LONG (3)
- **Wire Format**: 4 bytes, little-endian

#### BIGINT
- **Size**: 8 bytes
- **Range**: -2^63 to 2^63-1
- **Protocol Type**: MYSQL_TYPE_LONGLONG (8)
- **Wire Format**: 8 bytes, little-endian

#### DECIMAL(M,D) / NUMERIC(M,D)
- **Size**: Variable
- **Protocol Type**: MYSQL_TYPE_NEWDECIMAL (246)
- **Wire Format**:
  ```c
  // Length-encoded string representation
  struct {
      uint8 length;  // Or 2-3 bytes for longer
      char digits[length];  // ASCII decimal
  }
  ```

#### FLOAT
- **Size**: 4 bytes
- **Protocol Type**: MYSQL_TYPE_FLOAT (4)
- **Wire Format**: IEEE 754 single, little-endian

#### DOUBLE / REAL
- **Size**: 8 bytes
- **Protocol Type**: MYSQL_TYPE_DOUBLE (5)
- **Wire Format**: IEEE 754 double, little-endian

### Character Types

#### CHAR(M)
- **Size**: M × character_set_bytes
- **Max**: 255 characters
- **Protocol Type**: MYSQL_TYPE_STRING (254)
- **Wire Format**: Length-encoded string
- **Charset**: Included in column metadata

#### VARCHAR(M)
- **Size**: L + 1 or L + 2 bytes
- **Max**: 65,535 bytes
- **Protocol Type**: MYSQL_TYPE_VAR_STRING (253)
- **Wire Format**:
  ```c
  struct {
      uint8/uint16 length;  // 1 or 2 bytes
      char data[length];
  }
  ```

#### TINYTEXT
- **Size**: L + 1 byte
- **Max**: 255 bytes
- **Protocol Type**: MYSQL_TYPE_TINY_BLOB (249)
- **Wire Format**: 1-byte length + data

#### TEXT
- **Size**: L + 2 bytes
- **Max**: 65,535 bytes
- **Protocol Type**: MYSQL_TYPE_BLOB (252)
- **Wire Format**: 2-byte length + data

#### MEDIUMTEXT
- **Size**: L + 3 bytes
- **Max**: 16,777,215 bytes
- **Protocol Type**: MYSQL_TYPE_MEDIUM_BLOB (250)
- **Wire Format**: 3-byte length + data

#### LONGTEXT
- **Size**: L + 4 bytes
- **Max**: 4,294,967,295 bytes
- **Protocol Type**: MYSQL_TYPE_LONG_BLOB (251)
- **Wire Format**: 4-byte length + data

### Binary Types

#### BINARY(M)
- **Size**: M bytes
- **Max**: 255 bytes
- **Protocol Type**: MYSQL_TYPE_STRING (254)
- **Wire Format**: Fixed-length, zero-padded

#### VARBINARY(M)
- **Size**: L + 1 or L + 2 bytes
- **Max**: 65,535 bytes
- **Protocol Type**: MYSQL_TYPE_VAR_STRING (253)
- **Wire Format**: Length prefix + data

#### TINYBLOB
- **Size**: L + 1 byte
- **Max**: 255 bytes
- **Protocol Type**: MYSQL_TYPE_TINY_BLOB (249)

#### BLOB
- **Size**: L + 2 bytes
- **Max**: 65,535 bytes
- **Protocol Type**: MYSQL_TYPE_BLOB (252)

#### MEDIUMBLOB
- **Size**: L + 3 bytes
- **Max**: 16,777,215 bytes
- **Protocol Type**: MYSQL_TYPE_MEDIUM_BLOB (250)

#### LONGBLOB
- **Size**: L + 4 bytes
- **Max**: 4,294,967,295 bytes
- **Protocol Type**: MYSQL_TYPE_LONG_BLOB (251)

### Date/Time Types

#### DATE
- **Size**: 3 bytes
- **Range**: '1000-01-01' to '9999-12-31'
- **Protocol Type**: MYSQL_TYPE_DATE (10)
- **Wire Format**:
  ```c
  struct {
      uint16 year;
      uint8 month;
      uint8 day;
  }
  ```

#### TIME
- **Size**: 3 bytes + fractional
- **Range**: '-838:59:59' to '838:59:59'
- **Protocol Type**: MYSQL_TYPE_TIME (11)
- **Wire Format**:
  ```c
  struct {
      uint8 is_negative;
      uint32 days;
      uint8 hour;
      uint8 minute;
      uint8 second;
      uint32 microsecond;  // If fractional
  }
  ```

#### DATETIME
- **Size**: 5 bytes + fractional
- **Range**: '1000-01-01 00:00:00' to '9999-12-31 23:59:59'
- **Protocol Type**: MYSQL_TYPE_DATETIME (12)
- **Wire Format**: Packed integer format

#### TIMESTAMP
- **Size**: 4 bytes + fractional
- **Range**: '1970-01-01 00:00:01' UTC to '2038-01-19 03:14:07' UTC
- **Protocol Type**: MYSQL_TYPE_TIMESTAMP (7)
- **Wire Format**: Unix timestamp

#### YEAR
- **Size**: 1 byte
- **Range**: 1901 to 2155
- **Protocol Type**: MYSQL_TYPE_YEAR (13)
- **Wire Format**: Year - 1900

### JSON Type

#### JSON
- **Size**: Variable
- **Max**: 1GB
- **Protocol Type**: MYSQL_TYPE_JSON (245)
- **Wire Format**:
  ```c
  struct {
      uint32 length;
      uint8 json_type;  // Object, array, etc.
      // Binary JSON format follows
  }
  ```

### Spatial Types

#### GEOMETRY
- **Protocol Type**: MYSQL_TYPE_GEOMETRY (255)
- **Wire Format**: SRID (4 bytes) + WKB format

#### POINT
- **Subtype of**: GEOMETRY
- **Wire Format**: SRID + WKB Point

#### LINESTRING
- **Subtype of**: GEOMETRY
- **Wire Format**: SRID + WKB LineString

#### POLYGON
- **Subtype of**: GEOMETRY
- **Wire Format**: SRID + WKB Polygon

### Special Types

#### ENUM
- **Size**: 1 or 2 bytes
- **Max Values**: 65,535
- **Protocol Type**: MYSQL_TYPE_STRING (254)
- **Wire Format**: Index value (1 or 2 bytes)

#### SET
- **Size**: 1, 2, 3, 4, or 8 bytes
- **Max Members**: 64
- **Protocol Type**: MYSQL_TYPE_STRING (254)
- **Wire Format**: Bitmap of set members

---

## MariaDB Data Types

*MariaDB extends MySQL types with additional features*

### MariaDB-Specific Types

#### INET4
- **Size**: 4 bytes
- **Purpose**: IPv4 address
- **Wire Format**: 4-byte IP address
- **Protocol**: Extended type info in metadata

#### INET6
- **Size**: 16 bytes
- **Purpose**: IPv6 address
- **Wire Format**: 16-byte IP address

#### UUID
- **Size**: 16 bytes
- **Purpose**: UUID storage
- **Wire Format**: 16-byte UUID
- **Note**: Stored as BINARY(16) with functions

### Extended JSON Features

#### JSON with Constraints
- **Check Constraints**: JSON schema validation
- **Wire Format**: Same as MySQL JSON
- **Additional**: Validation on server side

### Temporal Extensions

#### DATETIME with Precision
- **Fractional Seconds**: Up to 6 digits
- **Wire Format**: Base + microseconds
- **Size**: 5-8 bytes depending on precision

### Dynamic Columns

#### COLUMN_JSON
- **Purpose**: Dynamic schema
- **Wire Format**: Binary JSON-like format
- **Functions**: COLUMN_CREATE, COLUMN_GET

### Sequences

#### SEQUENCE
- **Not a data type**: But a schema object
- **Values**: BIGINT internally
- **Wire Format**: As BIGINT when fetched

---

## JDBC Type Mappings

### JDBC to SQL Type Mapping

```java
// java.sql.Types constants and their mappings

// Numeric Types
Types.BIT         (-7)  -> BIT, BOOLEAN
Types.TINYINT     (-6)  -> TINYINT
Types.SMALLINT    (5)   -> SMALLINT, INT2
Types.INTEGER     (4)   -> INTEGER, INT, INT4
Types.BIGINT      (-5)  -> BIGINT, INT8
Types.FLOAT       (6)   -> FLOAT, REAL
Types.REAL        (7)   -> REAL, FLOAT4
Types.DOUBLE      (8)   -> DOUBLE PRECISION, FLOAT8
Types.NUMERIC     (2)   -> NUMERIC, NUMBER
Types.DECIMAL     (3)   -> DECIMAL

// Character Types
Types.CHAR        (1)   -> CHAR, CHARACTER
Types.VARCHAR     (12)  -> VARCHAR, VARCHAR2
Types.LONGVARCHAR (-1)  -> TEXT, LONGTEXT, CLOB
Types.NCHAR       (-15) -> NCHAR
Types.NVARCHAR    (-9)  -> NVARCHAR
Types.LONGNVARCHAR(-16) -> NTEXT, NCLOB

// Binary Types
Types.BINARY      (-2)  -> BINARY, RAW
Types.VARBINARY   (-3)  -> VARBINARY, BYTEA
Types.LONGVARBINARY(-4) -> BLOB, LONGBLOB, IMAGE

// Date/Time Types
Types.DATE        (91)  -> DATE
Types.TIME        (92)  -> TIME
Types.TIMESTAMP   (93)  -> TIMESTAMP, DATETIME
Types.TIME_WITH_TIMEZONE     (2013) -> TIME WITH TIME ZONE
Types.TIMESTAMP_WITH_TIMEZONE(2014) -> TIMESTAMP WITH TIME ZONE

// Special Types
Types.NULL        (0)   -> NULL
Types.OTHER       (1111)-> Database-specific
Types.JAVA_OBJECT(2000) -> Serialized Java object
Types.DISTINCT    (2001)-> User-defined type
Types.STRUCT      (2002)-> Composite type
Types.ARRAY       (2003)-> Array type
Types.BLOB        (2004)-> BLOB
Types.CLOB        (2005)-> CLOB
Types.REF         (2006)-> Reference type
Types.DATALINK    (70)  -> URL
Types.BOOLEAN     (16)  -> BOOLEAN
Types.ROWID       (-8)  -> ROWID
Types.NCLOB       (2011)-> NCLOB
Types.SQLXML      (2009)-> XML
```

### JDBC ResultSet Methods

```java
// Type-specific getters and their wire protocol handling

// Numeric
getByte()     -> 1 byte signed
getShort()    -> 2 bytes, network/host order conversion
getInt()      -> 4 bytes, network/host order conversion
getLong()     -> 8 bytes, network/host order conversion
getFloat()    -> 4 bytes IEEE 754
getDouble()   -> 8 bytes IEEE 754
getBigDecimal()-> Variable length, scale preserved

// Character
getString()   -> UTF-16 Java String
getNString()  -> UTF-16 with national charset
getAsciiStream() -> InputStream of ASCII bytes
getCharacterStream() -> Reader of Unicode

// Binary
getBytes()    -> byte[]
getBinaryStream() -> InputStream
getBlob()     -> Blob interface

// Date/Time
getDate()     -> java.sql.Date (days since epoch)
getTime()     -> java.sql.Time (milliseconds)
getTimestamp()-> java.sql.Timestamp (nanoseconds)

// Special
getObject()   -> Appropriate Java object
getArray()    -> java.sql.Array
getURL()      -> java.net.URL
getRowId()    -> java.sql.RowId
getSQLXML()   -> java.sql.SQLXML
```

### JDBC PreparedStatement Parameter Setting

```java
// Parameter setters and their wire protocol encoding

// Numeric
setByte(int parameterIndex, byte x)
  -> Single byte

setShort(int parameterIndex, short x)
  -> 2 bytes, converted to network order

setInt(int parameterIndex, int x)
  -> 4 bytes, converted to network order

setLong(int parameterIndex, long x)
  -> 8 bytes, converted to network order

setBigDecimal(int parameterIndex, BigDecimal x)
  -> String representation or binary format

// Character
setString(int parameterIndex, String x)
  -> UTF-8/UTF-16 based on database

setNString(int parameterIndex, String x)
  -> National character set encoding

// Binary
setBytes(int parameterIndex, byte[] x)
  -> Raw bytes with length prefix

setBinaryStream(int parameterIndex, InputStream x)
  -> Streamed with chunking

// Date/Time
setDate(int parameterIndex, Date x)
  -> Days since database epoch

setTimestamp(int parameterIndex, Timestamp x)
  -> Microseconds/nanoseconds since epoch

// Special
setNull(int parameterIndex, int sqlType)
  -> NULL indicator with type info

setObject(int parameterIndex, Object x)
  -> Type inference and conversion
```

---

## ODBC Type Mappings

### ODBC SQL Data Types

```c
// From sql.h and sqlext.h

// Core SQL Types
#define SQL_CHAR            1
#define SQL_NUMERIC         2
#define SQL_DECIMAL         3
#define SQL_INTEGER         4
#define SQL_SMALLINT        5
#define SQL_FLOAT           6
#define SQL_REAL            7
#define SQL_DOUBLE          8
#define SQL_DATETIME        9  // Deprecated
#define SQL_VARCHAR        12

// Extended Types
#define SQL_TYPE_DATE      91
#define SQL_TYPE_TIME      92
#define SQL_TYPE_TIMESTAMP 93
#define SQL_INTERVAL       10

// Binary Types
#define SQL_BINARY         -2
#define SQL_VARBINARY      -3
#define SQL_LONGVARBINARY  -4

// Unicode Types
#define SQL_WCHAR          -8
#define SQL_WVARCHAR       -9
#define SQL_WLONGVARCHAR  -10

// Large Objects
#define SQL_BLOB          -98
#define SQL_CLOB          -99
#define SQL_BLOB_LOCATOR -100
#define SQL_CLOB_LOCATOR -101

// Special Types
#define SQL_BIGINT        -5
#define SQL_TINYINT       -6
#define SQL_BIT           -7
#define SQL_GUID         -11
```

### ODBC C Data Types

```c
// C type identifiers for binding

#define SQL_C_CHAR           SQL_CHAR
#define SQL_C_SSHORT         SQL_SMALLINT
#define SQL_C_USHORT         -17
#define SQL_C_SLONG          SQL_INTEGER
#define SQL_C_ULONG          -18
#define SQL_C_FLOAT          SQL_REAL
#define SQL_C_DOUBLE         SQL_DOUBLE
#define SQL_C_BIT            SQL_BIT
#define SQL_C_STINYINT       SQL_TINYINT
#define SQL_C_UTINYINT       -28
#define SQL_C_SBIGINT        SQL_BIGINT
#define SQL_C_UBIGINT        -27
#define SQL_C_BINARY         SQL_BINARY
#define SQL_C_BOOKMARK       -18
#define SQL_C_VARBOOKMARK    -23

// Date/Time C Types
#define SQL_C_TYPE_DATE      SQL_TYPE_DATE
#define SQL_C_TYPE_TIME      SQL_TYPE_TIME
#define SQL_C_TYPE_TIMESTAMP SQL_TYPE_TIMESTAMP

// Structure Types
#define SQL_C_NUMERIC        SQL_NUMERIC
#define SQL_C_GUID           SQL_GUID
```

### ODBC Data Structures

```c
// Date/Time structures
typedef struct tagDATE_STRUCT {
    SQLSMALLINT year;
    SQLUSMALLINT month;
    SQLUSMALLINT day;
} DATE_STRUCT;

typedef struct tagTIME_STRUCT {
    SQLUSMALLINT hour;
    SQLUSMALLINT minute;
    SQLUSMALLINT second;
} TIME_STRUCT;

typedef struct tagTIMESTAMP_STRUCT {
    SQLSMALLINT year;
    SQLUSMALLINT month;
    SQLUSMALLINT day;
    SQLUSMALLINT hour;
    SQLUSMALLINT minute;
    SQLUSMALLINT second;
    SQLUINTEGER fraction;  // Nanoseconds
} TIMESTAMP_STRUCT;

// Numeric structure
typedef struct tagSQL_NUMERIC_STRUCT {
    SQLCHAR precision;
    SQLSCHAR scale;
    SQLCHAR sign;  // 1=positive, 0=negative
    SQLCHAR val[SQL_MAX_NUMERIC_LEN];
} SQL_NUMERIC_STRUCT;

// GUID structure
typedef struct tagSQLGUID {
    DWORD Data1;
    WORD Data2;
    WORD Data3;
    BYTE Data4[8];
} SQLGUID;
```

### ODBC Buffer Binding

```c
// Binding examples for different types

// Integer binding
SQLINTEGER value;
SQLLEN indicator;
SQLBindCol(hstmt, 1, SQL_C_SLONG, &value, 
           sizeof(value), &indicator);

// String binding
SQLCHAR buffer[256];
SQLLEN length;
SQLBindCol(hstmt, 2, SQL_C_CHAR, buffer, 
           sizeof(buffer), &length);

// Binary binding
SQLCHAR binary[1024];
SQLLEN binary_length;
SQLBindCol(hstmt, 3, SQL_C_BINARY, binary,
           sizeof(binary), &binary_length);

// Timestamp binding
TIMESTAMP_STRUCT ts;
SQLLEN ts_ind;
SQLBindCol(hstmt, 4, SQL_C_TYPE_TIMESTAMP, &ts,
           sizeof(ts), &ts_ind);

// Numeric binding
SQL_NUMERIC_STRUCT num;
SQLLEN num_ind;
SQLBindCol(hstmt, 5, SQL_C_NUMERIC, &num,
           sizeof(num), &num_ind);
```

### ODBC Parameter Binding

```c
// Parameter binding for prepared statements

// Integer parameter
SQLINTEGER param_value = 42;
SQLLEN param_ind = 0;
SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT,
                SQL_C_SLONG, SQL_INTEGER,
                0, 0, &param_value, 0, &param_ind);

// String parameter
SQLCHAR param_string[50] = "Hello";
SQLLEN str_ind = SQL_NTS;  // Null-terminated
SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT,
                SQL_C_CHAR, SQL_VARCHAR,
                50, 0, param_string, 0, &str_ind);

// Binary parameter
SQLCHAR param_binary[100];
SQLLEN binary_ind = 100;
SQLBindParameter(hstmt, 3, SQL_PARAM_INPUT,
                SQL_C_BINARY, SQL_VARBINARY,
                100, 0, param_binary, 100, &binary_ind);

// Output parameter
SQLINTEGER output_value;
SQLLEN output_ind;
SQLBindParameter(hstmt, 4, SQL_PARAM_OUTPUT,
                SQL_C_SLONG, SQL_INTEGER,
                0, 0, &output_value, 0, &output_ind);
```

### ODBC Type Conversion Rules

```c
// Implicit conversions supported by ODBC

// Numeric conversions
SQL_C_CHAR    <-> All SQL types (string representation)
SQL_C_SSHORT  <-> SQL_SMALLINT, SQL_INTEGER, SQL_REAL
SQL_C_SLONG   <-> SQL_INTEGER, SQL_SMALLINT, SQL_BIGINT
SQL_C_FLOAT   <-> SQL_REAL, SQL_DOUBLE, SQL_NUMERIC
SQL_C_DOUBLE  <-> SQL_DOUBLE, SQL_REAL, SQL_NUMERIC

// String conversions
SQL_C_CHAR    <-> SQL_CHAR, SQL_VARCHAR, SQL_LONGVARCHAR
SQL_C_WCHAR   <-> SQL_WCHAR, SQL_WVARCHAR
SQL_C_BINARY  <-> SQL_BINARY, SQL_VARBINARY

// Date/Time conversions
SQL_C_TYPE_DATE      <-> SQL_TYPE_DATE, SQL_TYPE_TIMESTAMP
SQL_C_TYPE_TIME      <-> SQL_TYPE_TIME, SQL_TYPE_TIMESTAMP
SQL_C_TYPE_TIMESTAMP <-> SQL_TYPE_TIMESTAMP, SQL_TYPE_DATE
```

---

## Wire Protocol Considerations

### Byte Order (Endianness)

**Network Byte Order (Big-Endian)**:
- PostgreSQL
- Firebird
- Standard network protocols

**Little-Endian**:
- SQL Server (TDS Protocol)
- MySQL/MariaDB

**Conversion Functions**:
```c
// Host to Network
uint32_t htonl(uint32_t hostlong);
uint16_t htons(uint16_t hostshort);

// Network to Host
uint32_t ntohl(uint32_t netlong);
uint16_t ntohs(uint16_t netshort);
```

### Length Encoding

**Fixed Length**:
- Most numeric types
- Fixed-length character types

**Length Prefix Formats**:
```c
// 1-byte length (max 255)
struct {
    uint8_t length;
    char data[length];
};

// 2-byte length (max 65535)
struct {
    uint16_t length;
    char data[length];
};

// 4-byte length (max 4GB)
struct {
    uint32_t length;
    char data[length];
};

// Variable length encoding (MySQL)
// 1 byte:   0-250
// 2 bytes:  251-65535
// 3 bytes:  65536-16777215
// 9 bytes:  > 16777215
```

### NULL Handling

**Null Indicators**:
```c
// Separate null bitmap (PostgreSQL)
struct {
    uint8_t null_bitmap[(ncols + 7) / 8];
    // Followed by non-null values
};

// Per-value indicator (ODBC)
SQLLEN indicator;  // SQL_NULL_DATA or length

// In-band null (MySQL)
0xFB in length-encoded integer = NULL
```

### Character Encoding

**UTF-8**:
- PostgreSQL default
- MySQL/MariaDB with utf8mb4

**UTF-16**:
- SQL Server NCHAR/NVARCHAR (Little-Endian)
- Some JDBC implementations

**Collation Information**:
- Transmitted in column metadata
- Affects sorting and comparison

### Compression

**Protocol-Level**:
- MySQL: zlib compression
- PostgreSQL: Optional SSL compression

**Column-Level**:
- SQL Server: Page/row compression
- PostgreSQL: TOAST compression

### Type Safety

**Strong Typing**:
- Type information in result set metadata
- Parameter type declaration

**Type Coercion Rules**:
- Implicit conversions
- Precision/scale preservation
- Overflow handling

---

## Notes

1. **Version Differences**: Data type implementations may vary between database versions
2. **Driver Variations**: JDBC/ODBC drivers may handle types differently
3. **Platform Dependencies**: Some aspects depend on OS and hardware architecture
4. **Performance Implications**: Wire format affects network overhead and parsing cost
5. **Compatibility Considerations**: Not all types are portable between systems