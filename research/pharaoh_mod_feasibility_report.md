# Pharaoh Mod Feasibility Report

**Research snapshot:** 2026-07-12  
**Local engine snapshot:** `844b35b68d7d37d142d5e16027c179af278abc7b`  
**Akhenaten snapshot inspected:** `fe8df9ef9d71aa16389ada2d51213bd6c84ecb36`  
**Scope:** early exploration only; no extractor, runtime, or mod XML was implemented.

## Executive conclusion

A new `Pharaoh` mod is feasible and is a good architectural test of whether this project is becoming an Impressions city-builder engine rather than a Caesar III engine with data files. It should be started as a long-lived vertical conversion, not presented as a short content mod.

There are three very different levels of difficulty:

| Target | Difficulty | Rough single-contributor effort | What it means |
| --- | --- | ---: | --- |
| A loadable Pharaoh-labelled stack using Julius behavior | Low to medium | Days to 3 weeks | The game starts with `Julius, Pharaoh`; most content is still Julius, with a few Pharaoh graphics and names. |
| A recognizable Pharaoh sandbox | High | 3-7 months | Pharaoh art, housing ladder, resources, core production, bazaars, Nile farming, and a representative set of buildings work. Existing XML-backed gods, resources, terrain work, and phased monuments reduce the amount of new infrastructure needed. |
| A faithful Pharaoh/Cleopatra engine | Very high | 1-3+ contributor-years | Campaigns, saves, monuments, flood cycles, religion, combat, events, UI, audio, and edge-case behavior approach the original game. |

The first milestone is realistic now. The second is a substantial engine program. The third is effectively another game implementation, as the Ozymandias/Akhenaten history already demonstrates.

The recommended start is:

1. finish the XML layering capability that the mod architecture was intended to have;
2. add `Mods/Pharaoh` as a sparse layer over `Mods/Julius`;
3. build a standalone Pharaoh graphics extractor using the proven `.sg3`/`.555` format knowledge in Akhenaten;
4. prove the pipeline with one complete vertical slice: crude hut, well, road, firehouse, bazaar, grain farm, granary, and their walkers/resources;
5. expand outward while recording every Caesar-only runtime assumption exposed by the slice.

That sequence keeps the project bootable and makes every Pharaoh addition increase the engine's generality.

## The fork that already attempted this

