# Housing density, recurring service costs, and fractional taxes

Historical planning snapshot: 2026-09-09. The per-building goods assumptions and proposed-tax tables below predate the implemented annual per-resident rules. See [the current consumption and Augustus/Vespasian budget tables](housing_annual_consumption_2026_09_09.md).
Companion to [the housing demand proposal](vespasian_housing_demand_rebalance_2026_09_09.md).

## Corrected comparison principle

Compare the same residential area within the same walker-service catchment.
Let population and potential workforce grow with housing capacity. Charge the
shared service buildings once, then add new requirements and any demonstrated
throughput-driven expansion. Report both district totals and recurring cost per
resident. Construction costs are outside this comparison.

The earlier version held population at 1,000 while also holding most service
counts constant. That concealed the extra catchments needed for lower-density
housing and cannot establish which housing is efficient. Its tables have been
replaced. Import-only tax deficits are a financing scenario, not a ranking of
housing's total economic value; surplus workers and service density matter too.

Three different scaling rules must remain distinct:

- Walker services: largely shared by geography; their wages and maintenance are
  amortized over more residents as houses become denser, until routes or service
  throughput require another building.
- Household goods: consumed per occupied building and cadence, so an unchanged
  good costs the same per merged house as it gains residents. Merging itself
  reduces the bill relative to four separate 1x1 houses in this runtime.
- Food: consumption grows with population. Greater density does not eliminate
  the additional ration, although distribution infrastructure can be shared.

## Defined area and recurring-cost assumptions

The benchmark reserves **144 residential tiles**, plus shared roads and service
space. This area is divisible by every housing footprint: 36 merged/fixed 2x2
houses, 16 houses at 3x3, or 9 at 4x4. Every house is full. This is an area budget,
not a claim that a solid 12x12 square has road or walker access. Service space is
reserved separately, so a new service does not displace a house in this model.
A constrained real layout must subtract displaced housing and include any
additional road frontage or supporting industry land.

Use Hard difficulty, 7% taxes, full tax coverage, wages 30, and the fixed 38%
plebeian workforce setting. Annual wages are approximately 3 per employee;
potential workers are an aggregate estimate before rounding or blessings.
Patricians provide zero workers. No bonuses, tourism revenue, trade modifiers,
festivals, or discretionary military/civic expenses are assumed.

The service bundle below is held geographically constant unless the housing
requirements add a service. Coverage, market delivery throughput, water coverage,
and entertainment supply have **not** been proven with a constructed layout.
At the higher populations, more markets or service buildings may be required;
the current counts are a shared-service benchmark, not guaranteed minimum costs.

| Component | Assigned buildings | Staff | Annual maintenance levy |
| --- | --- | ---: | ---: |
| Common safety and taxes | 2 prefectures, 1 engineers' post, 1 forum | 23 | 0 |
| Food distribution, when food required | 2 markets, 1 granary | 16 | 0 |
| Manufactured-goods storage, when required | 1 warehouse | 6 | 0 |
| Well water | Wells sufficient for the catchment | 0 | 0 |
| Fountain water | 5 fountains with a supplied water network | 20 | 0 |
| Each distinct god | 1 small temple | 2 | 48 |
| Education 1 / 2 / 3 | School / school + library / both + academy | 10 / 30 / 60 | 0 |
| Bathhouse | 1 | 10 | 0 |
| Barber | 1 | 2 | 0 |
| Health 1 / 2 | Doctor / doctor + hospital | 5 / 35 | 0 |
| Entertainment through 10 | Theater + actor colony | 13 | 0 |
| Entertainment 11–25 | Previous + amphitheater + gladiator school | 33 | 0 |
| Entertainment 26–45 | Previous + arena + lion house | 66 | 0 |
| Entertainment 46–80 | Previous + 10% of regional hippodrome/chariot-maker pair | 82 | 86.40 |

