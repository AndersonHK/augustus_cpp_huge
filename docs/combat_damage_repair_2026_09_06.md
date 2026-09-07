# Wall and projectile damage repair — 2026-09-06

Reproduction: `Consul - broken tower.svv`, native save version 205.

## Causes and changes

- Building damage used an unsigned byte, including its undo backup and save payload. Vespasian walls have 600 HP, so the counter wrapped at 256 before destruction could occur. Damage now uses a 32-bit grid throughout runtime, backup, serialization and reload. Authored HP and attack cadence are unchanged.
- `Figure::create` initializes tile progress to 128. Projectiles reuse that field as age and expire after 120 actions, so newly created projectiles expired on their first update. Missile creation now initializes age to zero. Explosion clouds had the same problem against their 44-action lifetime and receive the same correction.
- Native save version **206 / 0xCE** carries the wider damage grid. Older native saves and legacy imports retain their serialized byte damage. Damage already lost to an earlier counter wrap cannot be inferred from the remaining byte.
- Alive projectiles and explosion clouds saved with the erroneous initial age of 128 are repaired on import, with a warning. Ordinary walkers and melee combat timers are unaffected. Newly created effects are saved with correct ages.

## Validation

The new `--combat-test` option requires `--load-save-test`. Its fixtures modify a temporary loaded city; the original input or canonical roundtrip is reloaded before the rendered soak. Use a Vespasian city with a high-HP wall and an unoccupied flight area.

- Before the fix, the test failed with `New projectile expired on its first update`.
- After the fix, all six projectile types hit stationary targets across 12 trajectories each: eight directions and four shallow/steep paths, reaching up to 15 tiles.
- Ballista bolts kill unarmored enemy targets. Other friendly and hostile projectiles inflict damage on their respective target categories.
- All 16 explosion clouds remain alive after their first update and expire at their intended lifetime.
- A version-205 fixture with a bolt and an explosion at age 128 logs exactly the two effect repairs, writes age zero in version 206, and reloads cleanly. Ordinary enemy and tower tile progress remains 128. Output: `out/combat-timer-migration.log` / `.err`.
- A 600-HP wall retains 300 damage through undo backup/restore and serialization/reload, then is destroyed when damage exceeds its HP. Importing an older byte damage grid preserves its value.
- The actual Consul encounter passes a 3,000-tick rendered soak and a version-206 save/reload. Enemy 94, attacking the wall at grid 16471, is killed. Enemies 65 and 97 destroy the wall at grid 14843 and advance; enemy 97 also takes further damage.
- The original Consul save emits 22 existing surface-ownership migration warnings. Its canonical reload, rendered soak and final reload emit no warnings or errors.
- The full startup gate passed (exit 0): startup checks for all three base mod stacks, the original campaign fixture, Julius/Augustus dependency-stack saves, and the 67-save representative cohort. All 70 rendered city soaks completed 3,000 ticks, including native `.svv` and legacy `.sav` / `.svx` imports. Canonical reloads and soaks passed their strict diagnostics checks; expected negative parser fixtures remain visible in the gate log.

Focused test output: `out/combat-before-test.err` and `out/combat-final-test.log` / `.err`.
Broader startup validation output: `out/combat-startup-gate.log` / `.err`.

## Installed build

Runtime-only deployment to `D:\Games\GOG Games\Caesar 3` completed. The installed executable matches the tested Release build (SHA-256 `0e3aaf1fdc7135e7be5fa18269766f08d6234458bf86e2b6b724acf7e6daa5ab`). The installed load/render smoke test passed with empty stderr: `out/combat-installed-smoke.log` / `.err`.

The original user save is not overwritten. Changes are not committed.
