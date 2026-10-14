# Vespasian Pixel Art AI model retirement

On 2026-10-14, the maintainer authorized deletion of the opted-out AI model weights and cache in `C:\Users\imper\Documents\GitHub\VespasianPixelArt`. This is a standalone workspace without Git metadata; this game repository records the cleanup and its relationship to the withdrawn racer prototype. The workspace README and source notes were updated in place.

## Space audit and completed removal

Sizes are summed file lengths, in decimal GB, rather than an NTFS allocated-space measurement. Snapshot symlinks have zero file length and were not counted twice. The two checked copies of the shared FLUX text-encoder blob were separate files, not hard links.

| Location | Before (GB) | Action |
| --- | ---: | --- |
| `model_cache/hub/models--black-forest-labs--FLUX.2-klein-4B` | 23.740 | Removed |
| `model_cache/hub/models--black-forest-labs--FLUX.2-klein-base-4B` | 23.740 | Removed |
| Other `model_cache` content: pixel-art/spritesheet LoRAs and BiRefNet | 1.175 | Removed |
| `models/sdxl-base` | 6.941 | Removed |
| `models/sdxl-canny` | 2.502 | Removed |
| `models/birefnet` | 0.973 | Removed |
| `.venv` | 3.566 | Retained |
| Quaternius model pack, art, metadata, scripts, and other workspace files | 0.127 | Retained |

The workspace started at **62,764,202,463 bytes** (62.764 GB). Deleted targets totaled **59,071,006,867 bytes** (59.071 GB), approximately 94.1% of its original file bytes. After removal and metadata/documentation preservation, it measured approximately **3.701 GB**. Small evidence files added afterward account for minor differences in subsequent totals.

This was predominantly obsolete research model storage. `work` was only 49.94 MB, `candidates` 22.79 MB, and `archive` 14.57 MB. Purging those art artifacts would have saved little while losing useful history. Each FLUX snapshot contained both a roughly 7.75 GB standalone transformer checkpoint and a roughly 7.75 GB Diffusers transformer representation, plus roughly 8.04 GB of text-encoder shards. The snapshots also repeated shared encoder/VAE blob identifiers across separate per-model caches.

## Retained material

- Quaternius's CC0 farm-animal pack, including `Horse.blend`, and its original ZIP. The Blender configuration actively references this model; it is not an AI weight.
- Every existing full-resolution art source, guide, runtime/reference package, preview, baseline, audit, art-direction reference, rejected candidate sample, and archived prototype.
- Scripts, configuration, provenance, generation manifests with prompts/seeds and recorded parameters, and the portable Python environment. Existing AI Python dependencies remain installed; this task removed model storage, not packages.
- `docs/retired_ai_models_2026-10-14/` in the standalone workspace: removed-file inventory with resolved byte sizes/blob names and link targets, snapshot revision refs, small configuration/index/license/model-card files, download metadata, Python package inventory, cleanup totals, and retained-art SHA-256 records. Metadata was dereferenced into regular files before deleting cache links; no retired weights were copied or archived.

The deterministic Blender/Pillow pipeline uses none of the removed AI weights. Historical optional FLUX/SDXL scripts use local-only model loading and need an explicit model reinstall before reuse. Archived documentation remains a historical snapshot and can mention the formerly installed model stack.

## Verification and remaining scope

All **7,073** protected files passed before/after SHA-256 comparison. The four deletion targets were resolved and checked to be ordinary directories beneath the named standalone workspace before removal. Source model and artwork were preserved; no game graphics, code, or runtime definitions changed. Both retained Pillow entry points (`race_asset_pipeline.py --help` and `package_race_assets.py --help`) completed successfully after cleanup. This verifies imports/CLI availability, not a fresh Blender render or game soak.

Further cleanup of the roughly 3.57 GB Python environment is separate work: record a reproducible lightweight Pillow environment, validate derivation/packaging with it, then retire the unused AI dependencies. Keep the current environment until that replacement is verified. There is little value in pruning the small art history now.
