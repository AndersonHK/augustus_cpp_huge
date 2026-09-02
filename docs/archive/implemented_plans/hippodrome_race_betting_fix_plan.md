# Historical Hippodrome Race And Betting Fix

Status: superseded by the data-owned race module and maintained runtime contract in `docs/hippodrome_racing_and_betting.md`.

This slice originally joined the Augustus betting result to the visible race, replaced its unrelated random winner roll with actual finish order, and repaired the load-time relationship corruption exposed by `Praetor 2 9.svv`.

The crash was caused by one-pass figure loading: forward relationships were attached before later figure slots were reset. Resetting those slots disconnected endpoints without reconciling already-loaded combat counters, leaving a prefect with attackers recorded but no valid opponent relationship. Loading now stages relationship IDs until all figure records are stable, validates the reconstructed graph, repairs recoverable combat tuples with a warning, and emits a clean current-version round trip.

The first race implementation temporarily assumed four teams and inherited Julius combinations. It was subsequently replaced by the current BuildingType module, per-venue dynamic sessions, two-team Julius/Augustus definitions, four-team Vespasian data, a declarative betting window, and Vespasian-owned graphics. This archived note is historical evidence, not the current schema or implementation guide.
