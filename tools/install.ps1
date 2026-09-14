# Copy ChitChat.aex into every installed After Effects Plug-ins\Effects folder.
# Run from an elevated PowerShell prompt after a successful build.

param(
	[Parameter(Mandatory = $true)]
	[string]$AexPath
)

if (-not (Test-Path $AexPath)) {
	Write-Error "File not found: $AexPath"
	exit 1
}

$adobe = "C:\Program Files\Adobe"
if (-not (Test-Path $adobe)) {
	Write-Error "Adobe folder not found."
	exit 1
}

$copied = 0
Get-ChildItem $adobe -Directory | Where-Object { $_.Name -like "Adobe After Effects *" } | ForEach-Object {
	$dest = Join-Path $_.FullName "Support Files\Plug-ins\Effects"
	if (Test-Path $dest) {
		Copy-Item -LiteralPath $AexPath -Destination (Join-Path $dest "ChitChat.aex") -Force
		Write-Host "Installed to $dest"
		$copied++
	}
}

if ($copied -eq 0) {
	Write-Error "No After Effects Effects folders found."
	exit 1
}

Write-Host "Done. Restart After Effects."
