$ErrorActionPreference = 'Stop'

$packageArgs = @{
    packageName    = 'triangles'
    unzipLocation  = "$(Split-Path -Parent $MyInvocation.MyCommand.Definition)"
    url64bit       = 'https://github.com/SamiAhmed7777/triangles_v5/releases/download/v5.1.4/Triangles-v5.1.4-win-x64.zip'
    checksum64     = '777e475f366164b342e917111bcf3155ec39e0ab4bd97b2ac295885ad30a93c6'
    checksumType64 = 'sha256'
}

Install-ChocolateyZipPackage @packageArgs

$installDir = $packageArgs.unzipLocation
$desktopPath = [Environment]::GetFolderPath('Desktop')

Install-ChocolateyShortcut `
    -ShortcutFilePath "$desktopPath\Cryptographic Triangles.lnk" `
    -TargetPath "$installDir\triangles-qt.exe"