The last entertainment bundle includes the working-hippodrome bonus. Its 10%
allocation is an explicit shared regional cost; the remaining 90% is charged
elsewhere. Charging this district the entire venue adds 144 staff and 1,209.60
annual wages/levies. Other venue mixes may cost less. No venue has been optimized
for price. Current latrine-or-fountain requirements use the allowed fountain
option here; a latrine-based arrangement could be cheaper.

Maintenance means the recurring levies actually charged by the inspected game
rules, not an invented construction depreciation or repair allowance. Food and
goods used by households are separate resource lines. A scenario-specific
service consuming additional supplies needs its own recurring line.

## Current rules: density and shared-service efficiency

All money is denarii/year. `Wages` and `Maint.` cover the specified service and
distribution bundle, **excluding household resources**. `Service/person` divides
those two costs by the actual population in the same area. `Workers left` is
potential resident workforce minus the service staff; domestic production jobs
must still be deducted, and negative elite values need workers from elsewhere.

| Tier | Residents | Potential workers | Wages | Maint. | Service/person | Workers left | Tax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Small Tent | 720 | 273.6 | 69 | 0.0 | 0.096 | 250.6 | 302 |
| Large Tent | 1,008 | 383.0 | 69 | 0.0 | 0.068 | 360.0 | 423 |
| Small Shack | 1,296 | 492.5 | 117 | 0.0 | 0.090 | 453.5 | 544 |
| Large Shack | 1,584 | 601.9 | 123 | 48.0 | 0.108 | 560.9 | 665 |
| Small Hovel | 1,872 | 711.4 | 183 | 48.0 | 0.123 | 650.4 | 1,572 |
| Large Hovel | 2,160 | 820.8 | 222 | 48.0 | 0.125 | 746.8 | 1,814 |
| Small Casa | 2,448 | 930.2 | 252 | 48.0 | 0.123 | 846.2 | 2,056 |
| Large Casa | 2,736 | 1,039.7 | 300 | 48.0 | 0.127 | 939.7 | 2,298 |
| Small Insula | 2,736 | 1,039.7 | 360 | 48.0 | 0.149 | 919.7 | 3,447 |
| Medium Insula | 2,880 | 1,094.4 | 375 | 48.0 | 0.147 | 969.4 | 3,629 |
| Large Insula | 3,024 | 1,149.1 | 441 | 48.0 | 0.162 | 1,002.1 | 3,810 |
| Grand Insula | 3,024 | 1,149.1 | 540 | 48.0 | 0.194 | 969.1 | 5,080 |
| Small Villa | 1,440 | 0.0 | 546 | 96.0 | 0.446 | -182.0 | 5,443 |
| Medium Villa | 1,512 | 0.0 | 636 | 96.0 | 0.484 | -212.0 | 6,350 |
| Large Villa | 1,440 | 0.0 | 726 | 96.0 | 0.571 | -242.0 | 6,653 |
| Grand Villa | 1,600 | 0.0 | 780 | 230.4 | 0.632 | -260.0 | 7,392 |
| Small Palace | 1,696 | 0.0 | 780 | 230.4 | 0.596 | -260.0 | 8,548 |
| Medium Palace | 1,792 | 0.0 | 786 | 278.4 | 0.594 | -262.0 | 9,032 |
| Large Palace | 1,710 | 0.0 | 786 | 278.4 | 0.622 | -262.0 | 10,773 |
| Luxury Palace | 1,800 | 0.0 | 786 | 278.4 | 0.591 | -262.0 | 12,096 |

The tent example exposes the earlier model's error. In this area, Small Tent
holds 720 residents; Large Tent holds 1,008. Both use the same 23-person safety
and tax workforce. The well adds no wages or maintenance. Population, potential
workforce, and taxes increase by **40%**, while shared service cost per resident
falls by **28.57%**. The extra 288 residents provide about **109.44 workers** and
**120.96 annual taxes** at the unchanged 1x multiplier.

Likewise, an existing requirement does not need to be bought again on every
upgrade. More residents spread its recurring cost. Only the incremental services,
consumption, and any capacity-driven extra facilities belong in the upgrade cost.

