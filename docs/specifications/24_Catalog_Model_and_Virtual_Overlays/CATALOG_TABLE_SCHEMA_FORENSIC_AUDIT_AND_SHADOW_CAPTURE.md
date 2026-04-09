# Catalog: Forensic Audit, Replay, and Shadow Capture Tables

## Added Tables
### writeback_incident
- writeback_incident_uuid PK
- filespace_uuid nullable
- failure_class
- first_seen_time
- last_seen_time
- retry_count
- degraded_state
- clearance_condition_uuid nullable
- is_open
- is_valid

### recovery_incident
- recovery_incident_uuid PK
- recovery_generation
- classification
- checkpoint_generation nullable
- object_uuid nullable
- details_uuid nullable
- created_time
- is_valid
