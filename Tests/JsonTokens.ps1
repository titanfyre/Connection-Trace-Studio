param([Parameter(Mandatory=$true)][string]$TestExe)
$ErrorActionPreference='Stop'
$output=& $TestExe --json-endpoint-samples
if($LASTEXITCODE){throw 'Native endpoint formatter test failed.'}
$tokens=($output-join[Environment]::NewLine)|ConvertFrom-Json
$expected=@('0x0000000000000000','0x0020000000000001','0xFFFFFFFFFFFFFFFF')
if($tokens.Count-ne$expected.Count){throw 'Unexpected JSON endpoint sample count.'}
for($i=0;$i-lt$expected.Count;$i++){
 if($tokens[$i]-isnot[string]-or$tokens[$i]-cne$expected[$i]){throw 'JSON changed a 64-bit opaque endpoint token.'}
}
Write-Output 'ConvertFrom-Json preserves exact endpoint strings for 0, 2^53+1 and UINT64_MAX. No driver was opened.'
