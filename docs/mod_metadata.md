# Mod Metadata

Every mod must provide a `mod.xml` file at the root of its directory. This file is the authoritative declaration of the mod's identity, description, version, and prerequisites.

## Schema

```xml
<?xml version="1.0" encoding="utf-8"?>
<mod>
    <name value="Elven Tavern" />
    <description value="Replaces the Roman tavern with an elven tavern." />
    <version value="1.0.0" />
    <dependencies>
        <mod name="Vespasian" />
    </dependencies>
</mod>
```

The `name`, `description`, `version`, and `dependencies` elements are required exactly once. Values must be nonempty. A mod with no prerequisites uses `<dependencies />`.

The name must exactly match both the mod directory name and its entry in `mod-list`. Names cannot be `.`, `..`, contain control characters, or contain characters Windows forbids in file names (`/`, `\\`, `:`, `<`, `>`, `"`, `|`, `?`, or `*`). Dependency names are compared case-insensitively for duplicate and lookup checks, but authors should use the dependency's canonical spelling.

`version` is an opaque, required identifier in this first schema. Dependency version ranges and version ordering are not defined yet.

## Loading

`mod-list` remains the user-controlled activation order. For a selected mod, the loader activates that entry and every preceding entry. Each active mod's `mod.xml` must exist and parse successfully, and every declared dependency must already appear earlier in the active stack. Missing, duplicate, self, or forward dependencies fail startup with the relevant manifest or dependency in the error.

Dependencies are transitive. For example, Vespasian declares Augustus, while Augustus declares Julius; Vespasian does not need to repeat Julius. Likewise, a fourth mod that declares only Vespasian inherits the complete Julius, Augustus, and Vespasian stack when those dependencies precede it in `mod-list`.

## Definition Inheritance

The active stack is evaluated from the lowest dependency to the selected mod. Definition directories are optional in every layer: a mod may contain only `mod.xml` and the definitions it adds, replaces, or suppresses.

Registry-backed folders such as `BuildingType`, `FigureType`, `Foundations`, `Resources`, `StorageType`, `ProductionMethod`, `UnitType`, and the other definition registries merge by stable string identity. A definition absent from an upper mod is inherited unchanged. A definition present in an upper mod replaces the lower winner, while an identity-only `disabled="true"` tombstone suppresses it where that registry supports tombstones.

Whole-file resources such as declarative UI documents use the nearest file in the stack: the selected mod's file wins when present, otherwise the loader walks down through its dependencies.

Do not copy unchanged definitions into a dependent mod. Keeping only changed files makes provenance clear and permits a fourth mod that depends on Vespasian to add or replace a handful of definitions without reproducing the Julius, Augustus, or Vespasian trees.

## Future Migration Ownership

The next metadata extension will let each mod own its compatibility and migration declarations. Its element and file names are intentionally not fixed yet.

The initial bridge categories will cover buildings, figures, and terrain. A declaration will identify a source load key, which may be either a legacy numeric ID or a string ID, and resolve it to zero or one target string ID in the active object model. Multiple source keys may converge on the same target, but one source key must never fan out to multiple targets.

This supports direct replacements such as `tavern` to `elven_tavern`, lossy imports such as several Augustus overgrown-garden variants to a normal `garden`, and explicit removal where a source has no safe target. The same ownership model can later support partial city transfer among Caesar 3, Pharaoh, Poseidon, and Emperor-style mods.

Migration is a startup/save-load boundary. Runtime objects continue to use resolved string-owned definitions and must not branch on legacy numeric IDs.
