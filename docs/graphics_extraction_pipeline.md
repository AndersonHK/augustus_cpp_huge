# Graphics Extraction Pipeline

This document is the current handoff for the generated graphics pipeline. It covers the runtime extractors, the standalone harness, and the rules that keep Julius, Augustus, and future Vespasian graphics compatible in the same mod stack.

## Current Shape

Vespasian does not currently ship its own generated graphics. A clean generated output should have:

- `Mods/Julius/Graphics`
- `Mods/Augustus/Graphics`
- no `Mods/Vespasian/Graphics`

The default runtime stack is still Vespasian over Augustus over Julius. The graphics merge grain is an image entry id inside an assetlist, not an entire XML file. If a higher mod provides `Aesthetics\Statue.xml` with only one replacement image id, lower-mod entries in the same group remain available.

Runtime extraction assumes `Vespasian.exe` is running from the Caesar 3 game folder. Augustus source files come from:

```text
assets\Graphics
```

The runtime writes generated graphics into:

```text
Mods\Julius\Graphics
Mods\Augustus\Graphics
```

Julius extraction must run before Augustus extraction. `src/core/image.cpp` is compiled as C++ in the runtime and harness projects, and `image_load_climate(..., extract_legacy_graphics = 1)` now constructs `RuntimeGraphicsExtractionService` directly. The service calls `JuliusExtractor` first and then `AugustusExtractor` before asset XML loading. `src/platform/augustus.cpp` should not start the Augustus graphics extractor early, because Augustus needs the extracted Julius template data to resolve numeric legacy references and image numbering.

## Standalone Harness

The standalone project is:

```text
AugustusGraphicsExtractor.vcxproj
```

Build it from the repo root:

```powershell
MSBuild AugustusGraphicsExtractor.vcxproj /p:Configuration=Release /p:Platform=x64 /m:1 /p:MultiProcessorCompilation=false
```

The current full clean-run command used for validation is:

```powershell
.\x64\Release\AugustusGraphicsExtractor.exe `
    --game-root 'D:\Games\GOG Games\Caesar 3' `
    --julius-graphics 'x64\Release\Mods\Julius\Graphics' `
    --output 'x64\Release\Mods\Augustus\Graphics' `
    --extract-julius-first
```

CLI options:

```text
--game-root <path>          Caesar 3 folder. Defaults to current directory.
--source-graphics <path>    Source packaged assets. Defaults to <game-root>\assets\Graphics.
--output <path>             Extract output. Defaults to <game-root>\Mods\Augustus\Graphics.
--julius-graphics <path>    Julius template graphics. Defaults to <game-root>\Mods\Julius\Graphics.
--extract-julius-first      Simulate runtime: run Julius, then Augustus bootstrap.
--no-force                  Reuse matching stamped output when possible.
--stamp                     Write/check the Augustus extraction stamp.
```

Harness defaults are intentionally test-friendly:

- `--output` and `--julius-graphics` may point into the repo Release tree.
- Augustus extraction is forced by default.
- The harness does not write an Augustus stamp unless `--stamp` or `--extract-julius-first` is passed.
- `--extract-julius-first` calls `image_load_climate()`, which runs Julius extraction and then the runtime Augustus bootstrap. The explicit configured Augustus call becomes a stamped freshness/source-path check, so the normal clean-run output has one Julius summary and one Augustus summary.

Runtime defaults are different:

- `RuntimeGraphicsExtractionService::bootstrapAfterClimateLoad(...)` runs Julius and Augustus with `force = 0` and `write_stamp = 1`.
- Runtime source and output paths are resolved from the game folder.
- The stamp allows runtime startup to skip extraction when the source fingerprint and metadata version match.

## Output Contract

Generated graphics use one XML assetlist per logical group:

```text
Mods\<Mod>\Graphics\<Family>\<Group>.xml
Mods\<Mod>\Graphics\<Family>\<Group>\*.png
```

The XML `assetlist name` is the logical group key using backslashes, for example:

```xml
<assetlist name="Health_Culture\Colosseum">
```

Local PNG references use local names:

```xml
<layer src="Image_0000" part="footprint"/>
```

Cross-group references use `group` and `image`:

