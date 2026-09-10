# ProductionMethod XML

The loader reads live `*.xml` files. Registry keys are paths relative to this directory without the extension. Use non-XML suffixes for examples.

## Output and timing

- `<kind value="farm|workshop" />` selects ordinary resource production.
- `<output resource="..." production_per_month="N" />` declares absolute nominal inventory units per game month, independent of cycle size.
- `<output resource="..." rate_from="method_path" />` shares another method's monthly rate, including overrides.
- `<cart_loads value="N" />`, or `<cart_loads numerator="N" denominator="D" />`, sets output per completed cycle. The default is one load; each load is 100 inventory units. Fractions must produce whole inventory units.
- `<cart_capacity value="N" />` sets transport capacity without changing production rate or cycle work.
- `<climate_bonuses><bonus climate="central|northern|desert" percent="N" /></climate_bonuses>` adjusts the monthly rate.

Required work for a resource cycle is the legacy month-normalized work multiplied by `cart_loads` once. Larger harvests take longer; smaller harvests take less work. Both farms and workshops use this rule. Five wheat fields at 32 units/month produce 160/month, or 1,920/year, regardless of whether each field harvests 0.2 or 1 load. Actual delivery can vary with integer updates, crop phase, employment, storage, and transport.

`batch_size` has been retired. Its shipped values were all 1. Declare output size with `cart_loads`; no additional output or work multiplier exists.

## Inputs and other production modes

- `<input resource="..." amount="N" />` declares literal inventory units consumed per completed cycle. Multiple inputs are supported. **Inputs do not scale with `cart_loads`.** Increasing output per cycle while keeping input quantities unchanged changes the material ratio.
- `<production_method input_source="global_stockpile">` consumes inputs from the global stockpile; the default is building storage.
- `<treasury_cost amount="N" />` charges a literal amount per cycle.
- `<output effect="spawn_fishing_boat" production_per_month="N" />` keeps its existing event timing and per-event inputs; cartload scaling applies to resource outputs.
- `<output resource="fish" production_per_month="N" source="figure_delivery" />` describes delivery-driven production. Its figure owns the actual production loop.
- `<kind value="delay_factor" />` with `<output resource="..." delay_percent="N" />` scales a delay rather than producing inventory. It is independent of cartloads.

See [production throughput and validation baseline](../../../docs/production_throughput.md) for the equations, calendar comparison, historical correction, and upstream rate reference. Julius/Augustus use 50 ticks per day and 16 days per month; Vespasian uses 100 ticks per day and its 365-day calendar. Month length cancels out of nominal output per month.
