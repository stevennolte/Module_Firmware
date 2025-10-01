# PowerShell script to download and install Doxygen
Write-Host "Downloading Doxygen installer..."

# Create temp directory if it doesn't exist
$tempDir = "$env:TEMP\doxygen_install"
if (!(Test-Path $tempDir)) {
    New-Item -ItemType Directory -Path $tempDir -Force
}

# Download Doxygen installer
$doxygenUrl = "https://www.doxygen.nl/files/doxygen-1.9.8.windows.x64.bin.zip"
$zipFile = "$tempDir\doxygen.zip"
$extractDir = "$tempDir\doxygen"

try {
    Invoke-WebRequest -Uri $doxygenUrl -OutFile $zipFile
    Write-Host "Download completed. Extracting..."
    
    # Extract the zip file
    Expand-Archive -Path $zipFile -DestinationPath $extractDir -Force
    
    # Find the doxygen.exe file
    $doxygenExe = Get-ChildItem -Path $extractDir -Name "doxygen.exe" -Recurse | Select-Object -First 1
    
    if ($doxygenExe) {
        $doxygenPath = Join-Path $extractDir $doxygenExe
        Write-Host "Doxygen extracted to: $doxygenPath"
        
        # Copy to a permanent location
        $installDir = "C:\Tools\Doxygen"
        if (!(Test-Path $installDir)) {
            New-Item -ItemType Directory -Path $installDir -Force
        }
        
        Copy-Item -Path (Split-Path $doxygenPath) -Destination $installDir -Recurse -Force
        
        # Add to PATH for current session
        $env:PATH += ";$installDir"
        
        Write-Host "Doxygen installed successfully!"
        Write-Host "Location: $installDir"
        Write-Host "Testing installation..."
        
        & "$installDir\doxygen.exe" --version
    } else {
        Write-Error "Could not find doxygen.exe in the extracted files"
    }
} catch {
    Write-Error "Failed to download or install Doxygen: $_"
}

# Clean up temp files
Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
