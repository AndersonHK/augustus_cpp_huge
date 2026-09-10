# Barracks production and recruitment

The barracks produces `troops` through the ordinary production runtime. One cartload (100 resource units) represents one trained recruit. The Julius and Augustus output buffer holds 100 units; Vespasian overrides the same storage definition to hold 400. The nominal production rate is 200 units per month, or two recruits at full staffing before modifiers. Production work, calendar normalization, and partial staffing follow the same rules as other producers.

The building declares a `barracks_recruit` spawn policy. A successful dispatch converts one stored troop load into a soldier or tower sentry, respecting the existing priority controls. Soldiers retain the existing fort/academy travel and formation reservation behavior. If no destination is eligible, the load stays in storage. Ordinary goods carts cannot reserve non-tradeable output, and trade availability does not hide producers of non-tradeable resources.

## Equipment belongs to the soldier type

Recruitment costs are declared in `UnitType`, in resource units:

```xml
<recruit type="legionary">
    <requirement resource="weapons" amount="100" />
</recruit>
```

Multiple requirements are supported. All must be accessible in the recruiter's declared input storage before a soldier can spawn. Successful creation consumes the complete equipment list; failure consumes nothing. Recruitment does not identify weapons in code. Storage definitions determine which additional resources a barracks can receive. The former `requires_weapon` Boolean is replaced by this list.

`weapons_input_recruitment` holds four loads and declares `respect_orders="true"`. Warehouse and armoury suppliers deliver through the same native input reservations used by workshops. Acceptance orders are checked by storage availability, so alternate supplier selection cannot bypass a stopped delivery order. New buildings accept resources declared by such ordered storage; saved orders remain intact.

## Production modifiers

The barracks production method retains food stress as a declared work modifier:

```xml
<work_modifier source="military_food_stress" threshold="20" percent_per_point="12.5" />
```

Each stress point above 20 adds 12.5% to required work: stress 28 doubles training work, and stress 36 triples it. This carries the old full-staffing relationship of `8 + max(0, stress - 20)` into normal production. Staffing now scales continuously through worker progress. Methods without this declaration do not receive the penalty. The modifier interface resolves its named source in production code, alongside the existing climate modifiers; it is not embedded in barracks dispatch.

## Mars Grand Temple and saved games

The Mars Grand Temple keeps its existing `grand_temple_mars_recruit` policy, staffing delays, food-stress penalty, priest behavior, and recruitment priorities. It shares the new equipment requirements and native input delivery, but does not require a barracks troop buffer. Moving that religious policy into the god/religion subsystem is deferred.

Save version 213 converts pre-213 barracks and Mars Grand Temple weapon counts from cartloads to resource units after keyed resources load. Four old loads become 400 units. Old barracks industrial progress is discarded because the former delay modifier did not produce stock. Troop buffers use keyed resource persistence, including special resources. Existing troop-production overrides serialize under `barracks_recruits`; legacy `recruitment_delay` overrides convert from delay percentages to inverse throughput and emit a migration warning, then re-save canonically. Foreign Augustus recruitment-delay values are converted at import. Conversion rounds to integer monthly throughput; zero legacy delay maps to the fastest representable production rate.

Native tests exercise buffer limits, resource-based eligibility, empty-buffer dispatch, food stress, acceptance orders, exclusion from goods carts, keyed stock persistence, actual soldier creation, and the Mars Grand Temple's original spawn policy. The original saves are reloaded after test mutations before normal save soaks.

See the [validation and deployment record](../research/barracks_recruitment_validation_2026_09_10.md) for results and the outstanding installed-build performance discrepancy.
