param(
    [Parameter(Mandatory = $true)]
    [String]$CloudCompareInstallFolder,

    [Parameter()]
    [ValidateSet("En", "Fr")]
    [String]$Localization = "En"
)

$IsFolder = Test-Path -Path $CloudCompareInstallFolder -PathType Container
if ($IsFolder -eq $False) {
    throw '$CloudCompareInstallFolder does no point to a Folder'
}

$PythonDlls = Get-ChildItem -Path $CloudCompareInstallFolder -Filter "python*.dll"
if ($PythonDlls.count -gt 1) {
    throw "Too many Python DLLs, please remove the incorrect ones"
}
elseif ($PythonDlls.count -eq 0)
{
    throw "No Python DLL found"
}
else {
    $re = [Regex]::new("python(?<Suffix>[0-9]+).dll")
    $PythonDllSuffix = $re.Match($PythonDlls[0].Name).Groups['Suffix'].Value
}

$LocalizationFile = if ($Localization -eq "Fr") { "french.wxl" } else { "english.wxl" }
$LocalizationSwitch = if ($Localization -eq "Fr") { "fr-fr" } else { "en-us" }
$LocalizationName = if ($Localization -eq "Fr") { "French" } else { "English" }

$PythonEnvPrefix = Join-Path -Resolve -Path $CloudCompareInstallFolder -ChildPath "plugins" -AdditionalChildPath "Python"
$EnvTypeName = if (Test-Path -Path (Join-Path -Path $PythonEnvPrefix -ChildPath "conda-meta")) { "Conda" } else { "Venv" }

Write-Host "Python DLL suffix: $PythonDllSuffix"
Write-Host "Localization name: $LocalizationName"
Write-Host "Environment type : $EnvTypeName"
Write-Host ""


&heat `
    dir "$CloudCompareInstallFolder\plugins\Python" `
    -scom `
    -frag `
    -srd `
    -sreg `
    -gg `
    -cg PythonEnvironment `
    -dr PythonEnvironmentDir `
    -o PythonEnvironment.wxs


&candle `
    -arch x64 `
    -dSourceInstallPath="$CloudCompareInstallFolder" `
    -dPythonSuffix="$PythonDllSuffix" `
    .\Installer.wxs `
    .\PythonEnvironment.wxs `


&light `
    -ext WixUIExtension `
    .\Installer.wixobj `
    .\PythonEnvironment.wixobj `
    -b "$CloudCompareInstallFolder\plugins\Python" `
    -b "$CloudCompareInstallFolder\resources" `
    -b "$(Get-Location)" `
    -dWixUILicenseRtf="GPLv3_en.rtf" `
    -dWixUIBannerBmp="TopBanner.bmp" `
    -dWixUIDialogBmp="UiBanner.bmp" `
    -cultures:$LocalizationSwitch `
    -loc $LocalizationFile `
    -o "CloudCompare-PythonPlugin-Setup-$LocalizationName-Python$PythonDllSuffix-$EnvTypeName.msi"


# The heat command is taken from https://stackoverflow.com/questions/26550763/wix-how-to-copy-a-directory-to-install-folder
# Basically:
# it 'harvest' the "$CloudCompareInstallFolder\plugins\Python" directory
#   -scom -sfrag -srd -sreg = Suppress COM elements, fragments, root directory as element, registry harvesting (these options will create a grouping that most applications can use)
#   -gg = generate GUID's now (this will put the GUID's into your output file, rather than using a wildcard "*". The advantage here is you can use non-standard TARGETDIR, which would not qualify for autogenerated GUID's)
#   -cg PythonEnvironment : The ID we reference in the <Fearure> of the Installer.wxs
#    -dr PythonEnvironmentDir : ID of the directory (in Installer.wxs) were harvest files will be installed


# https://www.firegiant.com/wix/tutorial/user-interface/ui-wizardry/