```xml
<image id="Image_0001" group="Aesthetics\House_Tent_Variants" image="Image_0000"/>
```

Alias XMLs are a normal part of the output. They preserve source-visible names or old logical paths while pointing at the canonical generated group. Do not drop an alias just because the canonical path exists; inspect the collision and decide whether the source is exposing a real alternate name, a wrapper, or a true conflict.

## Julius Extractor

Implementation:

```text
src/core/legacy_image_extractor.h
src/core/legacy_image_extractor.cpp
```

Julius is stable and atlas-table driven. The extractor receives decoded `image` entries, legacy `group_image_ids`, and `image_atlas_data` from the normal Caesar 3 atlas load. It exports canonical XML and PNGs for the current climate package.

Important Julius rules:

- The extractor splits groups using the legacy group image table.
- Output family/group names are semantic where the legacy group id is known.
- Isometric footprint PNGs are trimmed when safe, while XML `height` and layer `y` preserve logical placement.
- `.legacy_extract.manifest` records generated files and directories so stale output can be removed before a rewrite.
- `.legacy_extract.stamp` currently uses prefix `legacy_extract_v8:`.
- `JuliusExtractor::resolveLegacyGroup()` maps a legacy group id to a canonical group key.
- `JuliusExtractor::resolveLegacyImage()` maps a legacy group id plus source-visible image offset to the canonical split group and image id.

The `House_Tent` case is intentionally special. BuildingType XML uses `Aesthetics\House_Tent/Image_0000..Image_0005`, but the packaged legacy group table splits the first tent image from the remaining tent variants. The extractor preserves the split and exports:

```text
Aesthetics\House_Tent.xml
Aesthetics\House_Tent_Variants.xml
```

`House_Tent.xml` exposes `Image_0001..Image_0005` as full-image aliases to `House_Tent_Variants`. This is not a bad merge, and those images should not be reassigned to `House_Shack`.

## Augustus Extractor

Implementation:

```text
src/assets/augustus_asset_extractor.h
src/assets/augustus_asset_extractor.cpp
src/assets/augustus_julius_template_resolver.h
src/assets/augustus_julius_template_resolver.cpp
src/assets/graphics_extractor_common.h
src/assets/graphics_extractor_common.cpp
```

The public C++ surface is intentionally small:

- `LegacyClimateAtlas`, `JuliusExtractor`, and `GroupImageKey` live in `src/core/legacy_image_extractor.h`.
- `ExtractorPaths`, `ExtractorOptions`, `ExtractionReport`, `AugustusExtractor`, and `RuntimeGraphicsExtractionService` live in `src/assets/augustus_asset_extractor.h`.
- Runtime climate loading calls `RuntimeGraphicsExtractionService` directly from the C++-compiled `src/core/image.cpp`; extractor order belongs to the service.
- `XmlReader`, `XmlElement`, and `XmlToken` in `graphics_extractor_common` are the extractor-owned XML parser. The Augustus extractor and Julius template resolver no longer depend on `core/xml_parser.c`.
- `JuliusTemplateCatalog`, `JuliusTemplateGroup`, and `JuliusTemplateImage` own extracted-Julius template parsing and translation.
- The standalone harness owns argument parsing through `HarnessCli`, then calls `AugustusExtractor` directly for the configured extraction pass.

Augustus is dynamic and source-packaged. The distributed Caesar 3 `assets\Graphics` folder contains packed XML plus packed PNG atlases. Those packed files are generated from Augustus source graphics under the repo's `res\assets\Graphics`, but runtime extraction must only depend on the game-folder `assets\Graphics` package.

Current extraction flow:

1. Fingerprint top-level packaged Augustus source XML and PNG files.
2. Parse each packed atlas XML.
3. Load the matching packed PNG atlas.
4. Build output groups from wrapper names, local references, and translated legacy references.
5. Translate numeric Julius references through the extracted Julius template resolver.
6. Offset generated `Image_####` ids after visible Julius ids when the same group exists in Julius.
7. Resolve local references against the output group that owns each source image after grouping.
8. Export materialized crops/composites and alias groups.
9. Write `.graphics_extract.stamp` when runtime bootstrap or the caller requests stamping.

