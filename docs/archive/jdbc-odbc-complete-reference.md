# JDBC and ODBC Complete Implementation Reference

# JDBC Type System and Implementation

## JDBC Architecture

```java
// JDBC Driver Types
// Type 1: JDBC-ODBC Bridge Driver
// Type 2: Native-API Driver (requires native library)
// Type 3: Network Protocol Driver (middleware)
// Type 4: Thin Driver (pure Java)
```

## JDBC Type Mappings

### SQL to Java Type Mappings

```java
// java.sql.Types class constants
public class Types {
    public static final int BIT             = -7;
    public static final int TINYINT         = -6;
    public static final int SMALLINT        = 5;
    public static final int INTEGER         = 4;
    public static final int BIGINT          = -5;
    public static final int FLOAT           = 6;
    public static final int REAL            = 7;
    public static final int DOUBLE          = 8;
    public static final int NUMERIC         = 2;
    public static final int DECIMAL         = 3;
    public static final int CHAR            = 1;
    public static final int VARCHAR         = 12;
    public static final int LONGVARCHAR     = -1;
    public static final int DATE            = 91;
    public static final int TIME            = 92;
    public static final int TIMESTAMP       = 93;
    public static final int BINARY          = -2;
    public static final int VARBINARY       = -3;
    public static final int LONGVARBINARY   = -4;
    public static final int NULL            = 0;
    public static final int OTHER           = 1111;
    public static final int JAVA_OBJECT     = 2000;
    public static final int DISTINCT        = 2001;
    public static final int STRUCT          = 2002;
    public static final int ARRAY           = 2003;
    public static final int BLOB            = 2004;
    public static final int CLOB            = 2005;
    public static final int REF             = 2006;
    public static final int DATALINK        = 70;
    public static final int BOOLEAN         = 16;
    public static final int ROWID           = -8;
    public static final int NCHAR           = -15;
    public static final int NVARCHAR        = -9;
    public static final int LONGNVARCHAR    = -16;
    public static final int NCLOB           = 2011;
    public static final int SQLXML          = 2009;
    public static final int REF_CURSOR      = 2012;
    public static final int TIME_WITH_TIMEZONE = 2013;
    public static final int TIMESTAMP_WITH_TIMEZONE = 2014;
}
```

### Java to SQL Type Conversions

```java
// Type conversion matrix
public class TypeConverter {
    
    // Java primitive to SQL
    public static int getSQLType(Class<?> javaType) {
        if (javaType == Boolean.class || javaType == boolean.class) 
            return Types.BOOLEAN;
        if (javaType == Byte.class || javaType == byte.class) 
            return Types.TINYINT;
        if (javaType == Short.class || javaType == short.class) 
            return Types.SMALLINT;
        if (javaType == Integer.class || javaType == int.class) 
            return Types.INTEGER;
        if (javaType == Long.class || javaType == long.class) 
            return Types.BIGINT;
        if (javaType == Float.class || javaType == float.class) 
            return Types.REAL;
        if (javaType == Double.class || javaType == double.class) 
            return Types.DOUBLE;
        if (javaType == BigDecimal.class) 
            return Types.DECIMAL;
        if (javaType == String.class) 
            return Types.VARCHAR;
        if (javaType == byte[].class) 
            return Types.VARBINARY;
        if (javaType == java.sql.Date.class) 
            return Types.DATE;
        if (javaType == java.sql.Time.class) 
            return Types.TIME;
        if (javaType == java.sql.Timestamp.class) 
            return Types.TIMESTAMP;
        if (javaType == java.sql.Blob.class) 
            return Types.BLOB;
        if (javaType == java.sql.Clob.class) 
            return Types.CLOB;
        if (javaType == java.sql.Array.class) 
            return Types.ARRAY;
        if (javaType == java.sql.Struct.class) 
            return Types.STRUCT;
        if (javaType == java.sql.SQLXML.class) 
            return Types.SQLXML;
        return Types.OTHER;
    }
}
```

## ResultSet Implementation

### ResultSet Type Handling