## Resource quantities before assigning a price

Each occupied merged/fixed house in this model consumes 24 units/year per `1`
goods requirement. With 36 plebeian houses that is **864 units/year per required
good**, constant as the house capacity rises. At 16 larger villas it becomes
384 units; at 9 palaces, 216. Wine requirement 2 doubles wine consumption in the
current code and also has the separate second-source requirement.

`P/O/F/W` mean pottery/oil/furniture/wine. Values below are inventory units per
year; divide by 100 for cartloads. Food is the approximate total annual ration,
split among food types, not multiplied by the number of types. Per-house integer
rounding can make realized food consumption slightly lower.

| Tier | Food units | Current P/O/F/W units | Proposed P/O/F/W units |
| --- | ---: | --- | --- |
| Small Tent | 0 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Large Tent | 0 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Small Shack | 7,776 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Large Shack | 9,504 | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| Small Hovel | 11,232 | 0 / 0 / 0 / 0 | 864 / 0 / 0 / 0 |
| Large Hovel | 12,960 | 0 / 0 / 0 / 0 | 864 / 0 / 0 / 0 |
| Small Casa | 14,688 | 0 / 0 / 0 / 0 | 864 / 864 / 0 / 0 |
| Large Casa | 16,416 | 864 / 0 / 0 / 0 | 864 / 864 / 0 / 0 |
| Small Insula | 16,416 | 864 / 0 / 0 / 0 | 864 / 864 / 0 / 0 |
| Medium Insula | 17,280 | 864 / 0 / 864 / 0 | 864 / 864 / 864 / 0 |
| Large Insula | 18,144 | 864 / 864 / 864 / 0 | 864 / 864 / 864 / 0 |
| Grand Insula | 18,144 | 864 / 864 / 864 / 0 | 864 / 864 / 864 / 0 |
| Small Villa | 8,640 | 864 / 864 / 864 / 864 | 864 / 864 / 864 / 864 |
| Medium Villa | 9,072 | 864 / 864 / 864 / 864 | 864 / 864 / 864 / 864 |
| Large Villa | 8,640 | 384 / 384 / 384 / 384 | 384 / 384 / 384 / 384 |
| Grand Villa | 9,600 | 384 / 384 / 384 / 384 | 384 / 384 / 384 / 384 |
| Small Palace | 10,176 | 384 / 384 / 384 / 768 | 384 / 384 / 384 / 768 |
| Medium Palace | 10,752 | 384 / 384 / 384 / 768 | 384 / 384 / 384 / 768 |
| Large Palace | 10,260 | 216 / 216 / 216 / 432 | 216 / 216 / 216 / 432 |
| Luxury Palace | 10,800 | 216 / 216 / 216 / 432 | 216 / 216 / 216 / 432 |

For the proposal, pottery begins at Small Hovel, oil and actual fountain water
at Small Casa, school OR library at Large Casa, and two gods at Large Insula.
All higher levels retain the requirement. Through Large Hovel, water needs only
well-level access. These interpretations remain planning assumptions.

## Complete specified recurring basket: import-price sensitivity

To put a cash value on those quantities, this table uses default imported goods:
pottery/oil 180 per load, furniture 200, wine 215. One-food housing uses wheat
at 28/load; two-food uses wheat/vegetables at 28/38; three-food adds fruit at 38.
Food is split equally across the available types in this estimate. No additional
production wages are charged for imported resources.

`Total` includes household food, manufactured goods, service/distribution wages,
and maintenance levies. It excludes one-time expenses. This is a financing stress
case; **domestic production costs and the output of surplus workers must be
considered before judging housing's overall value**. The household tax column
is only one of the district's economic benefits.

