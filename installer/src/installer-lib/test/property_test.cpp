#include <gtest/gtest.h>

#include "../database.h"
#include "../property.h"

TEST( Property_Test, null )
{
  /*
   * This is an extract of manual test code originally run from abp_close_ie DLL entry point.
   * This code relies on an MSI database opened for installation, which we don't need to access properties.
   * We can instead use an offline session, with the database opened outside Windows Installer.
   * That session class, though, isn't written yet.
   */
  /*
   * DISABLED. Refactor into proper tests.
   */
  if ( false )
  {
    // This variable was the argument to the entry point.
    MSIHANDLE session_handle = 0;

    // The code in the body.
    Immediate_Session session( session_handle, "abp_close_ie" ) ;
    session.log( L"Have session object" ) ;
    Installation_Database db( session ) ;
    session.log( L"Have database object" ) ;

    // Test: ensure that a property is present with its expected value. Exercises the conversion operator to String.
    session.log( L"VersionMsi = " + Property( session, L"VersionMsi" ) ) ;

    // Test: create a property dynamically from within the CA. Not sure if this can be done offline.
    Property tv( session, L"TESTVARIABLE" ) ;
    session.log( L"TESTVARIABLE = " + tv ) ;

    // Test: assign a new value to a property.
    session.log( L"Setting TESTVARIABLE to 'testvalue'" ) ;
    tv = L"testvalue" ;
    session.log( L"TESTVARIABLE = " + tv ) ;
  }
}