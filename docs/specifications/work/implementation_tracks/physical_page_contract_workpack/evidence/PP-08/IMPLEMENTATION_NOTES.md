# Implementation Notes

- Added strict bootstrap-page-map validation during `Database::open()`.
- Added canonical header checks for bootstrap references and minimum fixed-page count.
- Updated corruption test to target canonical FSM root page id.