```java
public class ResultSetImpl implements ResultSet {
    
    private byte[][] rowData;
    private int currentRow;
    private ResultSetMetaData metadata;
    
    // Type-specific getters with wire protocol handling
    
    @Override
    public boolean getBoolean(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return false;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.BIT:
            case Types.BOOLEAN:
                return data[0] != 0;
            case Types.TINYINT:
            case Types.SMALLINT:
            case Types.INTEGER:
            case Types.BIGINT:
                return getLong(columnIndex) != 0;
            case Types.CHAR:
            case Types.VARCHAR:
                String s = getString(columnIndex);
                return "true".equalsIgnoreCase(s) || "1".equals(s);
            default:
                throw new SQLException("Cannot convert to boolean");
        }
    }
    
    @Override
    public byte getByte(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return 0;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.TINYINT:
                return data[0];
            case Types.SMALLINT:
                return (byte) bytesToShort(data);
            case Types.INTEGER:
                return (byte) bytesToInt(data);
            case Types.BIGINT:
                return (byte) bytesToLong(data);
            case Types.REAL:
            case Types.FLOAT:
                return (byte) getFloat(columnIndex);
            case Types.DOUBLE:
                return (byte) getDouble(columnIndex);
            case Types.DECIMAL:
            case Types.NUMERIC:
                return getBigDecimal(columnIndex).byteValue();
            default:
                return Byte.parseByte(getString(columnIndex));
        }
    }
    
    @Override
    public short getShort(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return 0;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.TINYINT:
                return (short) (data[0] & 0xFF);
            case Types.SMALLINT:
                return bytesToShort(data);
            case Types.INTEGER:
                return (short) bytesToInt(data);
            case Types.BIGINT:
                return (short) bytesToLong(data);
            default:
                return Short.parseShort(getString(columnIndex));
        }
    }
    
    @Override
    public int getInt(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return 0;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.TINYINT:
                return data[0] & 0xFF;
            case Types.SMALLINT:
                return bytesToShort(data);
            case Types.INTEGER:
                return bytesToInt(data);
            case Types.BIGINT:
                return (int) bytesToLong(data);
            case Types.REAL:
            case Types.FLOAT:
                return (int) getFloat(columnIndex);
            case Types.DOUBLE:
                return (int) getDouble(columnIndex);
            case Types.DECIMAL:
            case Types.NUMERIC:
                return getBigDecimal(columnIndex).intValue();
            default:
                return Integer.parseInt(getString(columnIndex));
        }
    }
    
    @Override
    public long getLong(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return 0;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.TINYINT:
                return data[0] & 0xFF;
            case Types.SMALLINT:
                return bytesToShort(data);
            case Types.INTEGER:
                return bytesToInt(data);
            case Types.BIGINT:
                return bytesToLong(data);
            default:
                return Long.parseLong(getString(columnIndex));
        }
    }
    
    @Override
    public float getFloat(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return 0;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.REAL:
            case Types.FLOAT:
                return Float.intBitsToFloat(bytesToInt(data));
            case Types.DOUBLE:
                return (float) getDouble(columnIndex);
            default:
                return Float.parseFloat(getString(columnIndex));
        }
    }
    
    @Override
    public double getDouble(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return 0;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.REAL:
            case Types.FLOAT:
                return getFloat(columnIndex);
            case Types.DOUBLE:
                return Double.longBitsToDouble(bytesToLong(data));
            default:
                return Double.parseDouble(getString(columnIndex));
        }
    }
    
    @Override
    public BigDecimal getBigDecimal(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return null;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.DECIMAL:
            case Types.NUMERIC:
                return decodeDecimal(data, metadata.getScale(columnIndex));
            default:
                return new BigDecimal(getString(columnIndex));
        }
    }
    
    @Override
    public String getString(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return null;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.CHAR:
            case Types.VARCHAR:
            case Types.LONGVARCHAR:
            case Types.NCHAR:
            case Types.NVARCHAR:
            case Types.LONGNVARCHAR:
                return new String(data, getCharset(columnIndex));
            case Types.BINARY:
            case Types.VARBINARY:
            case Types.LONGVARBINARY:
                return bytesToHex(data);
            default:
                return convertToString(columnIndex);
        }
    }
    
    @Override
    public byte[] getBytes(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return null;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.BINARY:
            case Types.VARBINARY:
            case Types.LONGVARBINARY:
            case Types.BLOB:
                return data;
            default:
                return getString(columnIndex).getBytes();
        }
    }
    
    @Override
    public Date getDate(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return null;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.DATE:
                return decodeDate(data);
            case Types.TIMESTAMP:
            case Types.TIMESTAMP_WITH_TIMEZONE:
                return new Date(getTimestamp(columnIndex).getTime());
            default:
                return Date.valueOf(getString(columnIndex));
        }
    }
    
    @Override
    public Time getTime(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return null;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.TIME:
            case Types.TIME_WITH_TIMEZONE:
                return decodeTime(data);
            case Types.TIMESTAMP:
            case Types.TIMESTAMP_WITH_TIMEZONE:
                return new Time(getTimestamp(columnIndex).getTime());
            default:
                return Time.valueOf(getString(columnIndex));
        }
    }
    
    @Override
    public Timestamp getTimestamp(int columnIndex) throws SQLException {
        byte[] data = rowData[columnIndex - 1];
        if (data == null) return null;
        
        switch (metadata.getColumnType(columnIndex)) {
            case Types.TIMESTAMP:
            case Types.TIMESTAMP_WITH_TIMEZONE:
                return decodeTimestamp(data);
            case Types.DATE:
                return new Timestamp(getDate(columnIndex).getTime());
            case Types.TIME:
            case Types.TIME_WITH_TIMEZONE:
                return new Timestamp(getTime(columnIndex).getTime());
            default:
                return Timestamp.valueOf(getString(columnIndex));
        }
    }
    
    // Binary conversion utilities
    private short bytesToShort(byte[] bytes) {
        if (isLittleEndian()) {
            return (short) ((bytes[1] << 8) | (bytes[0] & 0xFF));
        } else {
            return (short) ((bytes[0] << 8) | (bytes[1] & 0xFF));
        }
    }
    
    private int bytesToInt(byte[] bytes) {
        if (isLittleEndian()) {
            return (bytes[3] << 24) | ((bytes[2] & 0xFF) << 16) |
                   ((bytes[1] & 0xFF) << 8) | (bytes[0] & 0xFF);
        } else {
            return (bytes[0] << 24) | ((bytes[1] & 0xFF) << 16) |
                   ((bytes[2] & 0xFF) << 8) | (bytes[3] & 0xFF);
        }
    }
    
    private long bytesToLong(byte[] bytes) {
        if (isLittleEndian()) {
            return ((long) bytes[7] << 56) | ((long) (bytes[6] & 0xFF) << 48) |
                   ((long) (bytes[5] & 0xFF) << 40) | ((long) (bytes[4] & 0xFF) << 32) |
                   ((long) (bytes[3] & 0xFF) << 24) | ((long) (bytes[2] & 0xFF) << 16) |
                   ((long) (bytes[1] & 0xFF) << 8) | (long) (bytes[0] & 0xFF);
        } else {
            return ((long) bytes[0] << 56) | ((long) (bytes[1] & 0xFF) << 48) |
                   ((long) (bytes[2] & 0xFF) << 40) | ((long) (bytes[3] & 0xFF) << 32) |
                   ((long) (bytes[4] & 0xFF) << 24) | ((long) (bytes[5] & 0xFF) << 16) |
                   ((long) (bytes[6] & 0xFF) << 8) | (long) (bytes[7] & 0xFF);
        }
    }
}
```

