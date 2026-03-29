function Clear-Screen {
    Clear-Host
}

function Pad-Start {
    param([string]$s, [int]$num, [string]$ch)
    
    if ($s.Length -lt $num) {
        return ($ch * ($num - $s.Length)) + $s
    }
    return $s
}

# 旧版ADB编码函数
function Func-A {
    param([string]$s)
    
    $i = [int]$s.Substring(0, 2)
    $i2 = [int]$s.Substring(2, 2)
    $i3 = [int]$s.Substring(4, 2)
    $i4 = [int]$s.Substring(6, 2)
    $i5 = [int]$s.Substring(8, 2)
    
    $i6 = $i4 -bxor $i3
    $i7 = $i5 -bxor $i3
    $i8 = $i3 -bxor ($i6 + $i7)
    $i9 = $i -bxor $i7
    $i10 = $i2 -bxor $i7
    
    return (Pad-Start $i9.ToString() 2 '0') + (Pad-Start $i10.ToString() 2 '0') + (Pad-Start $i6.ToString() 2 '0') + (Pad-Start $i7.ToString() 2 '0') + (Pad-Start $i8.ToString() 2 '0')
}

function Func-B {
    param([string]$s)
    
    $i = [int]$s.Substring(0, 2)
    $i2 = [int]$s.Substring(2, 2)
    $i3 = [int]$s.Substring(4, 2)
    $i4 = [int]$s.Substring(6, 2)
    $i5 = [int]$s.Substring(8, 2)
    
    $i6 = $i5 -bxor ($i3 + $i4)
    $i7 = $i4 -bxor $i6
    $i8 = $i3 -bxor $i6
    $i9 = $i -bxor $i6
    $i10 = $i2 -bxor $i6
    
    return (Pad-Start $i9.ToString() 2 '0') + (Pad-Start $i10.ToString() 2 '0') + (Pad-Start $i8.ToString() 2 '0') + (Pad-Start $i7.ToString() 2 '0') + (Pad-Start $i6.ToString() 2 '0')
}

# 旧版自检编码函数
function Func-C {
    param([string]$s)
    
    $i = [int]$s.Substring(0, 2)
    $i2 = [int]$s.Substring(2, 2)
    $i3 = [int]$s.Substring(4, 6)
    
    $i5 = $i3 -bxor ($i + $i2)
    $i6 = $i -bxor $i5
    $i4 = $i2 -bxor $i5
    
    return (Pad-Start $i6.ToString() 2 '0') + (Pad-Start $i4.ToString() 2 '0') + (Pad-Start $i5.ToString() 6 '0')
}

function Func-D {
    param([string]$s)
    
    $i = [int]$s.Substring(0, 2)
    $i2 = [int]$s.Substring(2, 2)
    $i3 = [int]$s.Substring(4, 6)
    
    $i5 = $i2 -bxor $i
    $i6 = $i3 -bxor $i
    $i4 = $i -bxor ($i5 + $i6)
    
    return (Pad-Start $i5.ToString() 2 '0') + (Pad-Start $i6.ToString() 6 '0') + (Pad-Start $i4.ToString() 6 '0')
}

# 新版编码类
Add-Type @"
using System;
public class NewCode {
    public static bool IsNumeric(string s) {
        foreach (char c in s) {
            if (!Char.IsDigit(c)) return false;
        }
        return true;
    }
    
    public static string Encode(string s, int typeCode) {
        if (string.IsNullOrEmpty(s) || !IsNumeric(s) || s.Length != 7 || (typeCode != 1 && typeCode != 2)) {
            return "";
        }
        
        Random rand = new Random();
        int keyId = rand.Next(0, 7);
        int[] intArr = new int[7];
        for (int i = 0; i < 7; i++) {
            intArr[i] = int.Parse(s[i].ToString());
        }
        int key = intArr[keyId];
        
        string result = "";
        for (int i = 0; i < 7; i++) {
            int curKey = (i == keyId) ? keyId : key;
            int curInt = intArr[i];
            result += ((curInt + curKey) % 10).ToString();
        }
        
        result += (typeCode ^ keyId).ToString();
        return result;
    }
    
    public static string Decode(string s, int typeCode) {
        if (string.IsNullOrEmpty(s) || !IsNumeric(s) || s.Length != 8 || (typeCode != 1 && typeCode != 2)) {
            return "";
        }
        
        int[] intArr = new int[8];
        for (int i = 0; i < 8; i++) {
            intArr[i] = int.Parse(s[i].ToString());
        }
        int key = intArr[7] ^ typeCode;
        int v7 = (intArr[key] - key + 10) % 10;
        
        string result = "";
        for (int i = 0; i < 7; i++) {
            if (i == key) {
                result += v7.ToString();
                continue;
            }
            int curInt = intArr[i];
            result += ((curInt + 10 - v7) % 10).ToString();
        }
        
        return result;
    }
}
"@

function Print-CenteredTitle {
    param([string]$text, [string]$fillChar = '=')
    
    $width = 80
    try { $width = $Host.UI.RawUI.WindowSize.Width } catch { }
    
    $lineLength = [math]::Floor(($width - $text.Length - 4) / 2)
    $leftLine = $fillChar * $lineLength
    $rightLine = $fillChar * ($width - $leftLine.Length - $text.Length - 4)
    
    Write-Host ($leftLine + " $text " + $rightLine)
}

