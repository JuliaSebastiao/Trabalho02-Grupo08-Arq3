param(
    [string]$Exe = ".\tomasulo.exe"
)

if (-not (Test-Path $Exe)) {
    Write-Error "Executavel nao encontrado: $Exe. Compile o projeto antes de rodar os testes."
    exit 1
}

$files = @()
$files += Get-ChildItem ".\examples\*.txt" | Sort-Object Name
$files += Get-ChildItem ".\tests\*.txt" | Sort-Object Name

$failed = 0
foreach ($file in $files) {
    Write-Host "== $($file.FullName) =="
    & $Exe $file.FullName --quiet
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL: $($file.Name)" -ForegroundColor Red
        $failed++
    } else {
        Write-Host "PASS: $($file.Name)" -ForegroundColor Green
    }
    Write-Host ""
}

if ($failed -gt 0) {
    Write-Error "$failed teste(s) falharam."
    exit 1
}

Write-Host "Todos os testes passaram." -ForegroundColor Green