## PreparedStatement Implementation

```java
public class PreparedStatementImpl implements PreparedStatement {
    
    private String sql;
    private Object[] parameters;
    private int[] parameterTypes;
    private Connection connection;
    
    @Override
    public void setNull(int parameterIndex, int sqlType) throws SQLException {
        parameters[parameterIndex - 1] = null;
        parameterTypes[parameterIndex - 1] = sqlType;
    }
    
    @Override
    public void setBoolean(int parameterIndex, boolean x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.BOOLEAN;
    }
    
    @Override
    public void setByte(int parameterIndex, byte x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.TINYINT;
    }
    
    @Override
    public void setShort(int parameterIndex, short x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.SMALLINT;
    }
    
    @Override
    public void setInt(int parameterIndex, int x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.INTEGER;
    }
    
    @Override
    public void setLong(int parameterIndex, long x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.BIGINT;
    }
    
    @Override
    public void setFloat(int parameterIndex, float x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.REAL;
    }
    
    @Override
    public void setDouble(int parameterIndex, double x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.DOUBLE;
    }
    
    @Override
    public void setBigDecimal(int parameterIndex, BigDecimal x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.DECIMAL;
    }
    
    @Override
    public void setString(int parameterIndex, String x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.VARCHAR;
    }
    
    @Override
    public void setBytes(int parameterIndex, byte[] x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.VARBINARY;
    }
    
    @Override
    public void setDate(int parameterIndex, Date x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.DATE;
    }
    
    @Override
    public void setTime(int parameterIndex, Time x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.TIME;
    }
    
    @Override
    public void setTimestamp(int parameterIndex, Timestamp x) throws SQLException {
        parameters[parameterIndex - 1] = x;
        parameterTypes[parameterIndex - 1] = Types.TIMESTAMP;
    }
    
    @Override
    public void setObject(int parameterIndex, Object x, int targetSqlType) 
            throws SQLException {
        parameters[parameterIndex - 1] = convertToType(x, targetSqlType);
        parameterTypes[parameterIndex - 1] = targetSqlType;
    }
    
    // Wire protocol encoding
    private byte[] encodeParameter(Object value, int sqlType) throws SQLException {
        if (value == null) {
            return null;
        }
        
        switch (sqlType) {
            case Types.BOOLEAN:
            case Types.BIT:
                return new byte[] { ((Boolean) value) ? (byte) 1 : (byte) 0 };
                
            case Types.TINYINT:
                return new byte[] { ((Number) value).byteValue() };
                
            case Types.SMALLINT:
                short s = ((Number) value).shortValue();
                return shortToBytes(s);
                
            case Types.INTEGER:
                int i = ((Number) value).intValue();
                return intToBytes(i);
                
            case Types.BIGINT:
                long l = ((Number) value).longValue();
                return longToBytes(l);
                
            case Types.REAL:
            case Types.FLOAT:
                float f = ((Number) value).floatValue();
                return intToBytes(Float.floatToIntBits(f));
                
            case Types.DOUBLE:
                double d = ((Number) value).doubleValue();
                return longToBytes(Double.doubleToLongBits(d));
                
            case Types.DECIMAL:
            case Types.NUMERIC:
                return encodeDecimal((BigDecimal) value);
                
            case Types.CHAR:
            case Types.VARCHAR:
            case Types.LONGVARCHAR:
                return ((String) value).getBytes(getCharset());
                
            case Types.BINARY:
            case Types.VARBINARY:
            case Types.LONGVARBINARY:
                return (byte[]) value;
                
            case Types.DATE:
                return encodeDate((Date) value);
                
            case Types.TIME:
                return encodeTime((Time) value);
                
            case Types.TIMESTAMP:
                return encodeTimestamp((Timestamp) value);
                
            default:
                throw new SQLException("Unsupported type: " + sqlType);
        }
    }
}
```

## JDBC Metadata

