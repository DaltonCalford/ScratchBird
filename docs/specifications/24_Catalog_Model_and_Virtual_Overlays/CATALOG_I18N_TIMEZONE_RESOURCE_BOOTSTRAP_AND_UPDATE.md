# Catalog I18N, Timezone, Resource Bootstrap, and Update

Status: current_authority

## Proven scope

Current source proves:
- charset loading through CharsetLoader
- collation and alias loading through the same loader family
- timezone loading through TimezoneLoader
- catalog-backed lookup paths for current charset and timezone surfaces

## Boundary

This file does not claim a universal staged activation lifecycle for all resource bundles or artifacts. resource_bundle and related families may be described only as persisted families unless and until a fuller activation and update pipeline is source-proven.
