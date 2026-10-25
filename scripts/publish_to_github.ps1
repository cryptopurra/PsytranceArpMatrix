# Crea il repository su GitHub e fa il primo push (Windows PowerShell).
# Requisiti: git + GitHub CLI (winget install GitHub.cli) autenticata con `gh auth login`.
#
#   .\scripts\publish_to_github.ps1 [-Repo PsytranceArpMatrix] [-Visibility private|public]
param(
    [string]$Repo = "PsytranceArpMatrix",
    [ValidateSet("private", "public")][string]$Visibility = "private"
)
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

if (-not (Test-Path ".git")) {
    git init -b main
    git add -A
    git commit -m "Initial commit: Psytrance Arp Matrix"
}

gh repo create $Repo "--$Visibility" --source . --remote origin --push `
    --description "Psytrance multi-channel MIDI arp / sequencer matrix (JUCE VST3/AU/Standalone)"

Write-Host ""
Write-Host "Fatto. Per pubblicare una release con i binari compilati dalla CI:"
Write-Host "  git tag v0.1.0; git push origin v0.1.0"
