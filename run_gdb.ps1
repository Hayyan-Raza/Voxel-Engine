$env:PATH = "C:\msys64\ucrt64\bin;C:\msys64\usr\bin;" + $env:PATH
make CXXFLAGS="-g -O0"
if ($LASTEXITCODE -eq 0) {
    Set-Content -Path gdb_cmds.txt -Value "run
bt
quit"
    gdb -batch -x gdb_cmds.txt .\TearDownClone_debug.exe > gdb_out.txt 2>&1
    Get-Content gdb_out.txt
} else {
    Write-Host "Make failed"
}
