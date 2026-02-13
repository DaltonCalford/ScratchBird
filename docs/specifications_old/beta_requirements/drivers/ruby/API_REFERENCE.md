# API Reference (Ruby)

## Wrapper Types
- JSONB: `Scratchbird::JSONB`
- RANGE: `Scratchbird::RangeValue`
- GEOMETRY: `Scratchbird::Geometry`


```ruby
module Scratchbird
  def self.connect(uri_or_opts) -> Connection; end
end

class Connection
  def exec(sql, params = nil) -> Result; end
  def query(sql, params = nil) -> Result; end
  def prepare(sql) -> Statement; end
  def begin; end
  def commit; end
  def rollback; end
  def transaction(&block); end
  def close; end
end

class Statement
  def bind_param(index_or_name, value); end
  def execute(params = nil) -> Result; end
  def fetch; end
  def fetch_all; end
  def finish; end
end

class Result
  def each(&block); end
  def fields; end
  def row_count; end
end
```

## Usage examples
```ruby
conn = Scratchbird.connect("scratchbird://user:pass@localhost:3092/db")
res = conn.query("select 1 as one")

stmt = conn.prepare("select * from widgets where id = $1")
stmt.bind_param(1, 42)
rows = stmt.execute
```
