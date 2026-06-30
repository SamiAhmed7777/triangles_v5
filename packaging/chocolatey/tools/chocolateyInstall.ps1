$ErrorActionPreference = 'Stop'

$packageArgs = @{
    packageName    = $env:ChocolateyPackageName
    fileType       = 'exe'
    softwareName   = 'Cryptographic Triangles*'
    url64bit       = "https://github.com/SamiAhmed7777/triangles_v5/releases/download/v$env:ChocolateyPackageVersion/Cryptographic-Triangles-$env:ChocolateyPackageVersion-win-x64-setup.exe"
    checksum64     = '__CHECKSUM_PLACEHOLDER__'
    checksumType64 = 'sha256'
    silentArgs     = '/S'
    validExitCodes = @(0, 3010, 1641)
}

Install-ChocolateyPackage @packageArgs
