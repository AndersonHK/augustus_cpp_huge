# Building Object Migration Agent Logs

Each worker owns one log file named after its nickname. Do not edit another
worker's log.

Record:
- owned file scope
- files changed
- public signatures changed
- new `Building` methods added or relied on
- remaining `legacy_record()` use and why the object API does not cover it yet
- build or focused compile command and result
- blockers or conflicts with other worker scopes

Central building API changes should be reusable, documented in the worker log,
and implemented in `src/building/building.h` plus `src/building/building.cpp`.
