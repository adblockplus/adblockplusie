#include <stdexcept>

#include "process.h"

Windows_Handle::Windows_Handle( HANDLE h ) 
  : handle( h ) 
{
  if ( handle == INVALID_HANDLE_VALUE )
  {
    throw std::runtime_error( "Invalid handle" ) ;
  }
}

Windows_Handle::~Windows_Handle()
{
  CloseHandle( handle ) ;
}