| Tier | Current goods | Food | Current services | Current total | Current tax | Proposed total | Candidate tax |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Small Tent | 0 | 0 | 69 | 69 | 302 | 69 | 302 |
| Large Tent | 0 | 0 | 69 | 69 | 423 | 69 | 529 |
| Small Shack | 0 | 2,177 | 117 | 2,294 | 544 | 2,294 | 816 |
| Large Shack | 0 | 2,661 | 171 | 2,832 | 665 | 2,832 | 1,164 |
| Small Hovel | 0 | 3,145 | 231 | 3,376 | 1,572 | 4,889 | 1,572 |
| Large Hovel | 0 | 3,629 | 270 | 3,899 | 1,814 | 5,412 | 2,041 |
| Small Casa | 0 | 4,113 | 300 | 4,413 | 2,056 | 7,511 | 2,570 |
| Large Casa | 1,555 | 4,596 | 348 | 6,500 | 2,298 | 8,055 | 3,160 |
| Small Insula | 1,555 | 4,596 | 408 | 6,560 | 3,447 | 8,115 | 3,447 |
| Medium Insula | 3,283 | 4,838 | 423 | 8,545 | 3,629 | 10,100 | 4,028 |
| Large Insula | 4,838 | 5,080 | 489 | 10,408 | 3,810 | 10,462 | 4,661 |
| Grand Insula | 4,838 | 5,988 | 588 | 11,414 | 5,080 | 11,468 | 5,080 |
| Small Villa | 6,696 | 2,851 | 642 | 10,189 | 5,443 | 10,189 | 5,443 |
| Medium Villa | 6,696 | 2,994 | 732 | 10,422 | 6,350 | 10,422 | 6,350 |
| Large Villa | 2,976 | 2,851 | 822 | 6,649 | 6,653 | 6,649 | 6,653 |
| Grand Villa | 2,976 | 3,328 | 1,010 | 7,314 | 7,392 | 7,314 | 7,392 |
| Small Palace | 3,802 | 3,528 | 1,010 | 8,340 | 8,548 | 8,340 | 8,548 |
| Medium Palace | 3,802 | 3,727 | 1,064 | 8,593 | 9,032 | 8,593 | 9,032 |
| Large Palace | 2,138 | 3,557 | 1,064 | 6,760 | 10,773 | 6,760 | 10,773 |
| Luxury Palace | 2,138 | 3,744 | 1,064 | 6,947 | 12,096 | 6,947 | 12,096 |

Current and proposed totals use the same land and service-sharing assumptions.
The candidate tax curve is `1, 1.25, 1.50, 1.75, 2, 2.25, 2.50, 2.75, 3, 3.33,
3.67, 4` for plebeians. Elite multipliers are unchanged. An all-import deficit
does not imply the tier is uneconomic: its surplus workforce may operate export
industries or cheaply produce the resources being valued at import prices here.

## Evaluating an upgrade on recurring economics

For a fixed catchment, compare:

- Additional tax revenue from both increased population and any multiplier rise.
- Additional usable workers after the new service and production staffing.
- Additional resource quantities, priced at actual marginal delivered cost.
- Additional recurring service wages, levies, and throughput-driven facilities.

For plebeians, `net workers = 0.38 * residents - service staff - production staff`.
If surplus workers have productive jobs, their net output belongs in an economic
comparison. Do not assign every spare worker a guaranteed export income: raw
materials, land, demand, quotas, and delivery may bind. Conversely, do not treat
additional workers as worthless merely because they are not house taxes.

At a fixed 36-house catchment, current Small-to-Medium Insula adds 144 residents,
54.72 potential workers, and a five-worker doctor. That leaves **49.72 extra
workers before furniture production**, alongside **181.44 extra annual taxes**.
It also adds 864 annual furniture units. The right question is whether the actual
furniture chain and extra food justify those benefits, not whether equal-sized
populations pay more taxes. With the candidate 3.33x multiplier, the tax increase
becomes **580.61/year**. Existing pottery still consumes 864 units in either tier.