```java
public class DatabaseMetaDataImpl implements DatabaseMetaData {
    
    @Override
    public ResultSet getTypeInfo() throws SQLException {
        // Return information about all supported SQL types
        String[] columnNames = {
            "TYPE_NAME", "DATA_TYPE", "PRECISION", "LITERAL_PREFIX",
            "LITERAL_SUFFIX", "CREATE_PARAMS", "NULLABLE", "CASE_SENSITIVE",
            "SEARCHABLE", "UNSIGNED_ATTRIBUTE", "FIXED_PREC_SCALE",
            "AUTO_INCREMENT", "LOCAL_TYPE_NAME", "MINIMUM_SCALE",
            "MAXIMUM_SCALE", "SQL_DATA_TYPE", "SQL_DATETIME_SUB",
            "NUM_PREC_RADIX"
        };
        
        Object[][] data = {
            // BIGINT
            {"BIGINT", Types.BIGINT, 19, null, null, null,
             typeNullable, false, typePredBasic, false, false,
             false, "BIGINT", 0, 0, Types.BIGINT, 0, 10},
            
            // BINARY
            {"BINARY", Types.BINARY, 255, "'", "'", "length",
             typeNullable, false, typePredBasic, false, false,
             false, "BINARY", 0, 0, Types.BINARY, 0, 0},
            
            // BIT
            {"BIT", Types.BIT, 1, null, null, null,
             typeNullable, false, typePredBasic, false, false,
             false, "BIT", 0, 0, Types.BIT, 0, 0},
            
            // BLOB
            {"BLOB", Types.BLOB, Integer.MAX_VALUE, "'", "'", null,
             typeNullable, false, typePredNone, false, false,
             false, "BLOB", 0, 0, Types.BLOB, 0, 0},
            
            // Additional types...
        };
        
        return new StaticResultSet(columnNames, data);
    }
    
    @Override
    public ResultSet getColumns(String catalog, String schemaPattern,
                                String tableNamePattern, String columnNamePattern)
            throws SQLException {
        // Return column metadata
        String query = 
            "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, COLUMN_NAME, " +
            "DATA_TYPE, TYPE_NAME, COLUMN_SIZE, BUFFER_LENGTH, " +
            "DECIMAL_DIGITS, NUM_PREC_RADIX, NULLABLE, REMARKS, " +
            "COLUMN_DEF, SQL_DATA_TYPE, SQL_DATETIME_SUB, " +
            "CHAR_OCTET_LENGTH, ORDINAL_POSITION, IS_NULLABLE, " +
            "SCOPE_CATALOG, SCOPE_SCHEMA, SCOPE_TABLE, " +
            "SOURCE_DATA_TYPE, IS_AUTOINCREMENT, IS_GENERATEDCOLUMN " +
            "FROM INFORMATION_SCHEMA.COLUMNS " +
            "WHERE TABLE_SCHEMA LIKE ? AND TABLE_NAME LIKE ? " +
            "AND COLUMN_NAME LIKE ? " +
            "ORDER BY TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, ORDINAL_POSITION";
        
        PreparedStatement stmt = connection.prepareStatement(query);
        stmt.setString(1, schemaPattern);
        stmt.setString(2, tableNamePattern);
        stmt.setString(3, columnNamePattern);
        
        return stmt.executeQuery();
    }
}
```

## JDBC Batch Operations

```java
public class BatchExecutor {
    
    private List<String> batchedStatements = new ArrayList<>();
    private PreparedStatement currentStatement;
    private List<Object[]> batchedParameters = new ArrayList<>();
    
    public void addBatch(String sql) throws SQLException {
        batchedStatements.add(sql);
    }
    
    public void addBatch() throws SQLException {
        if (currentStatement != null) {
            Object[] params = new Object[parameterCount];
            System.arraycopy(parameters, 0, params, 0, parameterCount);
            batchedParameters.add(params);
        }
    }
    
    public int[] executeBatch() throws SQLException {
        int[] results = new int[batchedParameters.size()];
        
        // Encode batch for wire protocol
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(baos);
        
        try {
            // Write batch header
            dos.writeInt(batchedParameters.size());
            
            // Write each set of parameters
            for (Object[] params : batchedParameters) {
                for (int i = 0; i < params.length; i++) {
                    if (params[i] == null) {
                        dos.writeInt(-1);  // NULL indicator
                    } else {
                        byte[] encoded = encodeParameter(params[i], parameterTypes[i]);
                        dos.writeInt(encoded.length);
                        dos.write(encoded);
                    }
                }
            }
            
            // Send batch to server
            byte[] batchData = baos.toByteArray();
            sendBatchToServer(batchData);
            
            // Read results
            for (int i = 0; i < results.length; i++) {
                results[i] = readUpdateCount();
            }
            
        } catch (IOException e) {
            throw new SQLException("Batch execution failed", e);
        }
        
        return results;
    }
}
```

---

# ODBC API and Protocol Details

## ODBC Architecture

```c
// ODBC Components
// 1. Application - Uses ODBC API
// 2. Driver Manager - Routes calls to appropriate driver
// 3. Driver - Translates ODBC calls to DBMS-specific calls
// 4. Data Source - The database
```

## ODBC Handles

```c
// Handle types
typedef void* SQLHANDLE;
typedef SQLHANDLE SQLHENV;     // Environment handle
typedef SQLHANDLE SQLHDBC;     // Connection handle
typedef SQLHANDLE SQLHSTMT;    // Statement handle
typedef SQLHANDLE SQLHDESC;    // Descriptor handle

// Handle allocation
SQLRETURN SQLAllocHandle(
    SQLSMALLINT HandleType,
    SQLHANDLE   InputHandle,
    SQLHANDLE*  OutputHandlePtr
);

// Handle types
#define SQL_HANDLE_ENV   1
#define SQL_HANDLE_DBC   2
#define SQL_HANDLE_STMT  3
#define SQL_HANDLE_DESC  4

// Example: Creating handles
SQLHENV henv;
SQLHDBC hdbc;
SQLHSTMT hstmt;

SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
SQLConnect(hdbc, "DSN", SQL_NTS, "user", SQL_NTS, "pass", SQL_NTS);
SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
```

## ODBC Data Types

