# Connection Problems

**Last Updated:** 2026-02-03

---

## Checklist

1. Confirm server is running.
2. Confirm listener port is enabled.
3. Validate credentials.
4. Verify HBA rules.
5. Check firewall and routing.

## Quick Diagnostics

```bash
ss -tlnp | grep -E '3092|5432'
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