The Augustus stamp prefix is currently `augustus_extract_v2:`.

### Naming

Use semantic names wherever the packed wrappers expose them. `Colosseum`, `Colosseum_Show`, `Caravanserai_S_OFF`, and `House_Tent_Variants` are useful output names. `48.xml` and arbitrary numbered folders are not.

`Image_####` ids are acceptable for legacy or generated numeric slots, but preserve source ids and wrapper ids when they carry meaning. The current extractor also offsets generated `Image_####` ids after the visible Julius range for the same group so Augustus can add entries without colliding with lower-stack Julius ids.

### Julius Template Resolver

The Augustus extractor needs Julius extraction to exist first. The resolver:

- parses extracted Julius XML templates;
- translates numeric legacy group/image references into split-aware canonical keys;
- discovers the next generated `Image_####` index for a group;
- tolerates Julius XML animation/frame children even when only layer-part structure is needed.

If a brand-new Augustus asset has no Julius template, that can be expected. Local Augustus references and wrapper structure should carry the asset when possible.

### Local References

Packed Augustus XML frequently uses local `group="this"` references or wrapper images that refer to other source ids. After the extractor groups and renames images, local references must resolve through the final owning output group, not through the original packed atlas name.

This is why `LocalReferenceTargets` exists in `augustus_asset_extractor.cpp`. Without that mapping, images can be exported correctly but still reference the wrong output group after grouping or id offsetting.

### Aliases and Collisions

Augustus exposes many source-visible names for the same art. When a wrapper or visible alias points at an output group that already exists:

- prefer exporting an alias group that forwards image ids to the canonical group;
- do not skip the alias silently;
- do not inflate materialized image counts with alias-only wrappers;
- check whether the alias image ids need to line up with Julius ids for mod-stack replacement.

This supports future mods that override a single image entry in a lower stack group.

## Current Validation Baseline

Against the current `D:\Games\GOG Games\Caesar 3\assets\Graphics` sample:

```text
Julius:   231 XML, 8933 PNG, 8465 logical images
Augustus: 3200 XML, 4088 PNG, 3259 logical images
Vespasian Graphics: absent
```

The BuildingType graphics reference validator currently sees:

```text
graphics_groups=3398 graphics_refs=494 button_icon_refs=152 checked_refs=646 missing=0
```

`graphics_refs` above counts explicit BuildingType path/image references across Augustus and Vespasian in the Release mod stack. `button_icon_refs` counts `<button icon="..." icon_image="...">` references; `icon` is a generated graphics group key and `icon_image` is optional, falling back to the group's default image when omitted.

The broad generated graphics XML cross-reference scan currently reports 28 unresolved references across 7 target groups. Those are not BuildingType misses and are not tent-related. Known buckets include:

- `UI\Big_People`
- `UI\Ratings_Background`
- `UI\Message_Images`
- `Warriors\Fort_Formations`
- `Environment\Group_206/Construction_Part_TimberPad_Shadow`
- catapult rock frames under `Warriors`

The timber pad and catapult rock misses are known Augustus upstream issues from unfinished or alpha features.

## Recommended Clean Validation

From the repo root, after building Release:

```powershell
$releaseRoot = (Resolve-Path 'x64\Release').Path
$targets = @()
$targets += (Join-Path $releaseRoot 'Mods\Julius\Graphics')
$targets += (Join-Path $releaseRoot 'Mods\Augustus\Graphics')
$targets += (Join-Path $releaseRoot 'Mods\Vespasian\Graphics')
foreach ($target in $targets) {
    $parent = Split-Path -Parent $target
    if (-not (Test-Path -LiteralPath $parent)) { continue }
    $resolvedParent = (Resolve-Path -LiteralPath $parent).Path
    if (-not $resolvedParent.StartsWith($releaseRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to delete outside Release: $target"
    }
    if (Test-Path -LiteralPath $target) {
        $resolvedTarget = (Resolve-Path -LiteralPath $target).Path
        if (-not $resolvedTarget.StartsWith($releaseRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to delete outside Release: $resolvedTarget"
        }
        Remove-Item -LiteralPath $resolvedTarget -Recurse -Force
    }
}
$targets = @()
$targets += (Join-Path $releaseRoot 'Mods\Julius\Graphics.legacy_extract.stamp')
$targets += (Join-Path $releaseRoot 'Mods\Julius\Graphics.legacy_extract.manifest')
$targets += (Join-Path $releaseRoot 'Mods\Augustus\Graphics.graphics_extract.stamp')
foreach ($target in $targets) {
    if (-not (Test-Path -LiteralPath $target)) { continue }
    $resolvedTarget = (Resolve-Path -LiteralPath $target).Path
    if (-not $resolvedTarget.StartsWith($releaseRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to delete outside Release: $resolvedTarget"
    }
    Remove-Item -LiteralPath $resolvedTarget -Force
}

.\x64\Release\AugustusGraphicsExtractor.exe `
    --game-root 'D:\Games\GOG Games\Caesar 3' `
    --julius-graphics 'x64\Release\Mods\Julius\Graphics' `
    --output 'x64\Release\Mods\Augustus\Graphics' `
    --extract-julius-first
```

