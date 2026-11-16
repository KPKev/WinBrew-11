import winreg
for subkey in [r'Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders',r'Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders']:
    with winreg.CreateKey(winreg.HKEY_CURRENT_USER,subkey) as key:
        winreg.SetValueEx(key,'Personal',0,winreg.REG_SZ,r'Z:\data\WinBrew\Users\Default\Documents')
        winreg.SetValueEx(key,'Desktop',0,winreg.REG_SZ,r'Z:\data\WinBrew\Users\Default\Desktop')
