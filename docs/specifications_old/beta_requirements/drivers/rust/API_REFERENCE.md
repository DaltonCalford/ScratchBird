# API Reference (Rust)

## Wrapper Types
- JSONB: `scratchbird::types::Jsonb`
- RANGE: `scratchbird::types::Range<T>`
- GEOMETRY: `scratchbird::types::Geometry`


## Async client
```rust
pub struct Client;
impl Client {
  pub async fn connect(opts: Config) -> Result<Self, Error>;
  pub async fn query(&self, sql: &str, params: Params) -> Result<Vec<Row>, Error>;
  pub async fn execute(&self, sql: &str, params: Params) -> Result<CommandResult, Error>;
  pub async fn prepare(&self, sql: &str) -> Result<Statement, Error>;
  pub async fn begin(&self) -> Result<Transaction, Error>;
  pub async fn close(self) -> Result<(), Error>;
}

pub struct Statement;
impl Statement {
  pub async fn bind(&mut self, index: usize, value: Value) -> Result<(), Error>;
  pub async fn bind_named(&mut self, name: &str, value: Value) -> Result<(), Error>;
  pub async fn execute(&self) -> Result<CommandResult, Error>;
  pub async fn query(&self) -> Result<Vec<Row>, Error>;
}

pub struct Transaction;
impl Transaction {
  pub async fn commit(self) -> Result<(), Error>;
  pub async fn rollback(self) -> Result<(), Error>;
}

pub struct Row;
impl Row {
  pub fn get<T: FromValue>(&self, index: usize) -> Result<T, Error>;
  pub fn get_named<T: FromValue>(&self, name: &str) -> Result<T, Error>;
}
```

## Blocking wrapper
```rust
pub mod blocking {
  pub struct Client;
  impl Client {
    pub fn connect(opts: Config) -> Result<Self, Error>;
    pub fn query(&self, sql: &str, params: Params) -> Result<Vec<Row>, Error>;
    pub fn execute(&self, sql: &str, params: Params) -> Result<CommandResult, Error>;
    pub fn prepare(&self, sql: &str) -> Result<Statement, Error>;
    pub fn begin(&self) -> Result<Transaction, Error>;
    pub fn close(self) -> Result<(), Error>;
  }
}
```

## Usage examples
```rust
let client = Client::connect(config).await?;
let rows = client.query("select 1 as one", Params::empty()).await?;

let mut stmt = client.prepare("select * from widgets where id = $1").await?;
stmt.bind(1, Value::from(42)).await?;
let rows = stmt.query().await?;
```
