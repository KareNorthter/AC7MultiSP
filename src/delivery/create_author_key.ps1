# 仅在作者明确授权密钥设置时运行；不编译、不安装、不上传。
param([Parameter(Mandatory = $true)][string]$BackupDirectory)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (-not $IsWindows -or $PSVersionTable.PSVersion.Major -lt 7) { throw '密钥设置需要Windows及PowerShell 7。' }
if ($null -eq [Security.Cryptography.X509Certificates.X509Certificate2].GetMethod('ExportPkcs12',
        [Type[]]@([Security.Cryptography.PbeParameters], [string]))) {
    throw '当前.NET没有所需的加密PFX导出接口，尚未生成密钥；请报告环境，不自动安装。'
}
$taskBackup = [IO.Path]::GetFullPath($BackupDirectory).TrimEnd([IO.Path]::DirectorySeparatorChar)
if (Test-Path -LiteralPath $taskBackup) { throw '密钥备份目录已存在，拒绝覆盖或自动重试。' }
$taskParent = [IO.Path]::GetDirectoryName($taskBackup)
if (-not (Test-Path -LiteralPath $taskParent -PathType Container)) { throw '请先准备仓库外的备份父目录。' }
for ($taskProbe = $taskParent; $taskProbe; $taskProbe = [IO.Path]::GetDirectoryName($taskProbe)) {
    if (([IO.File]::GetAttributes($taskProbe) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw '备份目录上级不能是链接。' }
}
$taskTrees = & git -C (Join-Path $PSScriptRoot '../..') worktree list --porcelain
if ($LASTEXITCODE -ne 0) { throw '无法核对工作树范围，尚未创建密钥。' }
foreach ($taskLine in $taskTrees) {
    if ($taskLine.StartsWith('worktree ')) {
        $taskRoot = [IO.Path]::GetFullPath($taskLine.Substring(9)).TrimEnd([IO.Path]::DirectorySeparatorChar)
        if ($taskBackup.Equals($taskRoot, [StringComparison]::OrdinalIgnoreCase) -or
            $taskBackup.StartsWith($taskRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw '私钥恢复材料不能放入任何项目工作树。'
        }
    }
}
$taskSid = [Security.Principal.WindowsIdentity]::GetCurrent().User
$taskAcl = [Security.AccessControl.DirectorySecurity]::new()
$taskAcl.SetAccessRuleProtection($true, $false)
$taskInheritance = [Security.AccessControl.InheritanceFlags]::ContainerInherit -bor [Security.AccessControl.InheritanceFlags]::ObjectInherit
foreach ($taskIdentity in @($taskSid, [Security.Principal.SecurityIdentifier]::new('S-1-5-18'))) {
    $taskAcl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($taskIdentity,
        [Security.AccessControl.FileSystemRights]::FullControl, $taskInheritance,
        [Security.AccessControl.PropagationFlags]::None, [Security.AccessControl.AccessControlType]::Allow))
}
[IO.FileSystemAclExtensions]::Create([IO.DirectoryInfo]::new($taskBackup), $taskAcl)
$taskKeyName = 'AC7MultiSP-Author-' + [Guid]::NewGuid().ToString('N')
$taskProvider = [Security.Cryptography.CngProvider]::MicrosoftSoftwareKeyStorageProvider
$taskParameters = [Security.Cryptography.CngKeyCreationParameters]::new()
$taskParameters.Provider = $taskProvider
$taskParameters.KeyUsage = [Security.Cryptography.CngKeyUsages]::Signing
# 恢复件要求可导出；Windows账户及外部目录ACL保护密钥，不承诺事后关闭导出。
$taskParameters.ExportPolicy = [Security.Cryptography.CngExportPolicies]::AllowExport -bor [Security.Cryptography.CngExportPolicies]::AllowPlaintextExport
$taskParameters.Parameters.Add([Security.Cryptography.CngProperty]::new('Length', [BitConverter]::GetBytes(3072), [Security.Cryptography.CngPropertyOptions]::None))
$taskKey = $null
$taskRsa = $null
$taskCertificate = $null
$taskPasswordBytes = $null
try {
    $taskKey = [Security.Cryptography.CngKey]::Create([Security.Cryptography.CngAlgorithm]::Rsa, $taskKeyName, $taskParameters)
    # 先保留非私密定位资料，后续恢复件失败也能定位该密钥；不登记到客户端公钥表。
    $taskPending = [ordered]@{schema = 1; provider = $taskProvider.Provider; key_name = $taskKeyName;
        state = 'created-recovery-pending'; private_export_disabled = $false}
    [IO.File]::WriteAllText((Join-Path $taskBackup 'local-key.json'), ($taskPending | ConvertTo-Json) + "`n", [Text.UTF8Encoding]::new($false))
    $taskRsa = [Security.Cryptography.RSACng]::new($taskKey)
    $taskPasswordBytes = [Security.Cryptography.RandomNumberGenerator]::GetBytes(48)
    $taskPassword = [Convert]::ToBase64String($taskPasswordBytes)
    $taskPbe = [Security.Cryptography.PbeParameters]::new([Security.Cryptography.PbeEncryptionAlgorithm]::Aes256Cbc,
        [Security.Cryptography.HashAlgorithmName]::SHA256, 600000)
    # 自签证书只用作加密PFX恢复容器，不安装证书、不冒充CA代码签名。
    $taskRequest = [Security.Cryptography.X509Certificates.CertificateRequest]::new('CN=AC7MultiSP local key recovery',
        $taskRsa, [Security.Cryptography.HashAlgorithmName]::SHA256, [Security.Cryptography.RSASignaturePadding]::Pss)
    $taskCertificate = $taskRequest.CreateSelfSigned([DateTimeOffset]::UtcNow.AddMinutes(-5), [DateTimeOffset]::UtcNow.AddYears(10))
    $taskEncrypted = $taskCertificate.ExportPkcs12($taskPbe, $taskPassword)
    [IO.File]::WriteAllBytes((Join-Path $taskBackup 'author-recovery.pfx'), $taskEncrypted)
    $taskProtectedPassword = [Security.Cryptography.ProtectedData]::Protect([Text.Encoding]::UTF8.GetBytes($taskPassword),
        $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
    [IO.File]::WriteAllBytes((Join-Path $taskBackup 'recovery-password.dpapi'), $taskProtectedPassword)
    $taskPublic = $taskKey.Export([Security.Cryptography.CngKeyBlobFormat]::new('RSAPUBLICBLOB'))
    $taskFingerprint = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($taskPublic)).ToLowerInvariant()
    $taskPublicRegistry = [ordered]@{schema = 1; algorithm = 'RSA-PSS-SHA256'; keys = @(
        [ordered]@{key_id = $taskFingerprint; public_blob = [Convert]::ToBase64String($taskPublic)})}
    [IO.File]::WriteAllText((Join-Path $taskBackup 'author.public-key.json'), ($taskPublicRegistry | ConvertTo-Json -Depth 5) + "`n", [Text.UTF8Encoding]::new($false))
    $taskLocal = [ordered]@{schema = 1; provider = $taskProvider.Provider; key_name = $taskKeyName;
        key_id = $taskFingerprint; private_export_disabled = $false; recovery_password_scope = 'Windows CurrentUser DPAPI'}
    [IO.File]::WriteAllText((Join-Path $taskBackup 'local-key.json'), ($taskLocal | ConvertTo-Json) + "`n", [Text.UTF8Encoding]::new($false))
    Write-Output ('作者公钥指纹：' + $taskFingerprint)
    Write-Output ('仓库外加密恢复材料：' + $taskBackup)
    Write-Output '私钥保存在当前Windows账户的CNG密钥存储；本机备份不是异机或离线恢复保证。'
} finally {
    $taskPassword = $null
    if ($null -ne $taskPasswordBytes) { [Array]::Clear($taskPasswordBytes) }
    if ($null -ne $taskCertificate) { $taskCertificate.Dispose() }
    if ($null -ne $taskRsa) { $taskRsa.Dispose() }
    if ($null -ne $taskKey) { $taskKey.Dispose() }
}
