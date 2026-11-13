Set WshShell = CreateObject("WScript.Shell")
strDir = CreateObject("Scripting.FileSystemObject").GetParentFolderName(WScript.ScriptFullName)
WshShell.CurrentDirectory = strDir

' Check if node is available or run bundled runner
strCmd = "node dist\index.js"
WshShell.Run strCmd, 0, False
