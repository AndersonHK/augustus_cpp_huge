[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$rows = @(& (Join-Path $PSScriptRoot 'housing_cost_model.ps1'))
$proposalRows = @(& (Join-Path $PSScriptRoot 'housing_cost_model.ps1') -ProposedPatricianCapacities | Where-Object { $_.Mod -eq 'Vespasian' -and $_.Level -ge 11 })
$culture = [Globalization.CultureInfo]::InvariantCulture
function Number($value, [string]$format = 'N0') { return ([double]$value).ToString($format, $culture) }
function Title([string]$value) { return $culture.TextInfo.ToTitleCase($value) }
function Goods($row) { return (@('pottery','oil','furniture','wine') | ForEach-Object { Number ($row.GoodsUnits[$_] / $row.Houses) '0.###' }) -join ' / ' }
$text = [Collections.Generic.List[string]]::new()
$text.Add(@'
# Annual housing consumption and recurring budgets

Implemented housing rules, revised 2026-09-09. Vespasian now uses 0.3/0.6 rates. The patrician capacity rework below is a proposal, not an implemented change. This replaces the fixed-per-building consumption assumptions in the earlier planning tables.

## Implementation contract

The four manufactured-goods attributes under `<requirements>` now declare **annual inventory units per actual resident**. Food still uses `food_types` and its existing population-based ration calculation. One trade load contains 100 inventory units.

- Julius and Augustus use exact fractions, calibrated separately for ordinary and merged houses. Small Insula uses `pottery="12/19"`; its merged variant uses `24/76`. Full occupancy therefore consumes exactly 12 or 24 pottery per year. Five residents in an ordinary Small Insula owe 60/19 pottery per year; the fraction carries into subsequent years.
- Vespasian plebeians consume 0.3 of each demanded manufactured good per resident per year. Patricians consume 0.6 pottery, 0.6 oil, and 0.6 furniture. Their wine is **0.3 through Grand Villa, then 0.6 from Small Palace onward**. This is a 70% reduction from the previous 1/2 rates; food is unchanged.
- `wine_sources` is independent of quantity: 1 through Grand Villa, 2 for palaces. Omitting it defaults to 1 when wine is demanded, otherwise 0. Existing source-access requirements are preserved.
- The previous proposed unlock moves and fractional tax multipliers remain planning proposals. This change preserves current unlocks and tax multipliers.

Execution plan: introduce rational rate parsing; convert profiles and merged variants; charge actual residents with persistent fractional carry; scale market targets; preserve legacy scenario/save boundaries; validate shipped rates and save/render behavior; regenerate the comparison below. These changes are implemented in the working tree.

The parser accepts nonnegative integers, exact fractions, and decimals with up to six decimal places. Fractions avoid approximating 12/19 as 0.63. Existing one/two-event monthly cadence and consumption-reduction bonuses remain in force. Market stock targets use the larger requirement of the current and next tier, keeping an eight-event buffer (twelve with the Venus bonus) scaled by current residents and annual demand. Supply must still reach the market and house.

Fractional carry belongs to each house and each good. Merging adds it; splitting apportions it. Save version 212 appends four doubles to the building record; older saves initialize these fractions to zero. Invalid serialized fractions are repaired with a warning. Whole demand that cannot be supplied is not accumulated as debt, matching the previous shortage behavior. Legacy scenario model controls and imported scenario overrides retain their per-event integer contract through an adapter that converts them to annual resident rates; the XML profile contract is the new annual one.

## Benchmark and limits

Each row uses **144 residential tiles**, fully occupied: 36 merged/fixed 2x2 houses, 16 3x3 houses, or 9 4x4 houses. Roads and services occupy additional space. This compares a fixed serviced area, allowing density to reduce service expense per person. It is not a tested 12x12 block layout.

Annual denarii estimates use Hard difficulty, 7% tax, complete tax coverage, wages 30, and 38% plebeian workforce. Patricians supply no workers. Tax potential is `6 × population × multiplier × 0.07`. Payroll is `staff × 30 / 10` annually. The game rounds actual tax, food, and wages, so these are smooth budget estimates, rounded only for display; displayed columns can differ by one denarius when subtracted.

The first budget table is a **land-import-funded supply case**; the second values the same food and goods at export prices. Food averages 6 units/person/year total: wheat for one food type; equal wheat/vegetables for two; equal wheat/vegetables/fruit for three. Per-load buy prices are 28/38/38 food, 180 pottery, 180 oil, 200 furniture, and 215 wine. Corresponding sell prices are 22/30/30 food, 140 pottery, 140 oil, 150 furniture, and 160 wine. These are the authored base prices in both stacks, before scenario changes or trade bonuses. Goods quantities below are annual demand; integer consumption carries any fraction between years. Food cost approximates the existing per-house/per-food rounding.

Shared service assumptions, held fixed until a requirement adds a service:

| Requirement | Allocated service staff | Annual levy |
| --- | ---: | ---: |
| Safety and taxes: 2 prefectures, engineer, forum | 23 | 0 |
| Food distribution: 2 markets, granary | 16 | 0 |
| Warehouse, when food or manufactured goods are required | 6 | 0 |
| Wells | 0 | 0 |
| Fountain water: 5 fountains and supplied water network | 20 | 0 |
| Each god: small temple | 2 | 48 |
| Education 1 / 2 / 3: school; plus library; plus academy | 10 / 30 / 60 | 0 |
| Bathhouse / barber | 10 / 2 | 0 |
| Health 1 / 2: doctor; plus hospital | 5 / 35 | 0 |
| Entertainment >0: theater and actors | 13 | 0 |
| Entertainment >10: add amphitheater and gladiators | +20 | 0 |
| Entertainment >25: add arena and lions | +33 | 0 |
| Entertainment >45: allocate 10% of hippodrome and chariots | +16 | 86.4 |

Maintenance here means actual recurring temple/hippodrome levies. Construction, land purchase, and initial stocking are excluded. No monument bonuses, tourism, exports, military costs, festivals, or administrative salaries are allocated. This benchmark assumes shared entertainment supply and adequate service capacity; it does not prove coverage, enrollment, market throughput, trade quota availability, or reserve staffing. Extra service facilities raise the bill. Source counts are scenario assumptions, not engine-enforced ratios.

Domestic production needs a different resource bill: allocate farm/workshop/raw-material-chain wages, maintenance, transport, and throughput. **Do not subtract both the import value and the domestic production payroll for the same units.** Surplus plebeian labor can produce these goods and exports; an import-funded deficit alone does not establish that a tier is economically unviable. Patrician districts also need workers from elsewhere; this table does not allocate those workers' housing costs a second time.

## Annual manufactured goods per full house

Goods columns are **pottery / oil / furniture / wine**, in annual inventory units of demand. The first ten tiers use their merged 2x2 variant. An unmerged full Large Casa/Small Insula consumes 12 pottery in Julius/Augustus; merged consumes 24. Unmerged Medium Insula consumes 12 each pottery/furniture; merged consumes 24 each. At the new Vespasian rates, unmerged Small Insula demands 5.7 pottery/year and merged demands 22.8; fractional carry avoids truncating this to a permanently lower rate.

| Tier | Footprint tiles | Residents/house | Augustus P/O/F/W | Vespasian P/O/F/W |
| --- | ---: | ---: | --- | --- |
'@)
for ($i = 0; $i -lt $rows.Count; $i += 2) {
    $a = $rows[$i]; $v = $rows[$i + 1]
    $text.Add("| $(Title $a.Tier) | $($a.HouseTiles) | $($a.Capacity) | $(Goods $a) | $(Goods $v) |")
}
$text.Add(@'

## Annual district tax versus total recurring expenses

Same population and tax potential for both stacks. **Balance = tax − wages − levies − food − manufactured goods.** Positive values are surpluses within this benchmark; negative values need other income or a cheaper supply chain.

| Tier | Residents | Tax potential | Augustus expenses | Augustus balance | Vespasian expenses | Vespasian balance |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
'@)
for ($i = 0; $i -lt $rows.Count; $i += 2) {
    $a = $rows[$i]; $v = $rows[$i + 1]
    $text.Add("| $(Title $a.Tier) | $(Number $a.Population) | $(Number $a.Tax) | $(Number $a.Expense) | $(Number $a.Balance) | $(Number $v.Expense) | $(Number $v.Balance) |")
}
$text.Add(@'

## Annual district tax versus recurring expenses at export values

This alternative uses **sell prices for both food and manufactured goods**. Population, tax, service payroll, and levies match the import table. **Opportunity-cost balance = tax − service wages − levies − export value of consumed food and goods.** The resource value is forgone revenue, not a treasury payment.

Assume the same local production is available in both alternatives and its output could actually be sold. Production payroll is common to producing-for-housing and producing-for-export, so this comparison does not add it again. The separate domestic payroll table measures cash requirements. Adding that entire production bill to this opportunity-cost table would mix the two baselines. Extra production, transport, or export-only costs require an incremental comparison of their own.

If export quotas, buyers, or routes prevent selling the marginal output, the full sell price overstates its opportunity cost. Bonuses and scenario-specific prices also change the result. This table measures the value of resources allocated to housing under available export demand; it is not a complete city treasury budget. It credits taxes only, without monetizing plebeian labor, prosperity, or other housing benefits.

| Tier | Residents | Tax potential | Augustus expenses at export values | Augustus balance | Vespasian expenses at export values | Vespasian balance |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
'@)
for ($i = 0; $i -lt $rows.Count; $i += 2) {
    $a = $rows[$i]; $v = $rows[$i + 1]
    $text.Add("| $(Title $a.Tier) | $(Number $a.Population) | $(Number $a.Tax) | $(Number $a.ExportExpense) | $(Number $a.ExportBalance) | $(Number $v.ExportExpense) | $(Number $v.ExportBalance) |")
}
$text.Add(@'

## Expense decomposition and density

Wages, levies, and food are shared across these Augustus/Vespasian rows because service unlocks and capacity are unchanged. Service/resident includes wages plus levies. Remaining workers are the 38% plebeian workforce less service staff, **before** domestic production or other jobs. Negative values for patricians indicate workers supplied by other districts.

| Tier | Wages | Levies | Food | Augustus goods | Vespasian goods | Service/resident | Remaining workers |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
'@)
for ($i = 0; $i -lt $rows.Count; $i += 2) {
    $a = $rows[$i]; $v = $rows[$i + 1]
    $text.Add("| $(Title $a.Tier) | $(Number $a.Wages) | $(Number $a.Maintenance 'N1') | $(Number $a.Food) | $(Number $a.Goods) | $(Number $v.Goods) | $(Number $a.ServicePerResident 'N3') | $(Number $a.NetWorkers 'N1') |")
}
$text.Add([IO.File]::ReadAllText((Join-Path $PSScriptRoot '../docs/production_throughput.md')).Replace('# Production throughput and cycle size', '## Domestic production payroll scenario').Replace('## Wheat harvests', '### Wheat harvests').Replace('## Housing payroll', '### Housing payroll'))
$text.Add("`n### Annual district tax versus domestic recurring expenses`n")
$text.Add('| Tier | Augustus production staff | Augustus expenses | Augustus balance | Vespasian production staff | Vespasian expenses | Vespasian balance |')
$text.Add('| --- | ---: | ---: | ---: | ---: | ---: | ---: |')
for ($i = 0; $i -lt $rows.Count; $i += 2) {
    $a = $rows[$i]; $v = $rows[$i + 1]
    $text.Add("| $(Title $a.Tier) | $(Number $a.ProductionWorkers) | $(Number $a.DomesticExpense) | $(Number $a.DomesticBalance) | $(Number $v.ProductionWorkers) | $(Number $v.DomesticExpense) | $(Number $v.DomesticBalance) |")
}
$vLuxury = $rows | Where-Object { $_.Mod -eq 'Vespasian' -and $_.Level -eq 19 }
$aLuxury = $rows | Where-Object { $_.Mod -eq 'Augustus' -and $_.Level -eq 19 }
$text.Add("`nAt Luxury Palace, nominal Vespasian production needs $(Number $vLuxury.ProductionWorkers) workers plus $(Number $vLuxury.ServiceWorkers) service staff, all supplied from plebeian districts. Augustus needs $(Number $aLuxury.ProductionWorkers) plus $(Number $aLuxury.ServiceWorkers). Supporting workers' own consumption, industrial land, and distribution facilities still need accounting. These are capacity/payroll estimates, not guaranteed profits.")
$text.Add(@'

## Balance implications

Large Tent illustrates the density benefit: the same 69 annual service budget covers 1,008 residents instead of Small Tent's 720. Annual tax potential rises from 302 to 423 while service cost per resident falls from 0.096 to 0.068. Required wells add no wages or maintenance.

Small Insula remains a strong transition from Large Casa: both merged houses hold 76, with the same resource demand. The extra amphitheater and gladiator school cost 60/year in the district, while the tax step adds approximately 1,149/year. The resulting balance improves about 1,089/year in either stack. This implementation preserves that tax step.

Vespasian also loses the old consumption discount from merging four houses: four full unmerged Small Insulae and one full merged Small Insula now both demand 22.8 pottery/year. Augustus preserves its vanilla 48 versus 24 totals. Population and residential density are identical in this comparison; the difference is the resource rule.

At 0.3, every extra plebeian at Large/Grand Insula requires 1.68/year of imported pottery, oil, and furniture, or 1.29 at export values. Grand Insula tax potential is 1.68/person/year before food or services. This means the import-valued manufactured goods alone use the entire tax potential.

The wine exception softens villa demand: imported manufactured goods cost 4.005/person/year through Grand Villa, then 4.65 for palaces. Villa wine at 0.3 instead of 0.6 saves 0.645/person/year at import prices, or 1,032/year for the current Grand Villa district. At export prices, the saving is 0.48/person/year, or 768/year for that district.

'@)
$text.Add("Luxury Palace's Vespasian import-valued expenses are $(Number $vLuxury.Expense) against $(Number $vLuxury.Tax) tax potential, for a balance of $(Number $vLuxury.Balance). Its import-case break-even tax rate is $(Number $vLuxury.BreakEvenTaxPercent 'N2')%. At export values its expense is $(Number $vLuxury.ExportExpense) and balance +$(Number $vLuxury.ExportBalance), versus Augustus's +$(Number $aLuxury.ExportBalance). These figures use current capacities. Density can amortize shared services, but it cannot amortize a per-resident goods bill.")
$text.Add(@'

## Proposed patrician capacity rework — not implemented

Keep footprints and tax multipliers unchanged. Start Small Villa at **22 residents per tile**, just above Grand Insula's 21, then add **2 residents per tile per patrician tier**. Capacity equals density times footprint, so neither the move to 3x3 nor the move to 4x4 causes a density drop. Only the calculator's `-ProposedPatricianCapacities` switch applies this scenario; shipped BuildingType capacities are unchanged. All preceding tables use current capacities.

With the current tax multipliers, Small Villa already beats Grand Insula in gross tax: `40 × 9 = 360` versus `84 × 4 = 336`, a 7.14% advantage on the same four tiles. The tax constraint alone requires at least 38 villa residents. Strictly increasing density requires at least 85; the proposed 88 gives a simple 22/tile starting point. At 7% tax on Hard, the proposed Small Villa collects 332.64 per house/year versus Grand Insula's 141.12, a **135.7% increase**. Both the per-house and equal-area tax tests are satisfied.

| Tier | Footprint tiles | Current capacity | Current residents/tile | Proposed capacity | Proposed residents/tile | Current annual tax/house | Proposed annual tax/house |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
'@)
foreach ($proposal in $proposalRows) {
    $current = $rows | Where-Object { $_.Mod -eq 'Vespasian' -and $_.Level -eq $proposal.Level }
    $text.Add("| $(Title $proposal.Tier) | $($proposal.HouseTiles) | $($current.Capacity) | $(Number ($current.Capacity / $current.HouseTiles) 'N2') | $($proposal.Capacity) | $(Number ($proposal.Capacity / $proposal.HouseTiles) 'N2') | $(Number ($current.Tax / $current.Houses) 'N2') | $(Number ($proposal.Tax / $proposal.Houses) 'N2') |")
}
$text.Add(@'

The following applies **both the implemented 0.3/0.6 consumption rates and the proposed capacities** to the same 144 residential tiles. Augustus remains at its current capacities; its unchanged export-value comparison is above. Shared service staffing is held constant for the density experiment. More residents may require extra capacity-limited facilities or market throughput, which would raise these expenses.

| Tier | Proposed district residents | Current tax | Proposed tax | Current export-value balance | Proposed export-valued expenses | Proposed export-value balance |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
'@)
foreach ($proposal in $proposalRows) {
    $current = $rows | Where-Object { $_.Mod -eq 'Vespasian' -and $_.Level -eq $proposal.Level }
    $text.Add("| $(Title $proposal.Tier) | $(Number $proposal.Population) | $(Number $current.Tax) | $(Number $proposal.Tax) | $(Number $current.ExportBalance) | $(Number $proposal.ExportExpense) | $(Number $proposal.ExportBalance) |")
}
$text.Add(@'

Gross taxes rise at every proposed tier, but more density does not guarantee a better opportunity-cost balance. The relevant equation is `balance = population × (tax/person − resource export value/person) − shared services`. With negative per-resident margins, additional residents deepen the district's absolute opportunity-cost deficit even while service cost per resident falls.

| Tier | Annual tax/person | Food and goods export value/person | Margin/person before services |
| --- | ---: | ---: | ---: |
'@)
foreach ($proposal in $proposalRows) {
    $resourceValue = ($proposal.ExportFood + $proposal.ExportGoods) / $proposal.Population
    $taxPerPerson = $proposal.Tax / $proposal.Population
    $text.Add("| $(Title $proposal.Tier) | $(Number $taxPerPerson 'N2') | $(Number $resourceValue 'N2') | $(Number ($taxPerPerson - $resourceValue) 'N2') |")
}
$text.Add(@'

This proposal satisfies the requested density and gross-tax constraints. Keeping the existing villa tax multiplier while removing the density drop produces a much larger gross-tax step than the current progression; that should be considered alongside any later tax-multiplier rework. At the current 7% tax and full export opportunity, the first six patrician tiers still have nonpositive per-resident margins (Large Villa is exactly zero before services). Capacity alone cannot make those tiers opportunity-cost positive. Large and Luxury Palace have positive margins, so their added density improves this balance. Patricians still provide no workers, and supporting plebeian housing and industrial land remain necessary.

## Reproduce and verify

Run `./research/write_housing_consumption_report.ps1` to regenerate this report from `housing_cost_model.ps1` and the authored XML stacks. Run `./research/housing_cost_model.ps1 -ProductionBenchmarks` for resource-by-resource staff, nominal annual output, and average loads per 1,000 ticks. The calculator also accepts `-ResidentialTiles`, `-TaxPercent`, `-Wage`, `-HippodromeShare`, `-ProductionEfficiency`, `-FarmFields`, `-VanillaFarmOutput`, and `-ProposedPatricianCapacities`; changing area or population does not automatically change coverage or service staffing. Export-valued results are emitted alongside import and domestic-payroll results.

Regression coverage includes all shipped Julius/Augustus/Vespasian profiles and merged variants, exact full-year totals, partial occupancy, decimal/fraction parsing, market buffers, and fractional carry through the housing save-state bridge. Build and executable startup/save/render validation results are recorded in [validation notes](housing_consumption_validation_2026_09_09.md).

Local implementation sources: [rates](../src/building/HousingProfileDef.cpp), [consumption](../src/building/house_evolution.cpp), [distribution](../src/figure/service.cpp), [save layout](../src/building/state.cpp), [taxes/wages/levies](../src/city/finance.cpp), [food](../src/city/resource.cpp), [production progress](../src/building/production_method_runtime.cpp), [production output](../src/building/production.cpp), and [regression cases](../tools/startup_parser_test/housing_profile_registry_layering_test.cpp).
'@)
$report = Join-Path $PSScriptRoot 'housing_annual_consumption_2026_09_09.md'
[IO.File]::WriteAllText($report, ($text -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
Write-Output $report