```c
// SQL data type identifiers
#define SQL_UNKNOWN_TYPE    0
#define SQL_CHAR            1
#define SQL_NUMERIC         2
#define SQL_DECIMAL         3
#define SQL_INTEGER         4
#define SQL_SMALLINT        5
#define SQL_FLOAT           6
#define SQL_REAL            7
#define SQL_DOUBLE          8
#define SQL_DATETIME        9
#define SQL_VARCHAR        12

// Extended SQL data types
#define SQL_TYPE_DATE      91
#define SQL_TYPE_TIME      92
#define SQL_TYPE_TIMESTAMP 93
#define SQL_INTERVAL_MONTH 102
#define SQL_INTERVAL_YEAR  101
#define SQL_INTERVAL_YEAR_TO_MONTH 107
#define SQL_INTERVAL_DAY   103
#define SQL_INTERVAL_HOUR  104
#define SQL_INTERVAL_MINUTE 105
#define SQL_INTERVAL_SECOND 106
#define SQL_INTERVAL_DAY_TO_HOUR 108
#define SQL_INTERVAL_DAY_TO_MINUTE 109
#define SQL_INTERVAL_DAY_TO_SECOND 110
#define SQL_INTERVAL_HOUR_TO_MINUTE 111
#define SQL_INTERVAL_HOUR_TO_SECOND 112
#define SQL_INTERVAL_MINUTE_TO_SECOND 113

// Binary types
#define SQL_BINARY         -2
#define SQL_VARBINARY      -3
#define SQL_LONGVARBINARY  -4
#define SQL_BIGINT         -5
#define SQL_TINYINT        -6
#define SQL_BIT            -7

// Unicode types
#define SQL_WCHAR          -8
#define SQL_WVARCHAR       -9
#define SQL_WLONGVARCHAR  -10

// GUID
#define SQL_GUID          -11

// C data type identifiers
#define SQL_C_CHAR      SQL_CHAR
#define SQL_C_WCHAR     SQL_WCHAR
#define SQL_C_SHORT     SQL_SMALLINT
#define SQL_C_SSHORT    SQL_SMALLINT
#define SQL_C_USHORT    -17
#define SQL_C_LONG      SQL_INTEGER
#define SQL_C_SLONG     SQL_INTEGER
#define SQL_C_ULONG     -18
#define SQL_C_FLOAT     SQL_REAL
#define SQL_C_DOUBLE    SQL_DOUBLE
#define SQL_C_BIT       SQL_BIT
#define SQL_C_TINYINT   SQL_TINYINT
#define SQL_C_STINYINT  SQL_TINYINT
#define SQL_C_UTINYINT  -28
#define SQL_C_SBIGINT   SQL_BIGINT
#define SQL_C_UBIGINT   -27
#define SQL_C_BINARY    SQL_BINARY
#define SQL_C_BOOKMARK  SQL_C_ULONG
#define SQL_C_VARBOOKMARK -23

// Date/Time C types
#define SQL_C_TYPE_DATE      SQL_TYPE_DATE
#define SQL_C_TYPE_TIME      SQL_TYPE_TIME
#define SQL_C_TYPE_TIMESTAMP SQL_TYPE_TIMESTAMP

// Numeric type
#define SQL_C_NUMERIC   SQL_NUMERIC
#define SQL_C_GUID      SQL_GUID
```

## ODBC Data Structures

```c
// Date structure
typedef struct tagDATE_STRUCT {
    SQLSMALLINT  year;
    SQLUSMALLINT month;
    SQLUSMALLINT day;
} DATE_STRUCT, SQL_DATE_STRUCT;

// Time structure
typedef struct tagTIME_STRUCT {
    SQLUSMALLINT hour;
    SQLUSMALLINT minute;
    SQLUSMALLINT second;
} TIME_STRUCT, SQL_TIME_STRUCT;

// Timestamp structure
typedef struct tagTIMESTAMP_STRUCT {
    SQLSMALLINT  year;
    SQLUSMALLINT month;
    SQLUSMALLINT day;
    SQLUSMALLINT hour;
    SQLUSMALLINT minute;
    SQLUSMALLINT second;
    SQLUINTEGER  fraction;  // Nanoseconds
} TIMESTAMP_STRUCT, SQL_TIMESTAMP_STRUCT;

// Numeric structure
#define SQL_MAX_NUMERIC_LEN 16
typedef struct tagSQL_NUMERIC_STRUCT {
    SQLCHAR  precision;
    SQLSCHAR scale;
    SQLCHAR  sign;      // 1 = positive, 0 = negative
    SQLCHAR  val[SQL_MAX_NUMERIC_LEN];
} SQL_NUMERIC_STRUCT;

// Interval structures
typedef enum {
    SQL_IS_YEAR = 1,
    SQL_IS_MONTH = 2,
    SQL_IS_DAY = 3,
    SQL_IS_HOUR = 4,
    SQL_IS_MINUTE = 5,
    SQL_IS_SECOND = 6,
    SQL_IS_YEAR_TO_MONTH = 7,
    SQL_IS_DAY_TO_HOUR = 8,
    SQL_IS_DAY_TO_MINUTE = 9,
    SQL_IS_DAY_TO_SECOND = 10,
    SQL_IS_HOUR_TO_MINUTE = 11,
    SQL_IS_HOUR_TO_SECOND = 12,
    SQL_IS_MINUTE_TO_SECOND = 13
} SQLINTERVAL;

typedef struct tagSQL_YEAR_MONTH_STRUCT {
    SQLUINTEGER year;
    SQLUINTEGER month;
} SQL_YEAR_MONTH_STRUCT;

typedef struct tagSQL_DAY_SECOND_STRUCT {
    SQLUINTEGER day;
    SQLUINTEGER hour;
    SQLUINTEGER minute;
    SQLUINTEGER second;
    SQLUINTEGER fraction;
} SQL_DAY_SECOND_STRUCT;

typedef struct tagSQL_INTERVAL_STRUCT {
    SQLINTERVAL interval_type;
    SQLSMALLINT interval_sign;
    union {
        SQL_YEAR_MONTH_STRUCT year_month;
        SQL_DAY_SECOND_STRUCT day_second;
    } intval;
} SQL_INTERVAL_STRUCT;

// GUID structure
typedef struct tagSQLGUID {
    DWORD Data1;
    WORD  Data2;
    WORD  Data3;
    BYTE  Data4[8];
} SQLGUID;
```

## ODBC Binding

### Column Binding

