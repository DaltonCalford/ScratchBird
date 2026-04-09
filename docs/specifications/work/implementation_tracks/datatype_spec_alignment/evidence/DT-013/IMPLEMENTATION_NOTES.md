# Implementation Notes

- Added explicit `allow_system_reserved_name` switch in `DomainCreateOptions`.
- Added reserved-name validation shared by all domain create entry points.
- Added rename guard so user namespaces cannot be silently converted into system namespace.
