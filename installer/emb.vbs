' Emb.vbs. 
'   Argument(0) is the name of the storage.
'   Argument(1) is the original database. 
'   Argument(2) is the path to the transform file. 
' This was changed from the original to work around a gyp defect involved that rewrites whole command lines rather than just path-containing variables.

Option Explicit

' Check arguments
If WScript.Arguments.Count < 2 Then
 WScript.Echo "Usage is emb.vbs [storage name] [original database] [transform]"
 WScript.Quit(1)
End If

' Connect to Windows Installer object
On Error Resume Next
Dim installer : Set installer = Nothing
Set installer = Wscript.CreateObject("WindowsInstaller.Installer")
 
' Evaluate command-line arguments and set open and update modes
Dim databasePath: databasePath = Wscript.Arguments(1)
Dim importPath  : importPath = Wscript.Arguments(2)
Dim storageName : storageName = Wscript.Arguments(0)
 
' Open database and create a view on the _Storages table
Dim sqlQuery : sqlQuery = "SELECT `Name`,`Data` FROM _Storages"
Dim database : Set database = installer.OpenDatabase(databasePath, 1)
Dim view     : Set view = database.OpenView(sqlQuery)
 
'Create and Insert the row.
Dim record   : Set record = installer.CreateRecord(2)
record.StringData(1) = storageName
view.Execute record
 
'Insert storage - copy data into stream
record.SetStream 2, importPath
view.Modify 3, record
database.Commit
Set view = Nothing
Set database = Nothing