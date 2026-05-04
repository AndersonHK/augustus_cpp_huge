# Save/Load Legacy Path Ledger

This ledger tracks hidden legacy save/load paths exposed while hardening the `0xb6` owned save heap. Each entry should identify the runtime action that hit the path, the legacy function involved, why it is not acceptable for `0xb6`, and the intended cleanup direction.

## Entries

| # | Trigger | Legacy path | What happened | Intended cleanup |
| --- | --- | --- | --- | --- |
| 1 | Selecting a save in the save/load dialog, likely while rebuilding the preview/minimap | `building_get_from_buffer()` -> `building_state_load_from_buffer(..., for_preview = 1)` | The preview path tried to read the `buildings` piece as legacy fixed-size building records. The new `0xb6` heap guard correctly raised a fatal error: `0xb6 building preview attempted to use the legacy building buffer reader; this line is wrong and needs to be cleaned up`. | Add a `0xb6` preview/minimap path that reads building preview data through the owned building save heap facade, or stores a dedicated preview payload. Legacy preview reads must remain limited to `0xb5` and older saves. |

## Rules For New Entries

- Record the user action or loader phase that triggered the path.
- Name the exact function chain and save piece.
- State whether the path read legacy bytes, reconstructed legacy state, or silently fell back.
- Mark the desired replacement owner: save heap, BuildingType bridge, dedicated preview payload, or old-save compatibility import.
- Keep `0xb6` failures loud until the replacement path exists.
