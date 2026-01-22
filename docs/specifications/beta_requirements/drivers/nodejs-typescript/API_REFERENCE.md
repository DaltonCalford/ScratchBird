# API Reference (Node.js / TypeScript)

## Wrapper Types
- JSONB: `ScratchbirdJsonb`
- RANGE: `ScratchbirdRange<T>`
- GEOMETRY: `ScratchbirdGeometry`


```ts
export interface ClientConfig {
  host?: string;
  port?: number;
  user?: string;
  password?: string;
  database?: string;
  ssl?: boolean | object;
  connectTimeoutMs?: number;
  applicationName?: string;
  binaryTransfer?: boolean;
  compression?: "zstd" | "off";
}

export interface FieldDef {
  name: string;
  dataType: string;
  format: "text" | "binary";
  nullable: boolean;
}

export interface QueryResult<T = any> {
  rows: T[];
  rowCount: number;
  fields: FieldDef[];
  command: string;
}

export class Client {
  constructor(config?: ClientConfig | string);
  connect(): Promise<void>;
  query<T = any>(text: string, params?: any[] | Record<string, any>): Promise<QueryResult<T>>;
  prepare(name: string, text: string, paramTypes?: string[]): Promise<void>;
  execute<T = any>(name: string, params?: any[] | Record<string, any>): Promise<QueryResult<T>>;
  begin(): Promise<void>;
  commit(): Promise<void>;
  rollback(): Promise<void>;
  end(): Promise<void>;
}

export class Pool {
  constructor(config?: ClientConfig | string);
  connect(): Promise<Client>;
  query<T = any>(text: string, params?: any[] | Record<string, any>): Promise<QueryResult<T>>;
  end(): Promise<void>;
}
```

## Usage examples
```ts
const client = new Client({
  host: "localhost",
  port: 3092,
  user: "user",
  password: "pass",
  database: "db",
});
await client.connect();

const res = await client.query("select 1 as one");
await client.prepare("by_id", "select * from widgets where id = $1");
const rows = await client.execute("by_id", [42]);
await client.end();
```