Small Casa to Large Casa adds 288 residents to the same catchment. Under current
rules, adding a warehouse and bathhouse consumes 16 extra workers, leaving about
93.44 extra potential workers before pottery/food production. Under the proposed
rules the warehouse and goods are already needed; education and baths add 20
workers, leaving about 89.44 before the incremental food supply. These staffing
comparisons assume the existing market and walker routes can handle the load.

For a domestic supply chain, measure delivered units per staffed building and
charge its wages, maintenance, raw materials, and transport. Allocate shared
capacity where it exists and add a whole extra building only when needed. Import
prices remain useful for import-dependent maps and opportunity-cost comparisons,
but are not a substitute for domestic recurring cost. Do not double-count wages
inside a delivered resource cost and then subtract the same payroll again.

Fractional taxes can smooth the reward across the sequence. They should be tuned
against **incremental recurring burden after density gains**, rather than a
fixed tax amount per 1,000 people or equal multiplier intervals alone.

## Fractional tax support: proposed implementation contract

No implementation is authorized by this planning note. If chosen later:

1. Parse decimal multipliers to an exact fixed-point representation, for example
   hundredths (`3.33` becomes 333), with clear range/precision validation. Runtime
   arithmetic should use a wide integer numerator, not repeatedly rounded floats.
2. Preserve precision through population, difficulty, tax percentage, and
   coverage. Do not call the current integer `difficulty_adjust_money` on the
   multiplier first. At 1,000 covered residents, Hard, 7%, and 3.33x, collection
   should average 116.55/month or 1,398.60/year before final money rounding.
3. Carry fractional revenue across months or use another explicit final-rounding
   rule. Rounding each house independently makes house merging change taxes for
   reasons unrelated to population or wealth. Aggregate first, and document any
   separation between plebeian/patrician totals.
4. Audit estimated taxes, actual treasury receipts, per-house tax reporting,
   overlays/advisors, UI formatting, save-state remainder fields, and reload
   behavior together. Displayed estimates and money collected must agree.
5. Keep Julius/Augustus compatibility intentional. Simply retaining integer XML
   values does **not** preserve old economics if difficulty rounding changes.
   For example at Normal, the old 3x multiplier becomes `floor(3 * 1.5) = 4`;
   precise math would make it 4.5. A Vespasian-only precision mode can preserve
   legacy collection rules for the compatibility profiles.
6. Later targeted tests should cover fractional parsing, difficulty scaling,
   aggregate/house split invariance, month-to-year totals, and remainder save/load,
   followed by the repository's required compatibility/startup validation.

The balancing objective should be **small, visible returns at every tier, with
larger returns where new recurring costs arrive**. Exact straight-line spacing
is a useful first comparison; it should not prevent a larger bump at pottery,
oil, or furniture if the complete service cost justifies one.


## Reproduction and scope

The calculator has since been updated for the implemented annual-consumption comparison; the historical Current/Proposed columns below are no longer its output. [housing_cost_model.ps1](housing_cost_model.ps1) reads current local capacities,
requirements, consumption cadence, prices, and employee counts. Service-bundle
counts, 38% workforce, Hard maintenance levies, and the candidate tax curve are
explicit benchmark assumptions.

```powershell
& ./research/housing_cost_model.ps1 | Format-Table Mod, Tier, Population, ServicePerResident, Tax
& ./research/housing_cost_model.ps1 -ResidentialTiles 144 -TaxPercent 10 -Wage 32
```

Changing residential area does not auto-scale the service bundle. Review actual
coverage and throughput for that area. Non-default areas use floor division for
house count; total tile area alone does not prove footprint packing or access.
Default 144 divides evenly into each footprint. A real district validation must
include total service/road/industry area and check sustained route coverage.

Calculated tables are planning estimates, not a headless soak or a tested city.
Local sources: [food](../src/city/resource.cpp), [taxes and levies](../src/city/finance.cpp),
[service scores](../src/building/house_service.cpp), [goods cadence](../src/building/HousingDef.cpp),
[well staffing](../Mods/Julius/BuildingType/well.xml), and the XML files read by the calculator.
