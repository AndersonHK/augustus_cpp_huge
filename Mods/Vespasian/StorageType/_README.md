# StorageType XML

The loader reads every `*.xml` file in this folder at startup. Keep templates and notes in non-`.xml` files so they do not get treated as live data.

Registry keys are the file path relative to this folder, without the `.xml` suffix.

For historical capacity, spoilage, moisture, guard, and port-storage context,
see [Roman City Facility Ratios](../../../research/roman_city_facility_ratios.md)
and [Roman Building and Infrastructure Maintenance Needs](../../../research/roman_building_maintenance_needs.md).
Ostia's horrea are the main storage model for port and annona-heavy cities.

Example:

- `Mods\Vespasian\StorageType\clay_input_pottery_workshop.xml` is referenced as `clay_input_pottery_workshop`

Prefer descriptive names that include both the resource and the owning building context.

Current supported nodes:

- `<storage_type role="input|output">`
- `<accepts resource="..." />`
- `<capacity amount="N" />`

Rules:

- `role` is required; input storage receives delivery carts and feeds production inputs, output storage receives production output and dispatches export carts
- `<accepts>` may appear one or more times
- `resource` must use the existing resource xml names
- `<capacity>` is optional
- `amount` is stored as raw resource units
- one full cartload is 100 raw resource units; fractional farm output can be stored below a full load, but carts reserve and move only whole loads

The current vertical slice uses StorageType as shared authored metadata for native building-owned storage slots.
Implemented production storage definitions now cover native farms, ordinary one-output workshops, and raw-material producers: clay, timber, iron, marble, gold, stone, and sand where the mod supports those resources.
