$ErrorActionPreference='Stop'
$root='E:\NEONMU\SRCMainGS\Source'
$build=Join-Path $root 'TierSystem\ReleaseCandidate'
$msbuild='C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
$jobs=@(
 @{Name='Lua52';Project='GameServer\lua52\lua52.vcxproj';Config='Release'},
 @{Name='DataServer';Project='DataServer\DataServer\DataServer.vcxproj';Config='Release_EX603'},
 @{Name='GameServer';Project='GameServer\GameServer\GameServer.vcxproj';Config='Release_EX603'},
 @{Name='GameServerCS';Project='GameServer\GameServer\GameServer.vcxproj';Config='Release_EX603CS'},
 @{Name='Main';Project='Main5.2\source\Main.vcxproj';Config='Release'},
 @{Name='Encoder';Project='Encoder\Encoder\Encoder.vcxproj';Config='PREMIUM'}
)
foreach($job in $jobs){
 $dest=Join-Path $build $job.Name
 New-Item -ItemType Directory -Path $dest -Force | Out-Null
 $log=Join-Path $dest 'build.log'
 & $msbuild (Join-Path $root $job.Project) /t:Build /m:2 /v:minimal /nologo "/p:Configuration=$($job.Config)" /p:Platform=Win32 "/p:OutDir=$dest\bin\" "/p:IntDir=$dest\obj\" /p:BuildProjectReferences=false "/p:ForceImportAfterCppTargets=$root\TierSystem\candidate_link.props" "/p:SolutionDir=$root\GameServer\" *> $log
 $code=$LASTEXITCODE
 Write-Output "$($job.Name): exit $code; $log"
 if($code -ne 0){Get-Content $log -Tail 15}
}
