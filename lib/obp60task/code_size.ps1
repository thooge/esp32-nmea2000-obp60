param(
    [string]$dir = "."
)

$total = 0

Get-ChildItem -Path $dir -Recurse -File | Where-Object {
    $_.Extension -in ".c", ".cpp", ".h"
} | ForEach-Object {
    $lines = [System.Linq.Enumerable]::Count([System.IO.File]::ReadLines($_.FullName))
    Write-Output "$($_.FullName) : $lines"
    $total += $lines
}

Write-Output "-----------------------------"
Write-Output "Over all files: $total"