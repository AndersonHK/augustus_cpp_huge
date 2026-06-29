# Building Runtime Ownership Plan

## Direction

Building creation is moving from record-first helpers to runtime-owned objects. Callers should ask the owning runtime to create a `Building` from a `BuildingType`; they should not allocate a legacy `building` record and then ask the runtime to discover it.

The legacy order is:

```cpp
building *record = building_create(type, x, y);
Building &building = building_runtime_impl::get_or_create_instance(record)->building;
```

The target order is:

```cpp
Building &building = city_building_runtime().create(type, x, y);
```

The runtime owns allocation, indexing, module/state binding, and eventual destruction. The legacy save record remains an implementation detail until the save bridge can synthesize it from runtime objects.

## Rules

- Public creation APIs take `const BuildingType &`, not `building_type` ids.
- Lookup and creation are separate verbs. `get_or_create_instance` remains a migration helper for existing record-owned code, not a public creation path.
- `Building::create` should disappear. `Building` is the object being owned, not the owner/factory.
- Creation returns a valid `Building &`. Failure at the ownership boundary is logged and terminated rather than hidden behind null checks.
- Ghosts should eventually use a separate `BuildingRuntime` with the same object shape, so ghost drawing can use `Graphics()`, `main()`, and modules without bespoke preview objects.
- Direct `building_create` callers should be migrated slice by slice into runtime calls, starting with object-oriented call sites and then construction/map import paths.

## Runtime Shape

```cpp
class BuildingRuntime {
public:
    Building &create(const BuildingType &type, int x, int y);
};

BuildingRuntime &city_building_runtime();
```

This first implementation still delegates record allocation to the legacy `building_create` internally. That is deliberately contained behind `BuildingRuntime::create` so callers stop depending on the record-first order.

## Migration Steps

1. Add the public runtime-owned creation API.
2. Remove `Building::create` from the class interface.
3. Migrate the current `Building::create` callers to `city_building_runtime().create(*definition, x, y)`.
4. Convert direct `building_create` call sites by subsystem: normal construction, composed child creation, routed construction, destruction/rubble creation, editor placement, and load-time terrain promotion.
5. Once direct callers are gone, make legacy record allocation private to the runtime or replace it with true runtime-owned record construction.
6. Split city and ghost runtimes so ghost previews are ordinary runtime-owned buildings with a shorter lifetime.
