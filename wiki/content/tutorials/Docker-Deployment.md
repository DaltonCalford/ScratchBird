# Docker Deployment

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

A minimal deployment flow for Docker-based environments.

## Steps

1) Pull the image.
2) Run with volumes for data and config.
3) Expose port `3092` for native clients.

```bash
docker pull scratchbird/scratchbird:latest

docker run -d \
  --name scratchbird \
  -p 3092:3092 \
  -v scratchbird_data:/var/lib/scratchbird/data \
  scratchbird/scratchbird:latest
```

## References

- [Docker Install](../installation/Docker.md)
- [Server CLI](../cli-tools/sb-server.md)
- [Configuration](../configuration/sb_server.conf.md)
