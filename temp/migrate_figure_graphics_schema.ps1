$ErrorActionPreference = 'Stop'

function Read-Attributes([string] $text) {
    $result = [ordered]@{}
    foreach ($match in [regex]::Matches($text, '([A-Za-z_][A-Za-z0-9_]*)="([^"]*)"')) {
        $result[$match.Groups[1].Value] = $match.Groups[2].Value
    }
    return $result
}

function Format-Attributes($attributes, [string[]] $names) {
    $parts = @()
    foreach ($name in $names) {
        if ($attributes.Contains($name)) {
            $parts += (' {0}="{1}"' -f $name, $attributes[$name])
        }
    }
    return $parts -join ''
}

function Format-Target([string] $indent, [string] $name, [string] $path, $attributes, [string[]] $attributeNames) {
    $options = Format-Attributes $attributes $attributeNames
    return @(
        ('{0}<{1}{2}>' -f $indent, $name, $options),
        ('{0}    <path value="{1}" />' -f $indent, $path),
        ('{0}</{1}>' -f $indent, $name)
    ) -join "`r`n"
}

function Wrapper-Targets([string] $figureType, [string] $assetlist) {
    switch ($assetlist) {
        'Walkers\legacy_criminal' { return @{ default = 'Walkers\Group_115' } }
        'Walkers\trade_caravan' { return @{ default = 'Walkers\Group_130' } }
        'Environment\fish_gulls' { return @{ default = 'Environment\Group_206' } }
        'Ships\flotsam' { return @{ default = 'Ships\Group_{legacy_group}'; allow_empty = 'true' } }
        'Walkers\work_camp_slave' { return @{ default = 'Walkers\Slave_{dir_upper}_{frame}'; corpse = 'Walkers\Slave_death_{frame}'; corpse_frames = '8' } }
        'Walkers\mess_hall' { return @{ default = 'Walkers\M_Hall_{dir_upper}_{frame}'; corpse = 'Walkers\M_Hall_death_{frame}'; corpse_frames = '8' } }
        'Walkers\caravanserai_collector' { return @{ default = 'Walkers\caravanserai_walker_{dir}_{frame}'; corpse = 'Walkers\caravanserai_walker_death_{frame}'; corpse_frames = '8' } }
        'Walkers\architect' { return @{ default = 'Walkers\architect_{dir}_{frame}'; corpse = 'Walkers\architect_death_{frame}'; corpse_frames = '5'; action = 'Walkers\Architect_{frame}'; action_state = 'work_camp_architect_working'; action_frames = '12'; action_min_wait_ticks = '2' } }
        default { throw "No direct extracted-asset mapping for FigureType '$figureType' wrapper '$assetlist'." }
    }
}

$defaultOptions = @('base_image_offset', 'max_image_offset', 'payload_frame_count', 'direction_stride', 'static_frame_count', 'allow_empty')
$corpseOptions = @('base_image_offset', 'frame_count')
$actionOptions = @('state', 'frame_count', 'min_wait_ticks', 'min_missile_wait_ticks')
$cartOptions = @('mode', 'offsets_x', 'offsets_y', 'high_load_threshold', 'high_load_y_adjust', 'direction_3_y_adjust')

