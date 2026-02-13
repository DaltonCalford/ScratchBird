# JDBC Client Implementation

JDBC connector for Remote Database UDR.

## 1. Scope
- Embedded JVM inside the UDR bundle
- JDBC 4.x compliant drivers
- Drivers bundled inside the UDR (no host install required)

## 2. Architecture
- UDR launches embedded JVM with a fixed classpath
- DriverManager loads the driver class
- Connection, Statement, PreparedStatement, ResultSet are wrapped
  into ScratchBird adapter interfaces

## 3. Connection Properties
- jdbc.url (required)
- user, password
- driverClass (optional if DriverManager auto-loads)
- ssl settings as supported by the driver

## 4. Execution Model
- PreparedStatement for parameter binding
- Statement for ad-hoc SQL
- ResultSet iteration with fetch size
- Scrollable ResultSet optional (TYPE_FORWARD_ONLY recommended)

## 5. Transactions
- Auto-commit off for explicit control
- Connection.commit/rollback

## 6. Error Mapping
- Map SQLException SQLState and vendorCode to ScratchBird errors

## 7. Type Mapping
- Use ResultSetMetaData and JDBC type codes
- Map standard JDBC types to ScratchBird types

## 8. Required Capabilities
- prepared statements
- parameter binding
- paging via fetch size
- cancellation via Statement.cancel
- schema introspection via DatabaseMetaData

## 9. References
- https://docs.oracle.com/javase/8/docs/api/java/sql/package-summary.html