```c
SQLRETURN SQLBindCol(
    SQLHSTMT     StatementHandle,
    SQLUSMALLINT ColumnNumber,
    SQLSMALLINT  TargetType,
    SQLPOINTER   TargetValuePtr,
    SQLLEN       BufferLength,
    SQLLEN*      StrLen_or_IndPtr
);

// Example: Binding different types
SQLHSTMT hstmt;
SQLRETURN ret;

// Integer binding
SQLINTEGER intValue;
SQLLEN intIndicator;
SQLBindCol(hstmt, 1, SQL_C_SLONG, &intValue, 0, &intIndicator);

// String binding
SQLCHAR stringValue[256];
SQLLEN stringIndicator;
SQLBindCol(hstmt, 2, SQL_C_CHAR, stringValue, sizeof(stringValue), &stringIndicator);

// Binary binding
SQLCHAR binaryValue[1024];
SQLLEN binaryIndicator;
SQLBindCol(hstmt, 3, SQL_C_BINARY, binaryValue, sizeof(binaryValue), &binaryIndicator);

// Timestamp binding
TIMESTAMP_STRUCT tsValue;
SQLLEN tsIndicator;
SQLBindCol(hstmt, 4, SQL_C_TYPE_TIMESTAMP, &tsValue, sizeof(tsValue), &tsIndicator);

// Numeric binding
SQL_NUMERIC_STRUCT numValue;
SQLLEN numIndicator;
SQLBindCol(hstmt, 5, SQL_C_NUMERIC, &numValue, sizeof(numValue), &numIndicator);
```

### Parameter Binding

```c
SQLRETURN SQLBindParameter(
    SQLHSTMT     StatementHandle,
    SQLUSMALLINT ParameterNumber,
    SQLSMALLINT  InputOutputType,
    SQLSMALLINT  ValueType,
    SQLSMALLINT  ParameterType,
    SQLULEN      ColumnSize,
    SQLSMALLINT  DecimalDigits,
    SQLPOINTER   ParameterValuePtr,
    SQLLEN       BufferLength,
    SQLLEN*      StrLen_or_IndPtr
);

// Parameter directions
#define SQL_PARAM_TYPE_UNKNOWN 0
#define SQL_PARAM_INPUT        1
#define SQL_PARAM_INPUT_OUTPUT 2
#define SQL_RETURN_VALUE       4
#define SQL_PARAM_OUTPUT       4
#define SQL_PARAM_INPUT_OUTPUT_STREAM 8
#define SQL_PARAM_OUTPUT_STREAM 16

// Example: Binding parameters
SQLHSTMT hstmt;
SQLCHAR* sql = "INSERT INTO table VALUES (?, ?, ?, ?)";

// Prepare statement
SQLPrepare(hstmt, sql, SQL_NTS);

// Bind integer parameter
SQLINTEGER intParam = 42;
SQLLEN intInd = 0;
SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                0, 0, &intParam, 0, &intInd);

// Bind string parameter
SQLCHAR strParam[50] = "Hello World";
SQLLEN strInd = SQL_NTS;
SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                50, 0, strParam, sizeof(strParam), &strInd);

// Bind binary parameter
SQLCHAR binParam[100];
SQLLEN binInd = 100;
SQLBindParameter(hstmt, 3, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_VARBINARY,
                100, 0, binParam, sizeof(binParam), &binInd);

// Bind NULL parameter
SQLLEN nullInd = SQL_NULL_DATA;
SQLBindParameter(hstmt, 4, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                0, 0, NULL, 0, &nullInd);

// Execute
SQLExecute(hstmt);
```

## ODBC Execution

### Direct Execution

```c
SQLRETURN SQLExecDirect(
    SQLHSTMT   StatementHandle,
    SQLCHAR*   StatementText,
    SQLINTEGER TextLength
);

// Example
SQLHSTMT hstmt;
SQLCHAR* sql = "SELECT * FROM employees WHERE department = 'IT'";
SQLRETURN ret = SQLExecDirect(hstmt, sql, SQL_NTS);

if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
    // Fetch results
    while (SQLFetch(hstmt) == SQL_SUCCESS) {
        // Process row
    }
}
```

### Prepared Execution

```c
SQLRETURN SQLPrepare(
    SQLHSTMT   StatementHandle,
    SQLCHAR*   StatementText,
    SQLINTEGER TextLength
);

SQLRETURN SQLExecute(
    SQLHSTMT StatementHandle
);

// Example
SQLHSTMT hstmt;
SQLCHAR* sql = "UPDATE employees SET salary = ? WHERE id = ?";

// Prepare
SQLPrepare(hstmt, sql, SQL_NTS);

// Bind parameters and execute multiple times
for (int i = 0; i < count; i++) {
    SQLDOUBLE salary = salaries[i];
    SQLINTEGER id = ids[i];
    
    SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                    0, 0, &salary, 0, NULL);
    SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                    0, 0, &id, 0, NULL);
    
    SQLExecute(hstmt);
}
```

## ODBC Data Retrieval

### Fetching Data

```c
SQLRETURN SQLFetch(
    SQLHSTMT StatementHandle
);

SQLRETURN SQLFetchScroll(
    SQLHSTMT    StatementHandle,
    SQLSMALLINT FetchOrientation,
    SQLLEN      FetchOffset
);

// Fetch orientations
#define SQL_FETCH_NEXT     1
#define SQL_FETCH_FIRST    2
#define SQL_FETCH_LAST     3
#define SQL_FETCH_PRIOR    4
#define SQL_FETCH_ABSOLUTE 5
#define SQL_FETCH_RELATIVE 6

// Example: Scrollable cursor
SQLHSTMT hstmt;

// Set cursor type
SQLSetStmtAttr(hstmt, SQL_ATTR_CURSOR_TYPE, 
              (SQLPOINTER)SQL_CURSOR_STATIC, 0);

// Execute query
SQLExecDirect(hstmt, "SELECT * FROM large_table", SQL_NTS);

// Fetch last row
SQLFetchScroll(hstmt, SQL_FETCH_LAST, 0);

// Fetch previous row
SQLFetchScroll(hstmt, SQL_FETCH_PRIOR, 0);

// Fetch absolute position
SQLFetchScroll(hstmt, SQL_FETCH_ABSOLUTE, 100);

// Fetch relative
SQLFetchScroll(hstmt, SQL_FETCH_RELATIVE, -5);
```