function Process-AdbMode {
    Clear-Screen
    cmd /c "echo ADB模式"
    
    while ($true) {
        $inputStr = Read-Host "请输入设备码"
        
        if ($inputStr.ToLower() -eq 'a') {
            return
        }
        
        if (-not ($inputStr -match '^\d+$')) {
            Write-Host "输入错误！请输入纯数字" -ForegroundColor Red
            continue
        }
        
        $length = $inputStr.Length
        
        if ($length -eq 10) {
            Write-Host "`n检测到10位数字 - 使用旧ADB逻辑" -ForegroundColor Yellow
            Write-Host "`n结果:" -ForegroundColor Green
            $result = Func-A (Func-B $inputStr)
            Write-Host $result -ForegroundColor Green
            break
        } elseif ($length -eq 8) {
            Write-Host "`n检测到8位数字 - 使用新ADB逻辑" -ForegroundColor Yellow
            Write-Host "`n结果:" -ForegroundColor Green
            
            # 新版编码逻辑 - 如果结果与输入相同，则重新计算
            $maxAttempts = 10  # 最大尝试次数
            $attempt = 0
            do {
                $decoded = [NewCode]::Decode($inputStr, 2)
                $result = [NewCode]::Encode($decoded, 2)
                $attempt++
                
                # 如果结果与输入相同，显示提示并继续尝试
                if ($result -eq $inputStr -and $attempt -lt $maxAttempts) {
                    Write-Host "计算结果与输入相同，正在重新计算... (尝试 $attempt/$maxAttempts)" -ForegroundColor Yellow
                }
            } while ($result -eq $inputStr -and $attempt -lt $maxAttempts)
            
            if ($result) {
                if ($result -eq $inputStr) {
                    Write-Host "经过 $maxAttempts 次尝试，结果仍与输入相同: $result" -ForegroundColor Yellow
                } else {
                    Write-Host $result -ForegroundColor Green
                }
            } else {
                Write-Host "编码失败（输入非法）" -ForegroundColor Red
            }
            break
        } else {
            Write-Host "输入错误！检测到$length 位数字，请输入10位(旧逻辑)或8位(新逻辑)数字" -ForegroundColor Red
            continue
        }
    }
    
    cmd /c "pause"
}

function Process-SelfTestMode {
    Clear-Screen
    cmd /c "echo 自检模式"
    
    while ($true) {
        $inputStr = Read-Host "请输入设备码"
        
        if ($inputStr.ToLower() -eq 'a') {
            return
        }
        
        if (-not ($inputStr -match '^\d+$')) {
            Write-Host "输入错误！请输入纯数字" -ForegroundColor Red
            continue
        }
        
        $length = $inputStr.Length
        
        if ($length -eq 10) {
            Write-Host "`n检测到10位数字 - 使用旧自检逻辑" -ForegroundColor Yellow
            $r1 = Func-C $inputStr
            Write-Host "`n结果:" -ForegroundColor Green
            $result = Func-D $r1
            Write-Host $result -ForegroundColor Green
            break
        } elseif ($length -eq 8) {
            Write-Host "`n检测到8位数字 - 使用新自检逻辑" -ForegroundColor Yellow
            Write-Host "`n结果:" -ForegroundColor Green
            
            # 新版编码逻辑 - 如果结果与输入相同，则重新计算
            $maxAttempts = 10  # 最大尝试次数
            $attempt = 0
            do {
                $decoded = [NewCode]::Decode($inputStr, 1)
                $result = [NewCode]::Encode($decoded, 1)
                $attempt++
                
                # 如果结果与输入相同，显示提示并继续尝试
                if ($result -eq $inputStr -and $attempt -lt $maxAttempts) {
                    Write-Host "计算结果与输入相同，正在重新计算... (尝试 $attempt/$maxAttempts)" -ForegroundColor Yellow
                }
            } while ($result -eq $inputStr -and $attempt -lt $maxAttempts)
            
            if ($result) {
                if ($result -eq $inputStr) {
                    Write-Host "经过 $maxAttempts 次尝试，结果仍与输入相同: $result" -ForegroundColor Yellow
                } else {
                    Write-Host $result -ForegroundColor Green
                }
            } else {
                Write-Host "编码失败（输入非法）" -ForegroundColor Red
            }
            break
        } else {
            Write-Host "输入错误！检测到$length 位数字，请输入10位(旧逻辑)或8位(新逻辑)数字" -ForegroundColor Red
            continue
        }
    }
    
    cmd /c "pause"
}

function Show-MainMenu {
    Clear-Screen
    
    
    Write-Host "`n请选择模式:" -ForegroundColor Cyan
    Write-Host "A: 退出程序" -ForegroundColor Yellow
    Write-Host "1: ADB" -ForegroundColor Green
    Write-Host "2: 自检" -ForegroundColor Green
}

# 主程序
while ($true) {
    Show-MainMenu
    
    $choice = Read-Host "`n请输入选择"
    
    if ($choice.ToLower() -eq 'a') {
        exit
    }
    
    switch ($choice) {
        '1' {
            Process-AdbMode
        }
        '2' {
            Process-SelfTestMode
        }
        default {
            Write-Host "输入错误！请输入 1, 2 或 A/a" -ForegroundColor Red
            cmd /c "timeout /t 2 >nul"
        }
    }
}