import sys
import msilib
import argparse

def main(): 
	# Check arguments
	parser = argparse.ArgumentParser(description='Embed .mst transform into the MSI database.')
	parser.add_argument('storageName', metavar='storageName', help="The name of the storage (language name)")
	parser.add_argument('databasePath', metavar='databasePath', help="Path to MSI in which to embed")
	parser.add_argument('importPath', metavar='importPath', help="Path to transform file (.mst)")
	args = parser.parse_args();
	
	# Open database and create a view on the _Storages table
	sqlQuery = "SELECT `Name`,`Data` FROM _Storages"
	database = msilib.OpenDatabase(args.databasePath, 1)
	view = database.OpenView(sqlQuery)
	 
	# Create and Insert the row with 2 fields. 
	record = msilib.CreateRecord(2)
	record.SetString(1, args.storageName)
	view.Execute(record)
	 
	# Insert storage - copy data into stream
	record.SetStream(2, args.importPath)
	view.Modify(3, record)
	database.Commit()

if __name__ == "__main__":
	main()