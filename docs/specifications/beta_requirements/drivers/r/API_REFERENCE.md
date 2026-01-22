# API Reference (R DBI)

## Wrapper Types
- JSONB: `sb_jsonb`
- RANGE: `sb_range`
- GEOMETRY: `sb_geometry`


## Usage examples
```r
con <- dbConnect(Scratchbird(), host=..., port=..., user=..., password=..., dbname=...)
dbDisconnect(con)

dbExecute(con, statement, params = list())
dbGetQuery(con, statement, params = list())
res <- dbSendQuery(con, statement, params = list())
dbFetch(res, n = -1)
dbClearResult(res)

dbBegin(con)
dbCommit(con)
dbRollback(con)

dbColumnInfo(res)
dbGetInfo(con)
dbListTables(con)        # optional
dbListFields(con, name)  # optional
```
