# API Reference (C++)

## Wrapper Types
- JSONB: `scratchbird::Jsonb`
- RANGE: `scratchbird::Range<T>`
- GEOMETRY: `scratchbird::Geometry`


## C++ classes
```cpp
namespace scratchbird {
struct ConnOptions;

class Connection {
public:
  static Connection connect(const std::string& uri);
  static Connection connect(const ConnOptions& options);
  void close();
  Transaction begin();
  void commit();
  void rollback();
  Result execute(const std::string& sql, const Params& params = {});
  Result query(const std::string& sql, const Params& params = {});
  PreparedStatement prepare(const std::string& sql);
  void set_option(const std::string& key, const std::string& value);
};

class PreparedStatement {
public:
  void bind(size_t index, const Value& value);
  void bind(const std::string& name, const Value& value);
  Result execute();
};

class Result {
public:
  bool next();
  Value get(size_t column) const;
  Value get(const std::string& name) const;
  size_t column_count() const;
  ColumnMeta column_meta(size_t column) const;
};

class Transaction {
public:
  void commit();
  void rollback();
};
}
```

## C API wrapper
- sb_connect / sb_disconnect
- sb_prepare / sb_bind_index / sb_bind_name
- sb_execute / sb_query
- sb_fetch / sb_get_column_meta / sb_value_get
- sb_tx_begin / sb_tx_commit / sb_tx_rollback

## Usage examples
```cpp
auto conn = scratchbird::Connection::connect("scratchbird://user:pass@localhost:3092/db");
auto result = conn.query("select 1 as one");

auto stmt = conn.prepare("select * from widgets where id = $1");
stmt.bind(1, 42);
auto rows = stmt.execute();
```
