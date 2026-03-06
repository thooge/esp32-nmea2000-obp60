param([string]$dir = ".")

$total = 0

Get-ChildItem $dir -Recurse -Include *.c, *.cpp, *.h -File | ForEach-Object {
    $lines = [System.IO.File]::ReadLines($_.FullName).Count
    Write-Output "$($_.FullName) : $lines"
    $total += $lines
}

Write-Output "-----------------------"
Write-Output "Over all files: $total"