param(
    [string]$Test,
    [string]$Exe = ".\tomasulo.exe",
    [switch]$Step,
    [switch]$Quiet,
    [switch]$List
)

function Get-Description($Path) {
    $firstComment = Get-Content $Path | Where-Object { $_.Trim().StartsWith("#") } | Select-Object -First 1
    if ($firstComment) {
        return $firstComment.Trim().TrimStart("#").Trim()
    }
    return ""
}

$files = @()
$files += Get-ChildItem ".\examples\*.txt" | Sort-Object Name
$files += Get-ChildItem ".\tests\*.txt" | Sort-Object Name

if ($List) {
    Write-Host "Testes disponiveis:"
    foreach ($file in $files) {
        $description = Get-Description $file.FullName
        Write-Host ("- {0,-40} {1}" -f $file.Name, $description)
    }
    exit 0
}

if (-not $Test) {
    Write-Error "Informe um teste. Use -List para ver as opcoes."
    exit 1
}

if (-not (Test-Path $Exe)) {
    Write-Error "Executavel nao encontrado: $Exe. Compile antes: cl /std:c++17 /EHsc /W4 /permissive- src\main.cpp /Fe:tomasulo.exe"
    exit 1
}

$matches = @()
if (Test-Path $Test) {
    $matches = @(Get-Item $Test)
} else {
    $needle = $Test.ToLowerInvariant()
    $matches = @($files | Where-Object {
        $name = $_.Name.ToLowerInvariant()
        $base = $_.BaseName.ToLowerInvariant()
        $name -eq $needle -or
        $base -eq $needle -or
        $name.StartsWith($needle) -or
        $base.StartsWith($needle) -or
        $name.Contains($needle) -or
        $base.Contains($needle)
    })

    if ($matches.Count -eq 0 -and $Test -match '^[0-9]+$') {
        $number = [int]$Test
        $prefix = "{0:D2}_" -f $number
        $matches = @($files | Where-Object { $_.Name.StartsWith($prefix) })
    }
}

if ($matches.Count -eq 0) {
    Write-Error "Nenhum teste encontrado para '$Test'. Use -List para ver as opcoes."
    exit 1
}

if ($matches.Count -gt 1) {
    Write-Host "Mais de um teste combina com '$Test':"
    foreach ($file in $matches) {
        Write-Host "- $($file.Name)"
    }
    Write-Error "Use um nome mais especifico."
    exit 1
}

$selected = $matches[0]
Write-Host "Rodando teste: $($selected.Name)" -ForegroundColor Cyan
Write-Host "Arquivo: $($selected.FullName)"

$argsList = @($selected.FullName)
if ($Step) {
    $argsList += "--step"
} elseif ($Quiet) {
    $argsList += "--quiet"
}

& $Exe @argsList
exit $LASTEXITCODE