$files = Get-ChildItem -Path 'Mods' -Recurse -File -Filter '*.xml' | Where-Object { $_.FullName -match '\\FigureType\\' }
foreach ($file in $files) {
    $raw = Get-Content -LiteralPath $file.FullName -Raw
    $typeMatch = [regex]::Match($raw, '<figure\s+type="([^"]+)"')
    if (-not $typeMatch.Success) { continue }
    $figureType = $typeMatch.Groups[1].Value

    $graphicsMatch = [regex]::Match($raw, '(?ms)^(?<indent>[ \t]*)<graphics\b(?<attrs>[^>]*)/?>')
    if (-not $graphicsMatch.Success) { continue }
    $indent = $graphicsMatch.Groups['indent'].Value
    $attributes = Read-Attributes $graphicsMatch.Groups['attrs'].Value
    $selfClosing = $graphicsMatch.Value.TrimEnd().EndsWith('/>')
    $targets = $null
    if ($attributes.Contains('assetlist')) {
        $targets = Wrapper-Targets $figureType $attributes['assetlist']
    } elseif ($attributes.Contains('runtime_selected_source')) {
        $targets = @{ default = $attributes['runtime_selected_source'] }
    } elseif ($attributes.Contains('path_pattern')) {
        $targets = @{ default = $attributes['path_pattern'] }
        if ($attributes.Contains('corpse_path_pattern')) { $targets.corpse = $attributes['corpse_path_pattern'] }
    }

    $children = @()
    if ($targets) {
        $defaultAttributes = [ordered]@{}
        foreach ($name in $defaultOptions) {
            if ($attributes.Contains($name)) { $defaultAttributes[$name] = $attributes[$name] }
        }
        if ($targets.ContainsKey('allow_empty')) { $defaultAttributes['allow_empty'] = $targets.allow_empty }
        $children += Format-Target ($indent + '    ') 'default' $targets.default $defaultAttributes $defaultOptions

        if ($targets.ContainsKey('action')) {
            $actionAttributes = [ordered]@{
                state = $targets.action_state
                frame_count = $targets.action_frames
                min_wait_ticks = $targets.action_min_wait_ticks
            }
            $children += Format-Target ($indent + '    ') 'action' $targets.action $actionAttributes $actionOptions
        }
        if ($targets.ContainsKey('corpse')) {
            $corpseAttributes = [ordered]@{}
            if ($attributes.Contains('corpse_base_image_offset')) { $corpseAttributes['base_image_offset'] = $attributes['corpse_base_image_offset'] }
            if ($attributes.Contains('corpse_frame_count')) { $corpseAttributes['frame_count'] = $attributes['corpse_frame_count'] }
            if ($targets.ContainsKey('corpse_frames')) { $corpseAttributes['frame_count'] = $targets.corpse_frames }
            $children += Format-Target ($indent + '    ') 'corpse' $targets.corpse $corpseAttributes $corpseOptions
        }
        if ($attributes.Contains('cart_mode')) {
            $cartAttributes = [ordered]@{ mode = $attributes['cart_mode'] }
            foreach ($name in @('offsets_x', 'offsets_y', 'high_load_threshold', 'high_load_y_adjust', 'direction_3_y_adjust')) {
                $legacyName = 'cart_' + $name
                if ($attributes.Contains($legacyName)) { $cartAttributes[$name] = $attributes[$legacyName] }
            }
            $children += ('{0}    <cart{1} />' -f $indent, (Format-Attributes $cartAttributes $cartOptions))
        }
    }

    if ($children.Count) {
        $replacement = $indent + '<graphics>' + "`r`n" + ($children -join "`r`n")
        if ($selfClosing) { $replacement += "`r`n" + $indent + '</graphics>' }
        $raw = $raw.Remove($graphicsMatch.Index, $graphicsMatch.Length).Insert($graphicsMatch.Index, $replacement)
    } elseif ($attributes.Contains('max_image_offset')) {
        $raw = $raw.Remove($graphicsMatch.Index, $graphicsMatch.Length).Insert($graphicsMatch.Index, $indent + '<graphics>')
        if ($raw -match '<default\b') {
            $raw = [regex]::Replace($raw, '<default\b', ('<default max_image_offset="{0}"' -f $attributes['max_image_offset']), 1)
        } elseif ($raw -match '<directional\b') {
            $raw = [regex]::Replace($raw, '<directional\b', ('<directional max_image_offset="{0}"' -f $attributes['max_image_offset']), 1)
        } else {
            throw "FigureType '$figureType' has max_image_offset without a default or directional target."
        }
    }

    $targetPattern = '(?m)^(?<indent>[ \t]*)<(?<name>default|action|corpse)\b(?<attrs>[^>]*)/>'
    $raw = [regex]::Replace($raw, $targetPattern, {
        param($match)
        $targetAttributes = Read-Attributes $match.Groups['attrs'].Value
        if (-not $targetAttributes.Contains('path_pattern')) { return $match.Value }
        $name = $match.Groups['name'].Value
        $names = if ($name -eq 'default') { $defaultOptions } elseif ($name -eq 'action') { $actionOptions } else { $corpseOptions }
        return Format-Target $match.Groups['indent'].Value $name $targetAttributes['path_pattern'] $targetAttributes $names
    })

    # FigureType sources always address asset XMLs. An image selector would couple
    # the type definition to an asset's internals and is intentionally unsupported.
    $raw = [regex]::Replace($raw, '(?m)^\s*<image\s+value="[^"]*"\s*/>\r?\n?', '')
    [System.IO.File]::WriteAllText($file.FullName, $raw, [System.Text.UTF8Encoding]::new($false))
}
