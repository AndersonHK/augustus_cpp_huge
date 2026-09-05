# Runtime Refactor Working Memory

Snapshot: 2026-06-27 object-owned runtime doctrine update
Workspace: `<repository checkout>`

## Public Data Versus Methods

- Do not add one-line accessors just to hide fields. If a member read is only `return value;`, or a member write is only `value = other_value;`, that state should normally be public object data and callers should use it directly.
- Keep state private only when access must enforce real behavior: clamping, normalization, lifecycle registration, deregistration, route/cache invalidation, dirty marking, id-to-pointer bridge resolution, reservation release, save/load migration, or multi-field invariants.
- Methods should be named for behavior and ownership, not for field plumbing. Prefer `figure.release_destination_reservations()` or `building.register_storage_policy_change()` over `set_*()` wrappers that only assign an integer.
- During record-to-object migrations, a temporary `private` block can be useful to expose compile fallout, but it is not the desired final shape. The follow-up pass should move assignment-only state to public data and reserve private members for invariant-bearing state.
- This rule is especially important while migrating `building`/`Building`: copying legacy record fields into a class and then rebuilding one accessor per old field recreates the old C compatibility layer in object syntax. The point is to make the runtime object own data and behavior directly.
- Identity is special. `Building.id` and similar ids are stable bridge keys, not ordinary mutable state. Keep them read-only or directly backed by the current runtime/save bridge state until save/load serializes live objects and modules directly.
- Peeled fields should eventually leave the runtime record and live in the module that owns them. The save bridge should reconstruct the save record from the runtime struct plus module state. If a required module slice is missing, finish writing the safest partial/default save payload first, then report the error with enough object/type/module context to diagnose the missing state.

## Owner-Bound Module Shape

- Prefer external APIs shaped like `building.production().tick()` or `building.culture().should_animate()`.
- Avoid loose calls shaped like `definition->tick(building)` in normal runtime code. Internally, a bound module can delegate to its immutable definition, but callers should enter through the owning object or owner-bound module.
- A bound module packages `Building &owner_`, a mutable state reference, and a pointer to immutable type/module definition data. The definition pointer can change when the building changes type; the definition object itself should remain startup-owned and immutable.
- See `docs/bound_runtime_module_extraction_plan.md` for the first planned module facades and XML folder mapping.

## Practical Review Check

When touching a new object method, ask:

1. Does this method do anything beyond returning or assigning one field?
2. If yes, is the behavior attached to the object that owns the invariant?
3. If no, can the field be public and the method deleted?

If the honest answer is "this is just a getter/setter for an int", remove the wrapper during the cleanup slice instead of spreading it to more callers.