Then check:

```powershell
Get-Content 'x64\Release\Mods\Julius\Graphics\Aesthetics\House_Tent.xml'
Test-Path 'x64\Release\Mods\Vespasian\Graphics'
Test-Path 'x64\Release\Mods\Julius\Graphics\UI\Group_018.xml'
Test-Path 'x64\Release\Mods\Julius\Graphics\Aesthetics\House_Tent_Variants.xml'
git diff --check
```

Expected boolean checks:

```text
Mods\Vespasian\Graphics: False
UI\Group_018.xml: False
Aesthetics\House_Tent_Variants.xml: True
```

Validate BuildingType graphics and button icons against the Release mod stack:

```powershell
$release = (Resolve-Path 'x64\Release').Path
$groups = @{}
foreach ($mod in @('Julius', 'Augustus', 'Vespasian')) {
    $graphicsRoot = Join-Path $release "Mods\$mod\Graphics"
    if (-not (Test-Path $graphicsRoot)) { continue }
    foreach ($file in Get-ChildItem $graphicsRoot -Recurse -Filter *.xml) {
        $key = $file.FullName.Substring($graphicsRoot.Length + 1, $file.FullName.Length - $graphicsRoot.Length - 5).Replace('/', '\')
        if (-not $groups.ContainsKey($key)) {
            $groups[$key] = [System.Collections.Generic.List[string]]::new()
        }
        [xml]$xml = Get-Content $file.FullName -Raw
        foreach ($node in $xml.SelectNodes('//image[@id]')) {
            $groups[$key].Add([string]$node.id)
        }
    }
}

$refs = @()
foreach ($mod in @('Augustus', 'Vespasian')) {
    foreach ($file in Get-ChildItem (Join-Path $release "Mods\$mod\BuildingType") -Filter *.xml) {
        [xml]$xml = Get-Content $file.FullName -Raw
        foreach ($pathNode in $xml.SelectNodes('//path[@value]')) {
            $imageNode = $pathNode.ParentNode.SelectSingleNode('image[@value]')
            $refs += [pscustomobject]@{
                Kind = 'graphics'
                Group = [string]$pathNode.value
                Image = if ($imageNode) { [string]$imageNode.value } else { '' }
            }
        }
        if ($xml.building.button.icon) {
            $refs += [pscustomobject]@{
                Kind = 'button_icon'
                Group = [string]$xml.building.button.icon
                Image = [string]$xml.building.button.icon_image
            }
        }
    }
}

$missing = $refs | Where-Object {
    -not $groups.ContainsKey($_.Group) -or
    ($_.Image -and -not ($groups[$_.Group] -contains $_.Image)) -or
    (-not $_.Image -and $groups.ContainsKey($_.Group) -and $groups[$_.Group].Count -eq 0)
}
"graphics_refs=$(($refs | Where-Object Kind -eq 'graphics').Count) button_icon_refs=$(($refs | Where-Object Kind -eq 'button_icon').Count) checked_refs=$($refs.Count) missing=$($missing.Count)"
```

Expected:

```text
graphics_refs=494 button_icon_refs=152 checked_refs=646 missing=0
```

`git diff --check` may print CRLF conversion warnings on Windows; those are not whitespace errors.
