# Docker

**Last Updated:** 2026-01-28

## Quick Start

```bash
docker pull scratchbird/scratchbird:latest

docker run -d   --name scratchbird   -p 3092:3092   -p 5432:5432   -p 3306:3306   -p 3050:3050   -e SCRATCHBIRD_PASSWORD=mypassword   -v scratchbird_data:/var/lib/scratchbird/data   scratchbird/scratchbird:latest
```

Connect:
```bash
sb_isql -H localhost -p 3092 -U admin -d mydb
psql -h localhost -p 5432 -U admin -d mydb
```

See `docs/user-documentation/installation/` for platform-specific details.
