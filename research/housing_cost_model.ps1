# Annual recurring-cost benchmark. Reads authored mod stacks; does not modify game definitions.
[CmdletBinding()]
param([int]$ResidentialTiles = 144, [double]$TaxPercent = 7, [double]$Wage = 30, [double]$HippodromeShare = 0.1, [double]$ProductionEfficiency = 1, [int]$FarmFields = 5, [switch]$ProposedPatricianCapacities, [switch]$ProductionBenchmarks, [switch]$VanillaFarmOutput)
$ErrorActionPreference = 'Stop'
if ($ResidentialTiles -lt 16 -or $TaxPercent -lt 0 -or $Wage -lt 0 -or $HippodromeShare -lt 0 -or $HippodromeShare -gt 1) { throw 'Invalid benchmark parameters.' }
if ($ProductionEfficiency -le 0 -or $ProductionEfficiency -gt 1 -or $FarmFields -lt 1) { throw 'Invalid production assumptions.' }
$repoRoot = Split-Path $PSScriptRoot -Parent
$modNames = @('Julius', 'Augustus')
function Read-Attributes([string]$relativePath, [string]$xpath, [switch]$ReplaceNode) {
    $merged = @{}
    foreach ($modName in $modNames) {
        $path = Join-Path $repoRoot "Mods/$modName/$relativePath"
        if (Test-Path -LiteralPath $path) {
            [xml]$document = Get-Content -LiteralPath $path -Raw
            $node = $document.SelectSingleNode($xpath)
            if ($node) {
                if ($ReplaceNode) { $merged = @{} }
                foreach ($attribute in $node.Attributes) { $merged[$attribute.Name] = $attribute.Value }
            }
        }
    }
    return $merged
}
function Employees([string]$type) { return [double](Read-Attributes "BuildingType/$type.xml" '/building/labor/employees').count }
function Buy-Price([string]$resource) { return [double](Read-Attributes "Resources/$resource.xml" '/resource/trade').buy }
function Sell-Price([string]$resource) { return [double](Read-Attributes "Resources/$resource.xml" '/resource/trade').sell }
function Service-Cost($requirements) {
    # One benchmark district: these are scenario assumptions, not coverage guarantees.
    $workers = 2 * (Employees 'prefecture') + (Employees 'engineers_post') + (Employees 'forum')
    if ([int]$requirements.food_types -gt 0) { $workers += 2 * (Employees 'market') + (Employees 'granary') }
    if ([int]$requirements.food_types -gt 0 -or ((Rate $requirements.pottery) + (Rate $requirements.oil) + (Rate $requirements.furniture) + (Rate $requirements.wine)) -gt 0) { $workers += Employees 'warehouse' }
    if ($requirements.water -in @('fountain', 'latrine_or_fountain')) { $workers += 5 * (Employees 'fountain') }
    $gods = [int]$requirements.religion
    $workers += $gods * (Employees 'small_temple_ceres')
    $levies = 12 * 4 * $gods # Hard difficulty, small-temple levy.
    if ([int]$requirements.education -ge 1) { $workers += Employees 'school' }
    if ([int]$requirements.education -ge 2) { $workers += Employees 'library' }
    if ([int]$requirements.education -ge 3) { $workers += Employees 'academy' }
    if ([int]$requirements.bathhouse -ge 1) { $workers += Employees 'bathhouse' }
    if ([int]$requirements.barber -ge 1) { $workers += Employees 'barber' }
    if ([int]$requirements.health -ge 1) { $workers += Employees 'doctor' }
    if ([int]$requirements.health -ge 2) { $workers += Employees 'hospital' }
    $ent = [int]$requirements.entertainment
    if ($ent -gt 0) { $workers += (Employees 'theater') + (Employees 'actor_colony') }
    if ($ent -gt 10) { $workers += (Employees 'amphitheater') + (Employees 'gladiator_school') }
    if ($ent -gt 25) { $workers += (Employees 'arena') + (Employees 'lion_house') }
    if ($ent -gt 45) {
        $workers += $HippodromeShare * ((Employees 'hippodrome') + (Employees 'chariot_maker'))
        $levies += $HippodromeShare * 12 * 72
    }
    return @{ Workers = $workers; Levies = $levies; Wages = $workers * $Wage / 10; Cost = $workers * $Wage / 10 + $levies }
}
function Rate([string]$value) {
    if (!$value) { return 0.0 }
    $parts = $value.Split('/')
    if ($parts.Count -eq 2) { return [double]$parts[0] / [double]$parts[1] }
    return [double]$value
}
function Calendar-Ticks-Per-Year {
    $calendar = Read-Attributes 'defines.xml' '/defines/calendar[@id="default"]'
    $months = Read-Attributes 'defines.xml' '/defines/calendar[@id="default"]/month_days'
    $days = ($months.values.Split(',') | ForEach-Object { [int]$_ } | Measure-Object -Sum).Sum
    return $days * [int]$calendar.ticks_per_day
}
function Production-Spec([string]$resource) {
    $types = @{ wheat='wheat_farm'; vegetables='vegetable_farm'; fruit='fruit_farm'; olives='olive_farm'; vines='vines_farm'; pottery='pottery_workshop'; oil='oil_workshop'; furniture='furniture_workshop'; wine='wine_workshop'; clay='clay_pit'; timber='timber_yard' }
    $type = $types[$resource]
    if (!$type) { throw "No production benchmark for $resource" }
    $isFarm = $type.EndsWith('_farm')
    $method = if ($isFarm) { "${type}_field_basic" } else { "${type}_basic" }
    $monthly = [double](Read-Attributes "ProductionMethod/$method.xml" '/production_method/output').production_per_month
    # A cart_loads override replaces its alternative value/fraction representation.
    $cart = Read-Attributes "ProductionMethod/$method.xml" '/production_method/cart_loads' -ReplaceNode
    $cartLoads = if ($cart.ContainsKey('value')) { [double]$cart.value } elseif ($cart.ContainsKey('numerator')) { [double]$cart.numerator / [double]$cart.denominator } else { 1.0 }
    # Output size scales required work once, so cycles/month = monthly / (100 * cartLoads).
    # Nominal monthly output is absolute; harvest grouping changes cadence, not throughput.
    $annual = 12 * $monthly * $ProductionEfficiency
    $workers = Employees $type
    if ($isFarm) { $workers += $FarmFields * (Employees "${type}_field") }
    if ($isFarm) { $annual *= $FarmFields }
    if ($isFarm -and $VanillaFarmOutput) {
        # Reference scenario, not a game-definition edit: whole-farm rates before the composed-field conversion.
        # Wheat 160/month (central/desert); other crops 80/month, divided across five fields exactly once.
        $wholeFarmMonthly = if ($resource -eq 'wheat') { 160.0 } else { 80.0 }
        $annual = 12 * $wholeFarmMonthly * ($FarmFields / 5.0) * $ProductionEfficiency
    }
    $input = Read-Attributes "ProductionMethod/$method.xml" '/production_method/input'
    return [pscustomobject]@{ Resource=$resource; Type=$type; Monthly=$monthly; CartLoads=$cartLoads; AnnualUnits=$annual; Workers=$workers; Input=$input }
}
function Production-Staff([string]$resource, [double]$units) {
    if ($units -le 0) { return 0.0 }
    $spec = Production-Spec $resource
    $workers = [math]::Ceiling($units / $spec.AnnualUnits - 1e-9) * $spec.Workers
    if ($spec.Input.resource) { $workers += Production-Staff $spec.Input.resource ($units * [double]$spec.Input.amount / (100 * $spec.CartLoads)) }
    return $workers
}
if ($ProductionBenchmarks) {
    foreach ($mod in @('Augustus', 'Vespasian')) {
        $modNames = if ($mod -eq 'Augustus') { @('Julius','Augustus') } else { @('Julius','Augustus','Vespasian') }
        $ticks = Calendar-Ticks-Per-Year
        foreach ($resource in @('wheat','vegetables','fruit','olives','vines','pottery','oil','furniture','wine','clay','timber')) {
            $spec = Production-Spec $resource
            [pscustomobject]@{ Mod=$mod; Resource=$resource; Workers=$spec.Workers; AnnualUnits=$spec.AnnualUnits; TicksPerYear=$ticks; LoadsPer1000Ticks=$spec.AnnualUnits * 10 / $ticks; CartLoadsPerCycle=$spec.CartLoads; ProductionBasis=$(if ($VanillaFarmOutput) { 'Vanilla farm output reference' } else { 'Authored checkout' }) }
        }
    }
    return
}
$baseProfiles = Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Mods/Julius/HousingProfile') -Filter '*.xml' | ForEach-Object {
    [xml]$document = Get-Content -LiteralPath $_.FullName -Raw
    if (!$document.housing_profile.variant_of) {
        [pscustomobject]@{ Type = [string]$document.housing_profile.type; Level = [int]$document.housing_profile.level }
    }
} | Sort-Object Level
foreach ($profile in $baseProfiles) {
    foreach ($mod in @('Augustus', 'Vespasian')) {
        $modNames = if ($mod -eq 'Augustus') { @('Julius','Augustus') } else { @('Julius','Augustus','Vespasian') }
        $type = $profile.Type
        $level = $profile.Level
        $variant = if ($level -le 9) { "${type}_2x2" } else { $type }
        $housing = Read-Attributes "BuildingType/$variant.xml" '/building/housing'
        $capacity = [int]$housing.capacity
        $houseTiles = if ($level -le 13) { 4 } elseif ($level -le 17) { 9 } else { 16 }
        # Proposal only: no BuildingType definitions are changed by this calculator.
        if ($ProposedPatricianCapacities -and $mod -eq 'Vespasian' -and $level -ge 12) {
            $capacity = $houseTiles * (22 + 2 * ($level - 12))
        }
        $houses = [math]::Floor($ResidentialTiles / $houseTiles)
        $population = $houses * $capacity
        $taxMultiplier = [double](Read-Attributes "HousingProfile/$($housing.path).xml" '/housing_profile/tax').multiplier
        $req = Read-Attributes "HousingProfile/$($housing.path).xml" '/housing_profile/requirements'
        $units = @{}
        $goods = 0.0
        $exportGoods = 0.0
        $productionWorkers = 0.0
        foreach ($resource in @('pottery','oil','furniture','wine')) {
            $units[$resource] = $population * (Rate $req[$resource])
            $goods += $units[$resource] * (Buy-Price $resource) / 100
            $exportGoods += $units[$resource] * (Sell-Price $resource) / 100
            $productionWorkers += Production-Staff $resource $units[$resource]
        }
        $foodCount = [int]$req.food_types
        $goodsProductionWorkers = $productionWorkers
        $food = 0.0
        $exportFood = 0.0
        if ($foodCount -gt 0) {
            $foodNames = @('wheat','vegetables','fruit')
            $averageFoodPrice = 0.0
            $averageExportFoodPrice = 0.0
            for ($i = 0; $i -lt $foodCount; $i++) {
                $averageFoodPrice += Buy-Price $foodNames[$i]
                $averageExportFoodPrice += Sell-Price $foodNames[$i]
                $productionWorkers += Production-Staff $foodNames[$i] (6 * $population / $foodCount)
            }
            $averageFoodPrice /= $foodCount
            # Smooth approximation: actual food rounds separately by house/type at each feeding.
            $food = 6 * $population * $averageFoodPrice / 100
            $exportFood = 6 * $population * $averageExportFoodPrice / ($foodCount * 100)
        }
        $services = Service-Cost $req
        $tax = 6 * ($TaxPercent / 100) * $population * $taxMultiplier
        $expense = $services.Cost + $food + $goods
        $exportExpense = $services.Cost + $exportFood + $exportGoods
        $domesticExpense = $services.Cost + $productionWorkers * $Wage / 10
        $potentialWorkers = if ($level -le 11) { $population * 0.38 } else { 0.0 }
        [pscustomobject]@{
            Mod = $mod; Tier = (($type -replace '^house_', '') -replace '_', ' ')
            ProductionBasis = if ($VanillaFarmOutput) { 'Vanilla farm output reference' } else { 'Authored checkout' }
            Level = $level; Capacity = $capacity; Houses = $houses; HouseTiles = $houseTiles
            ResidentialTiles = $ResidentialTiles; Population = $population; Multiplier = $taxMultiplier
            Tax = $tax; GoodsUnits = $units; Goods = $goods; Food = $food
            ExportGoods = $exportGoods; ExportFood = $exportFood
            ExportExpense = $exportExpense; ExportBalance = $tax - $exportExpense
            ServiceWorkers = $services.Workers; Wages = $services.Wages; Maintenance = $services.Levies
            Services = $services.Cost; Expense = $expense; Balance = $tax - $expense
            ServicePerResident = $services.Cost / $population
            ExpensePerResident = $expense / $population; PotentialWorkers = $potentialWorkers
            NetWorkers = $potentialWorkers - $services.Workers
            ProductionWorkers = $productionWorkers; DomesticExpense = $domesticExpense
            FoodProductionWorkers = $productionWorkers - $goodsProductionWorkers; GoodsProductionWorkers = $goodsProductionWorkers
            DomesticBalance = $tax - $domesticExpense; DomesticNetWorkers = $potentialWorkers - $services.Workers - $productionWorkers
            BreakEvenTaxPercent = $expense / (6 * $population * $taxMultiplier) * 100
        }
    }
}
