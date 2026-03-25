$ErrorActionPreference = 'Stop'

$packageArgs = @{
    packageName    = 'triangles'
    unzipLocation  = "$(Split-Path -Parent $MyInvocation.MyCommand.Definition)"
    url64bit       = 'https://github.com/SamiAhmed7777/triangles_v5/releases/download/v5.3.7/Cryptographic-Triangles-5.3.7-win-x64.zip'
    checksum64     = '6f002a669a7e92aaf3d8dd7b1ae80f06a086c99a15ca05cf107665009ffc06b7'
    checksumType64 = 'sha256'
}

Install-ChocolateyZipPackage @packageArgs

$installDir = $packageArgs.unzipLocation
$desktopPath = [Environment]::GetFolderPath('Desktop')

Install-ChocolateyShortcut `
    -ShortcutFilePath "$desktopPath\Cryptographic Triangles.lnk" `
    -TargetPath "$installDir\triangles-qt.exe"