### Getting Data

```c
SQLRETURN SQLGetData(
    SQLHSTMT     StatementHandle,
    SQLUSMALLINT Col_or_Param_Num,
    SQLSMALLINT  TargetType,
    SQLPOINTER   TargetValuePtr,
    SQLLEN       BufferLength,
    SQLLEN*      StrLen_or_IndPtr
);

// Example: Getting large data in pieces
SQLHSTMT hstmt;
SQLCHAR buffer[1024];
SQLLEN indicator;
SQLRETURN ret;

// Get BLOB data in chunks
while ((ret = SQLGetData(hstmt, 1, SQL_C_BINARY, buffer, 
                         sizeof(buffer), &indicator)) != SQL_NO_DATA) {
    if (ret == SQL_SUCCESS_WITH_INFO) {
        // More data available
        ProcessChunk(buffer, sizeof(buffer));
    } else if (ret == SQL_SUCCESS) {
        // Last chunk
        ProcessChunk(buffer, indicator);
        break;
    }
}
```

## ODBC Metadata

### Result Set Metadata

```c
SQLRETURN SQLDescribeCol(
    SQLHSTMT     StatementHandle,
    SQLUSMALLINT ColumnNumber,
    SQLCHAR*     ColumnName,
    SQLSMALLINT  BufferLength,
    SQLSMALLINT* NameLengthPtr,
    SQLSMALLINT* DataTypePtr,
    SQLULEN*     ColumnSizePtr,
    SQLSMALLINT* DecimalDigitsPtr,
    SQLSMALLINT* NullablePtr
);

SQLRETURN SQLColAttribute(
    SQLHSTMT     StatementHandle,
    SQLUSMALLINT ColumnNumber,
    SQLUSMALLINT FieldIdentifier,
    SQLPOINTER   CharacterAttributePtr,
    SQLSMALLINT  BufferLength,
    SQLSMALLINT* StringLengthPtr,
    SQLLEN*      NumericAttributePtr
);

// Field identifiers
#define SQL_DESC_AUTO_UNIQUE_VALUE  11
#define SQL_DESC_BASE_COLUMN_NAME   22
#define SQL_DESC_BASE_TABLE_NAME    23
#define SQL_DESC_CASE_SENSITIVE     12
#define SQL_DESC_CATALOG_NAME       17
#define SQL_DESC_CONCISE_TYPE       2
#define SQL_DESC_COUNT              1
#define SQL_DESC_DISPLAY_SIZE       6
#define SQL_DESC_FIXED_PREC_SCALE   9
#define SQL_DESC_LABEL              18
#define SQL_DESC_LENGTH             3
#define SQL_DESC_LITERAL_PREFIX     27
#define SQL_DESC_LITERAL_SUFFIX     28
#define SQL_DESC_LOCAL_TYPE_NAME    29
#define SQL_DESC_NAME               4
#define SQL_DESC_NULLABLE           7
#define SQL_DESC_NUM_PREC_RADIX     32
#define SQL_DESC_OCTET_LENGTH       13
#define SQL_DESC_PRECISION          5
#define SQL_DESC_SCALE              8
#define SQL_DESC_SCHEMA_NAME        16
#define SQL_DESC_SEARCHABLE         14
#define SQL_DESC_TABLE_NAME         15
#define SQL_DESC_TYPE               1
#define SQL_DESC_TYPE_NAME          19
#define SQL_DESC_UNNAMED            20
#define SQL_DESC_UNSIGNED           21
#define SQL_DESC_UPDATABLE          10

// Example: Getting column metadata
SQLHSTMT hstmt;
SQLSMALLINT numCols;

SQLNumResultCols(hstmt, &numCols);

for (SQLSMALLINT i = 1; i <= numCols; i++) {
    SQLCHAR colName[256];
    SQLSMALLINT nameLen;
    SQLSMALLINT dataType;
    SQLULEN colSize;
    SQLSMALLINT decimalDigits;
    SQLSMALLINT nullable;
    
    SQLDescribeCol(hstmt, i, colName, sizeof(colName), &nameLen,
                  &dataType, &colSize, &decimalDigits, &nullable);
    
    printf("Column %d: %s, Type: %d, Size: %lu\n", 
           i, colName, dataType, colSize);
}
```

## ODBC Transactions

```c
// Transaction isolation levels
#define SQL_TXN_READ_UNCOMMITTED 1
#define SQL_TXN_READ_COMMITTED   2
#define SQL_TXN_REPEATABLE_READ  4
#define SQL_TXN_SERIALIZABLE     8

// Set auto-commit
SQLSetConnectAttr(hdbc, SQL_ATTR_AUTOCOMMIT, 
                 (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);

// Set isolation level
SQLSetConnectAttr(hdbc, SQL_ATTR_TXN_ISOLATION,
                 (SQLPOINTER)SQL_TXN_SERIALIZABLE, 0);

// Begin transaction (implicit with autocommit off)

// Execute statements
SQLExecDirect(hstmt, "INSERT INTO table VALUES (...)", SQL_NTS);
SQLExecDirect(hstmt, "UPDATE table SET ...", SQL_NTS);

// Commit or rollback
SQLRETURN SQLEndTran(
    SQLSMALLINT HandleType,
    SQLHANDLE   Handle,
    SQLSMALLINT CompletionType
);

#define SQL_COMMIT   0
#define SQL_ROLLBACK 1

// Commit transaction
SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT);

// Rollback transaction
SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_ROLLBACK);
```

