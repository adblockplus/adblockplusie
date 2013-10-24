/**
 * \file record.cpp Implementation of Session class.
 */

#include "record.h"
#include "msiquery.h"

//-----------------------------------------------------------------------------------------
// Record
//-----------------------------------------------------------------------------------------
Record::Record( unsigned int n_fields )
  : n_fields( n_fields )
{
  _handle = MsiCreateRecord( n_fields ) ;
  if ( ! _handle )
  {
    throw std::runtime_error( "Failed to create record" ) ;
  }
}

Record::~Record()
{
  MsiCloseHandle( _handle ) ;
}

void 
Record::assign_string( unsigned int field_index, std::wstring value )
{
  MsiRecordSetString( _handle, field_index, value.c_str() ) ;
}