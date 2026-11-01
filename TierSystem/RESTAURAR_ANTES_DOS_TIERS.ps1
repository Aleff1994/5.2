$ErrorActionPreference='Stop'
$backup='C:\MuServer1\_Backup_IndependentTiers_20261018_194631'
$running=@(Get-Process Main,DataServer,GameServer,GameServerCS,SubSala,Encoder -ErrorAction SilentlyContinue)
if($running.Count){throw 'Feche os executaveis do jogo/servidor antes de restaurar.'}
$manifest=Get-Content -LiteralPath (Join-Path $backup 'manifest.json') -Raw | ConvertFrom-Json
foreach($entry in $manifest){
 if($entry.backup -and (Test-Path -LiteralPath $entry.backup)){Copy-Item -LiteralPath $entry.backup -Destination $entry.target -Force}
}
# The new shared server sidecar had no previous file; disable it on rollback.
Add-Type -TypeDefinition 'using System.Runtime.InteropServices;public static class TierRollbackProfile{[DllImport("kernel32.dll",CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string section,string key,string value,string path);}'
foreach($key in @('MetadataTransportEnable','IndependentDropCreationEnable','ItemTierSystemEnable','ItemTierVisualEnable','AdditionalTierSystemEnable','AdditionalTierVisualEnable')){
 if(-not [TierRollbackProfile]::WritePrivateProfileString('Tier',$key,'0','C:\MuServer1\Data\Item\TierSystem.ini')){throw 'Falha ao desativar configuracao de tier.'}
}
Write-Output 'Executaveis/configuracoes anteriores restaurados. Metadados SQL preservados; inventarios nao foram reescritos.'