The original relevant fork is [Banderi/Ozymandias_Julius](https://github.com/Banderi/Ozymandias_Julius), itself forked from Augustus. Its stated purpose was to make the Julius/Augustus engine work with Pharaoh rather than Caesar III. It remained unfinished but could load original Pharaoh saves and play early campaign missions. Its README now points to [dalerank/Akhenaten](https://github.com/dalerank/Akhenaten) as the continued fork.

Akhenaten is active and much further along. Its current README says it:

- is a Julius/Augustus fork aimed at Pharaoh;
- requires an original Pharaoh plus Cleopatra installation;
- loads original Pharaoh saves and early campaign missions;
- uses original graphics, sounds, and other assets rather than redistributing them;
- increasingly expresses gameplay in embedded JavaScript and packaged mod data.

This is important precedent, but Akhenaten should not simply become the new base of this repository. Its architecture has diverged substantially: the inspected snapshot contains about 1,426 C/C++ source/header files and 310 JavaScript files, with Pharaoh behavior split between both. The value of Akhenaten here is as:

- a primary technical reference for Pharaoh file formats and asset-pack indices;
- a behavioral and data reference for house requirements, resources, buildings, and mechanics;
- evidence of the true size of the conversion;
- a possible source of AGPL-compatible implementations, subject to deliberate attribution and integration review.

The local project should retain its XML-first direction and translate Pharaoh concepts into its own registries instead of importing Akhenaten's script architecture wholesale.

## What the local mod XML already provides

`Mods/Julius` is already a strong bootstrap vocabulary. At this snapshot it contains approximately:

| Category | Julius XML files | Relevance to Pharaoh |
| --- | ---: | --- |
| `BuildingType` | 125 | identity, cost, desirability, foundation, graphics, housing link, production/storage links, spawns, menu placement |
| `BuildingTypeMenu` | 11 | nested construction menu groups |
| `HousingProfile` | 20 | evolution requirements, residents, prosperity, tax, services and goods |
| `Resources` | 19 | named/numbered resource slots, trade values, cart/storage/UI graphics |
| `ProductionMethod` | 17 | input/output production behavior |
| `StorageType` | 21 | input/output storage roles and capacities |
| `FigureType` | 23 | walker profiles, ownership, movement, pathing, and graphics |
| `Foundations` | 29 | placement and terrain policy |
| `Distribution` | 7 | what a distributor may collect |
| Other registries | 96 | gods, religions, culture, formations, units, tiles, water access, and UI |

The current XML decomposition is conceptually well suited to Pharaoh. A Pharaoh grain farm should not need one monolithic definition: the building, field, grain resource, output storage, production method, cart/walker, and menu button can remain separate definitions. The same applies to housing: `BuildingType` can own footprint, art, capacity, and transitions, while `HousingProfile` owns residents and evolution requirements.

The conversion also starts further ahead than a directory count suggests:

- resources are already XML definitions with authored identity, flags, trade values, and graphics;
- gods and religions already have XML registries, including XML-authored blessings, tiers, capacity, and presentation;
- terrain/buildable-tile definitions are already moving through the XML pipeline;
- Augustus monuments have already supplied XML-backed monument identity and graphics;
- phased monument construction is already a generic `<construction mode="phased">` contract with per-phase graphics, architects, and material requirements, as demonstrated by the Augustus lighthouse;
- foundations, water-access types, production, storage, distribution, culture modules, figures, formations, and units already have data seams that Pharaoh can extend.

This changes the expected work from “invent most Pharaoh-capable systems” to “exercise and generalize existing systems, then add the genuinely absent Pharaoh semantics.” Pharaoh pyramids and tombs will still stress scale, footprints, delivery, labor, ramps, and completion behavior, but the project is not starting monument construction from zero.

There is also room for genuinely new building identifiers. The local engine reserves dynamic building types from 212 through 511, so the XML parser is not limited to the Caesar III enum for every new structure. That is enough numerical space for a large first Pharaoh catalogue. It does not mean every new building can have novel behavior without code; dynamic identity and generic XML behavior are separate questions.

## The deferred XML layering requirement

The mod manager already reads an ordered `mod-list`. Its default stack is:

```xml
<mod_list>
  <mod name="Julius" />
  <mod name="Augustus" />
  <mod name="Vespasian" />
</mod_list>
```

A provisional Pharaoh stack can therefore be expressed as:

```xml
<mod_list>
  <mod name="Julius" />
  <mod name="Pharaoh" />
</mod_list>
```

However, this does **not** currently make all XML a sparse overlay. Layering was intended by the architecture and deferred while the three existing data sets did not require sparse inheritance. A Pharaoh layer makes the deferred requirement immediately visible and overdue.

Graphics are layered: an authored group is sought in the top mod and then in lower recognized sources. This is already close to what Pharaoh needs. Most gameplay registries instead build their directory from `mod_manager::mod_path()`, which is the final mod in the list. This includes at least:

- `BuildingType` and `BuildingTypeMenu`;
- `Resources`;
- `HousingProfile`;
- `ProductionMethod`;
- `StorageType`;
- `Distribution`;
- `CultureModule`, gods, religions, foundations, formations, and units.

`FigureType` has some explicit fallback behavior, but that exception does not make the whole data model layered.

Consequently, an empty or sparse `Mods/Pharaoh/BuildingType` cannot currently inherit all missing Julius buildings. The immediate alternatives are:

### Option A: generated Julius shadow copy

Copy the complete Julius data tree into `Mods/Pharaoh`, then replace files gradually.

This is the fastest bootable proof, probably measured in days. It also creates permanent duplication, makes upstream Julius corrections harder to inherit, and obscures which files actually define Pharaoh identity. It is acceptable only as a throwaway bootstrap or generated build artifact.

### Option B: genuine registry overlays

Teach every registry to resolve definitions across the ordered mod paths, lower layer first and top layer last, with deterministic replacement by stable ID/path. A missing top-layer definition then naturally inherits Julius; a Pharaoh file with the same logical ID replaces it.

The basic implementation should be small: the ordered paths already exist, graphics already establish the precedence direction, and the registries share similar directory-loading patterns. A focused implementation is plausibly a few days; allowing up to 1-2 weeks provides room for consistent duplicate handling, cross-definition resolution, diagnostics, tests, and any registry-specific edge cases. It is the recommended route. The work completes an intended capability, benefits every future total conversion, and makes the declared mod stack truthful.

Deletion or suppression also needs a contract. Eventually Pharaoh must be able to hide inherited Roman buildings without creating dummy replacements. A small explicit mechanism such as a definition-level `disabled="true"` or a separate removal manifest is preferable to magic empty files.

## Pharaoh graphics extraction

### Why this is tractable

The extractor does not need fresh reverse engineering. Akhenaten's inspected implementation already reads Pharaoh image packs from paired files:

- `.sg3` contains the pack header, group starts, bitmap names, entry metadata, dimensions, animation metadata, offsets, and flags;
- `.555` contains the 16-bit pixel payload, including compressed data and isometric footprint data.

Akhenaten's [image pack loader](https://github.com/dalerank/Akhenaten/blob/fe8df9ef9d71aa16389ada2d51213bd6c84ecb36/src/content/imagepak.cpp) demonstrates the decoding path, while its [image-pack catalogue](https://github.com/dalerank/Akhenaten/blob/fe8df9ef9d71aa16389ada2d51213bd6c84ecb36/src/scripts/imagepaks.js) enumerates core packs such as `SprMain`, `Pharaoh_General`, `Pharaoh_Terrain`, `SprAmbient`, expansion art, monuments, temples, tombs, and enemy sets.

The local engine already has the other half of the pipeline:

- PNG writing;
- path-keyed graphics groups;
- per-image XML metadata;
- mod graphics precedence;
- a Julius extractor and an Augustus extractor;
- runtime consumers that resolve building, figure, resource, tile, and UI images by authored path.

The clean design is a new `PharaohGraphicsExtractor`, not an ever-growing Caesar legacy extractor branch.

### Proposed extractor pipeline

1. **Locate and validate the game installation.** Require original Pharaoh with Cleopatra, verify a small signature set of expected `.sg3`/`.555` pairs, and reject Pharaoh: A New Era assets.
2. **Parse packs generically.** Port or independently adapt the SG3 entry and 555 pixel decoder into a neutral reader with no gameplay knowledge.
3. **Preserve source provenance.** Record pack name, source entry index, group index, animation metadata, offsets, dimensions, compression, and checksum in generated XML or a manifest.
4. **Emit stable path-keyed groups.** Start with pack/group-derived names rather than guessed semantic names. Add semantic aliases only when verified by a building/figure mapping.
5. **Write PNG plus XML in `Mods/Pharaoh/Graphics`.** Match the existing runtime format so no Pharaoh-specific renderer path is required.
6. **Validate referential completeness.** Check every `<graphics>` reference in Pharaoh XML against extracted output, including building-menu icons and resource icons.
7. **Cache by source fingerprint.** Re-extract only when source pack metadata, extractor version, or mapping data changes.

### Extractor effort

| Deliverable | Estimate | Main uncertainty |
| --- | ---: | --- |
| Reader proof: one SG3/555 pack to PNG | 1-2 weeks | exact palette/transparency/footprint parity |
| Core packs with metadata and stable output | 2-4 additional weeks | animation grouping, mirrored entries, offsets, naming |
| Full Cleopatra/monument/enemy coverage | 4-8 additional weeks | many delayed/special packs and validation volume |
| Semantic catalogue suitable for broad XML authoring | ongoing alongside XML | mapping images to game concepts, not decoding pixels |

An extractor that dumps every image is easier than an extractor that gives every image a durable semantic name. The report recommends treating source pack/index as immutable provenance and semantic names as a reviewed mapping layer. That avoids inventing unstable folder taxonomies.

### Local-install finding

This pass did not find an installed Pharaoh/Cleopatra copy in the standard Steam library folders or common GOG locations checked on this machine. That does not affect feasibility, but implementation will need the actual installation path before the reader can be validated against the user's assets. No asset files should be committed; only user-local extracted output or ignored caches should be produced.

## Authoring the Pharaoh data

### Buildings

Purely generic buildings are a good early fit. Roads, wells, firehouses, simple service buildings, farms, raw-material producers, workshops, storage, and basic temples can reuse or extend existing XML concepts if their behavior is close enough.

The difficulty rises when Pharaoh identity depends on behavior rather than presentation. Several of the necessary contracts already exist or are in flight, so the first step for every item should be to exercise the existing XML path before proposing new runtime machinery:

- floodplain farms and the Nile cycle;
- water lifts and irrigation coverage;
- bazaars with Pharaoh goods and distribution rules;
- work camps dispatching labor to farms and monuments;
- Pharaoh-specific monument scale, material delivery, ramps, and worker animations on top of the existing phased-construction XML;
- temples and festival logic for the Egyptian gods;
- mortuaries, physicians, dentists, apothecaries, malaria, and disease;
- scribal education and the Pharaoh housing-service model;
- ship types, river navigation, fishing, and military wharves;
- mansion/palace administration and tax rules;
- Pharaoh campaign, kingdom rating, requests, events, and invasions.

Many of these already have partial generic contracts: resources, gods/religions, foundations, terrain/buildable tiles, production/storage/distribution, and phased construction are not blank slates. The work is to determine whether the existing contract is sufficient, extend it when Pharaoh exposes a missing dimension, and add runtime behavior only where data cannot yet express the rule. Where a concept is genuinely absent, the goal should be a reusable engine capability such as seasonal terrain production or configurable health services—not a `if (Pharaoh)` branch scattered through the simulator.

### Housing

Pharaoh has a 20-stage house ladder, visible in Akhenaten's [house data](https://github.com/dalerank/Akhenaten/blob/fe8df9ef9d71aa16389ada2d51213bd6c84ecb36/src/scripts/houses.js), from crude hut through palatial estate. Conveniently, Julius also currently has 20 `HousingProfile` files. This makes a one-for-one provisional mapping possible and is ideal for gradual replacement.

The numerical equality should not be mistaken for behavioral compatibility. Pharaoh requirements include its own water, entertainment, religion, education, health, food, pottery, linen, jewelry, beer, desirability, crime, disease, malaria, tax, and prosperity rules. The current XML covers much of the shape, but several fields are still Caesar-named (`barber`, `bathhouse`, `oil`, `furniture`, `wine`) or tied to Caesar runtime meanings.

Recommended housing progression:

1. map 20 Pharaoh stages to 20 provisional Julius slots and switch the art/names;
2. author Pharaoh capacities, evolution thresholds, food/goods, prosperity, and tax data;
3. generalize service requirements from named Caesar fields to service IDs/tags;
4. implement Pharaoh-specific health/crime risks through generic modules;
5. only then claim behavioral parity.

### Resources and production

This is an early hard limit. The local engine currently reserves only 24 resource slots including `none`, while the inspected Akhenaten resource enum declares 35 ordinary Pharaoh resource types before currency/troop special values. Pharaoh's set includes grain, meat, lettuce, chickpeas, pomegranates, figs, fish, game meat, straw, clay, bricks, pottery, barley, beer, flax, linen, gems, luxury goods, timber, gold, reeds, papyrus, several stones, chariots, copper, oil, henna, paint, lamps, and marble.

Therefore, the full Pharaoh resource catalogue cannot be achieved by writing XML alone. The resource capacity must be expanded and audited through:

- building and city storage arrays;
- warehouses, granaries, bazaars, carts, and distribution;
- trade and empire/campaign data;
- advisors, UI lists, overlays, editor, and localization;
- save/load layout and compatibility bridges;
- iteration boundaries that still assume Caesar's food/goods ranges.

An early vertical slice can temporarily reuse a few Julius slots by giving them Pharaoh definitions, but that should be visibly labelled provisional. It is useful for proving the pipeline, not a sustainable resource model.

### Figures and walkers

The current `FigureType` profiles already express owner relationship, service roaming, pathing, movement range, and graphics. They should cover several reskinned or mildly adjusted Pharaoh walkers. New behavior is constrained by the finite native-class vocabulary and remaining legacy callbacks. Pharaoh's distinct bazaar buyers/sellers, work-camp labor, monument laborers, tax collectors, entertainers, priests, health walkers, boats, and soldiers will progressively require new generic profiles or native implementations.

This is exactly where Akhenaten is valuable as a behavioral reference and exactly where copying only its data would be insufficient: much of that project's behavior lives in C++ and JavaScript event handlers.

## Difficulty by subsystem

| Subsystem | Initial data conversion | Faithful behavior | Assessment |
| --- | --- | --- | --- |
| Graphics extraction | Medium | Medium | Known format and working reference implementation reduce risk. Semantic mapping is the long tail. |
| Mod layering | Low to medium | Low to medium | Intended but deferred capability; ordered paths and graphics precedence already exist. Complete it first. |
| Building catalogue | Medium | High | Many structures fit generic XML; signature structures need new capabilities. |
| Housing | Medium | High | Existing 20-profile shape is promising; service semantics and health/crime differ. |
| Resources/production | High | High | Current 24-slot ceiling is below Pharaoh's ordinary resource set. |
| Walkers/distribution | Medium | High | Profiles help, but unique actions still need runtime work. |
| Nile/floodplain | High | Very high | Terrain is already on the XML pipeline, but calendar, flood state, fertility, irrigation, farming, and scenario behavior remain a defining integrated system. |
| Monuments | Medium | High | XML-backed monuments and generic phased construction already exist; Pharaoh adds much greater scale, footprints, labor, ramps, delivery, and completion rules. |
| Religion | Low to medium | High | Gods, religions, blessings, tiers, capacity, and presentation already have XML seams; Pharaoh authoring and festival/god semantics remain. |
| UI/localization/audio | Medium | High | Art can be extracted, but screen behavior, messages, voices, and language data are broad. |
| Campaign/scenario/save | Very high | Very high | Original formats and Pharaoh-specific state create a separate compatibility program. |
| Military/river craft | High | Very high | Different units, formations, navigation, wharves, invasions, and balance. |

## Recommended phased program

### Phase 0: finish intended XML layering — a few days to 2 weeks

- Define `Julius, Pharaoh` as the development stack.
- Complete the intended gameplay-registry behavior: load lower-to-upper layers with stable-ID replacement.
- Add explicit disable/suppress semantics for inherited definitions.
- Add startup reporting that identifies the winning source of each definition.
- Add tests for sparse overrides, cross-layer references, duplicate failures within one layer, and missing top directories.

**Exit criterion:** an almost-empty `Mods/Pharaoh` boots by inheriting Julius without copying Julius XML.

### Phase 1: extractor proof — 2-6 weeks

- Locate/validate the original install.
- Extract one core pack, then the general/terrain/main sprite packs.
- Emit current path-keyed PNG/XML format.
- Add source manifest and completeness validator.
- Author one Pharaoh building and one Pharaoh walker using only extracted assets.

**Exit criterion:** a Pharaoh crude hut or well is drawn, previewed, built, animated, and reloaded from a save while the rest of the city remains inherited Julius.

### Phase 2: first playable street block — 1-2 months

- Roads, wells/water supply, firehouse, architect, bazaar, grain farm, granary, storage yard, and basic service walkers.
- First few house stages with Pharaoh art and provisional requirements.
- Small Pharaoh construction menu and localization set.
- Replace obvious Roman UI/building presentation in this slice.

**Exit criterion:** a small visually Egyptian neighborhood can grow and receive food/services, even though much simulation remains Caesar-like.

### Phase 3: resource and housing identity — 2-4 months

- Expand and stabilize resource capacity and save bridges.
- Author the core Pharaoh food/material/goods chains.
- Complete the 20-stage housing ladder.
- Generalize service requirements and implement Pharaoh health/crime modules.
- Finish bazaar/storage/trade UI for the expanded resource set.

**Exit criterion:** housing progression and the ordinary economy feel mechanically Pharaoh-like, not merely reskinned.

### Phase 4: Nile and civic identity — 2-5 months

- Extend the terrain XML pipeline through Nile flood state, floodplain fertility, irrigation, water lifts, and work-camp farming.
- Author Egyptian gods/religions through the existing XML registries, then add missing festival and god-effect semantics.
- Education, health, administration, entertainment, and river industries.
- Scenario rules and representative objectives.

**Exit criterion:** a purpose-built sandbox scenario depends on Pharaoh's defining systems.

### Phase 5: monument fidelity, campaign, combat, and compatibility — open-ended

- Extend the existing phased-construction XML/runtime for Pharaoh monument scale, footprints, ramps, specialized labor, delivery, and completion behavior.
- Military, invasions, naval/river craft, and enemies.
- Original map/save/campaign support where desired.
- Cleopatra content, special monuments, events, audio, and UI completion.

**Exit criterion:** campaign-parity goals should be declared individually; there should be no vague single “Pharaoh complete” milestone.

## How to keep the conversion honest

The proposal's most valuable principle is that every Pharaoh obstacle should expose a Caesar-only assumption. The following rules would preserve that value:

- Keep `Pharaoh` a separate top mod, never a theme flag inside Julius.
- Do not commit original game assets; extract them from the user's licensed installation.
- Prefer generic runtime capabilities configured by XML over game-name conditionals.
- Record provisional slot reuse and Caesar fallback behavior explicitly in the Pharaoh data.
- Require each converted feature to include graphics, authored data, runtime behavior, save/load, UI, and validation—not only a sprite swap.
- Preserve the ability to boot a tiny Pharaoh layer throughout development.
- Use Akhenaten as a reference implementation and behavior oracle, but translate into this project's XML/object architecture.
- Treat original Pharaoh behavior and this project's future design changes as separate modes or decisions; do not lose the baseline while adding improvements.

A practical identity scorecard for each milestone is:

1. **Presentation:** Pharaoh graphics, text, sounds, and menus.
2. **Data ownership:** Pharaoh definitions exist in XML and do not rely on unexplained Caesar aliases.
3. **Simulation:** the feature obeys Pharaoh rules or a documented intentional variation.
4. **Persistence:** save/load preserves the new state.
5. **Isolation:** Julius still works and contains no Pharaoh-specific branch.

Only a feature satisfying all five should be called converted.

## Principal risks

1. **Mistaking data volume for engine completeness.** Hundreds of authored XML files can still sit on Caesar-specific simulation.
2. **Resource/save ABI expansion.** Enlarging fixed resource arrays and introducing new dynamic identities can break old saves unless bridges are designed early.
3. **Special-case accumulation.** A sequence of `Pharaoh` checks would defeat the engine-generalization purpose.
4. **Semantic asset naming churn.** Raw pack indices are stable; guessed names are not. Provenance and aliases must be separated.
5. **Incomplete mod inheritance.** A shadow copy can silently drift from Julius and make fixes appear to vanish.
6. **Scope collapse into full parity.** The conversion needs small playable exit criteria so it can deliver value years before complete campaign compatibility.
7. **Reference-code drift.** Akhenaten is active; this report pins a commit so future research can distinguish current upstream changes from the inspected evidence.

## Recommendation

Proceed, but begin with infrastructure rather than bulk XML authoring.

The first implementation package should be narrowly defined as **“Sparse Pharaoh layer plus extracted crude-hut vertical slice.”** It should contain:

- true lower-layer XML inheritance;
- `Mods/Pharaoh` with only the definitions it changes;
- a Pharaoh SG3/555 reader and current-format graphics output;
- one house stage, one service building, one walker, one resource, and a minimal menu/localization path;
- startup and referential validation;
- a short save/load test.

If that package remains clean, the architecture is suitable for the larger conversion. If it requires pervasive Caesar/Pharaoh branches, that is evidence to generalize the relevant runtime contract before scaling the data set.

The extractor is not the largest risk. It is an attainable first subsystem with unusually strong prior art. The true project is gradually replacing Caesar-shaped simulation assumptions with data-driven Impressions-engine concepts while keeping Julius working beneath the new layer.

## Sources inspected

### Online primary sources

- [Ozymandias_Julius repository and project status](https://github.com/Banderi/Ozymandias_Julius)
- [Akhenaten repository and README](https://github.com/dalerank/Akhenaten)
- [Akhenaten SG3/555 image-pack reader at the inspected commit](https://github.com/dalerank/Akhenaten/blob/fe8df9ef9d71aa16389ada2d51213bd6c84ecb36/src/content/imagepak.cpp)
- [Akhenaten image-pack catalogue at the inspected commit](https://github.com/dalerank/Akhenaten/blob/fe8df9ef9d71aa16389ada2d51213bd6c84ecb36/src/scripts/imagepaks.js)
- [Akhenaten Pharaoh house data at the inspected commit](https://github.com/dalerank/Akhenaten/blob/fe8df9ef9d71aa16389ada2d51213bd6c84ecb36/src/scripts/houses.js)
- [Akhenaten building script catalogue at the inspected commit](https://github.com/dalerank/Akhenaten/tree/fe8df9ef9d71aa16389ada2d51213bd6c84ecb36/src/scripts/building)
- [Akhenaten resource enum at the inspected commit](https://github.com/dalerank/Akhenaten/blob/fe8df9ef9d71aa16389ada2d51213bd6c84ecb36/src/game/resource.h)

### Local grounding points

- `src/game/mod_manager.cpp`
- `src/assets/xml_path_resolution.cpp`
- `src/assets/augustus_asset_extractor.cpp`
- `src/assets/graphics_extractor_common.cpp`
- `src/core/legacy_image_extractor.cpp`
- `src/building/building_type_registry_xml.cpp`
- `src/building/housing_profile_registry.cpp`
- `src/building/production_method_registry.cpp`
- `src/building/storage_type_registry.cpp`
- `src/figure/figure_type_registry.cpp`
- `src/game/resource.cpp` and `src/game/resource.h`
- representative definitions throughout `Mods/Julius`