## ODBC Error Handling

```c
SQLRETURN SQLGetDiagRec(
    SQLSMALLINT  HandleType,
    SQLHANDLE    Handle,
    SQLSMALLINT  RecNumber,
    SQLCHAR*     SQLState,
    SQLINTEGER*  NativeErrorPtr,
    SQLCHAR*     MessageText,
    SQLSMALLINT  BufferLength,
    SQLSMALLINT* TextLengthPtr
);

SQLRETURN SQLGetDiagField(
    SQLSMALLINT  HandleType,
    SQLHANDLE    Handle,
    SQLSMALLINT  RecNumber,
    SQLSMALLINT  DiagIdentifier,
    SQLPOINTER   DiagInfoPtr,
    SQLSMALLINT  BufferLength,
    SQLSMALLINT* StringLengthPtr
);

// Diagnostic field identifiers
#define SQL_DIAG_CLASS_ORIGIN       8
#define SQL_DIAG_COLUMN_NUMBER      -1247
#define SQL_DIAG_CONNECTION_NAME    10
#define SQL_DIAG_CURSOR_ROW_COUNT   -1249
#define SQL_DIAG_DYNAMIC_FUNCTION   7
#define SQL_DIAG_DYNAMIC_FUNCTION_CODE 12
#define SQL_DIAG_MESSAGE_TEXT       6
#define SQL_DIAG_NATIVE             5
#define SQL_DIAG_NUMBER             2
#define SQL_DIAG_RETURNCODE         1
#define SQL_DIAG_ROW_COUNT          3
#define SQL_DIAG_ROW_NUMBER         -1248
#define SQL_DIAG_SERVER_NAME        11
#define SQL_DIAG_SQLSTATE           4
#define SQL_DIAG_SUBCLASS_ORIGIN    9

// Example: Error handling
void HandleError(SQLHANDLE handle, SQLSMALLINT type) {
    SQLSMALLINT i = 0;
    SQLINTEGER native;
    SQLCHAR state[7];
    SQLCHAR text[256];
    SQLSMALLINT len;
    SQLRETURN ret;
    
    do {
        ret = SQLGetDiagRec(type, handle, ++i, state, &native,
                           text, sizeof(text), &len);
        if (SQL_SUCCEEDED(ret)) {
            printf("SQLSTATE: %s, Native: %ld, Message: %s\n",
                   state, native, text);
        }
    } while (ret == SQL_SUCCESS);
}

// Usage
SQLRETURN ret = SQLExecDirect(hstmt, sql, SQL_NTS);
if (!SQL_SUCCEEDED(ret)) {
    HandleError(hstmt, SQL_HANDLE_STMT);
}
```

## ODBC Bulk Operations

```c
// Bulk operations
SQLRETURN SQLBulkOperations(
    SQLHSTMT    StatementHandle,
    SQLSMALLINT Operation
);

#define SQL_ADD           4
#define SQL_UPDATE_BY_BOOKMARK 5
#define SQL_DELETE_BY_BOOKMARK 6
#define SQL_FETCH_BY_BOOKMARK  7

// Array binding for bulk insert
SQLHSTMT hstmt;
#define ARRAY_SIZE 100

SQLINTEGER ids[ARRAY_SIZE];
SQLCHAR names[ARRAY_SIZE][50];
SQLLEN idInd[ARRAY_SIZE];
SQLLEN nameInd[ARRAY_SIZE];

// Set array size
SQLSetStmtAttr(hstmt, SQL_ATTR_PARAMSET_SIZE, (SQLPOINTER)ARRAY_SIZE, 0);

// Bind arrays
SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                0, 0, ids, 0, idInd);
SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                50, 0, names, 50, nameInd);

// Fill arrays
for (int i = 0; i < ARRAY_SIZE; i++) {
    ids[i] = i + 1;
    sprintf(names[i], "Name_%d", i);
    idInd[i] = 0;
    nameInd[i] = SQL_NTS;
}

// Execute bulk insert
SQLExecDirect(hstmt, "INSERT INTO table (id, name) VALUES (?, ?)", SQL_NTS);
```

## ODBC Asynchronous Execution

```c
// Enable async execution
SQLSetStmtAttr(hstmt, SQL_ATTR_ASYNC_ENABLE, 
              (SQLPOINTER)SQL_ASYNC_ENABLE_ON, 0);

// Start async execution
SQLRETURN ret = SQLExecDirect(hstmt, sql, SQL_NTS);

// Poll for completion
while (ret == SQL_STILL_EXECUTING) {
    // Do other work...
    
    // Check if complete
    ret = SQLExecDirect(hstmt, sql, SQL_NTS);
}

// Process results
if (SQL_SUCCEEDED(ret)) {
    // Fetch results...
}
```

## ODBC Connection Pooling

```c
// Enable connection pooling
SQLSetEnvAttr(NULL, SQL_ATTR_CONNECTION_POOLING,
             (SQLPOINTER)SQL_CP_ONE_PER_DRIVER, 0);

// Set pool timeout
SQLSetEnvAttr(henv, SQL_ATTR_CP_TIMEOUT, (SQLPOINTER)60, 0);

// Connection will be pooled when closed
SQLDisconnect(hdbc);
SQLFreeHandle(SQL_HANDLE_DBC, hdbc);

// Next allocation may reuse pooled connection
SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
SQLConnect(hdbc, dsn, SQL_NTS, user, SQL_NTS, pass, SQL_NTS);
```