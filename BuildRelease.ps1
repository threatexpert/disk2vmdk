<#
��;�����ɷ����档
ע�⣺��Ҫ�ѹ��̶���release��ʽ������ɺ���ִ������ű���
#>

$ErrorActionPreference = "Stop"

try{

$ts = (Get-Date).ToString("yyyyMMdd-HHmmss")
$scriptDir = split-path -parent $MyInvocation.MyCommand.Definition
$releaseDir = join-path $scriptDir ("Release" + "-$ts")

$dirs = @(
"$releaseDir",
"$releaseDir\tools",
"$releaseDir\disk2vmdk")

Write-Host "�����ļ�..."

foreach ($i in $dirs) {
   mkdir $i | Out-Null
}

Copy-Item "$scriptDir\vbox32" -Recurse -Destination "$releaseDir\disk2vmdk\"
Copy-Item "$scriptDir\vbox64" -Recurse -Destination "$releaseDir\disk2vmdk\"
Copy-Item "$scriptDir\build\Win32\Release\disk2vmdk.exe" -Destination "$releaseDir\disk2vmdk\" 
Copy-Item "$scriptDir\build\x64\Release\disk2vmdk_x64.exe" -Destination "$releaseDir\disk2vmdk\" 
Copy-Item "$scriptDir\build\Win32\Release\d2vagent.exe" -Destination "$releaseDir\disk2vmdk\" 
Copy-Item "$scriptDir\build\x64\Release\d2vagent_x64.exe" -Destination "$releaseDir\disk2vmdk\" 

Copy-Item "$scriptDir\build\Win32\Release\bootdump.exe" -Destination "$releaseDir\tools\bootdump.exe" 
Copy-Item "$scriptDir\build\x64\Release\bootdump_x64.exe" -Destination "$releaseDir\tools\bootdump_x64.exe" 
Copy-Item "$scriptDir\build\Win32\Release\vsscopy.exe" -Destination "$releaseDir\tools\vsscopy.exe" 
Copy-Item "$scriptDir\build\x64\Release\vsscopy_x64.exe" -Destination "$releaseDir\tools\vsscopy_x64.exe" 

Write-Host "���"

}
catch{
  $error[0].Exception
  $_ |select -expandproperty invocationinfo
}

Write-Host "��������˳�..."
cmd /c pause | Out-Null
