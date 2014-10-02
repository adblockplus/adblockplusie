/**
 * \file custom-i18n.h
 */

#ifndef CUSTOM_I18N_H
#define CUSTOM_I18N_H

#include <database.h>

//-------------------------------------------------------
// Message box text
//-------------------------------------------------------
/**
 * Accessor to localizable content for custom actions.
 *
 * This class requires that the MSI contain a custom table named "AbpUIText" in the MSI database.
 * The WiX definition of that table is in the file "custom_i18n.wxi".
 * Each custom action has the responsibility for defining its own rows within this table.
 */
class CustomMessageText
{
  Database & db ;
  const std::wstring component ;

public:
  CustomMessageText( Database & db, const std::wstring component )
    : db( db ), component( component )
  {}

  std::wstring Text( const std::wstring id )
  {
    try {
      View v( db, L"SELECT `content` FROM `AbpUIText` WHERE `component`=? and `id`=?" ) ;
      Record arg( 2 ) ;
      arg.assign_string( 1, component ) ;
      arg.assign_string( 2, id.c_str() ) ;
      Record r( v.first( arg ) ) ;
      return r.value_string( 1 ) ;
    }
    catch( ... )
    {
      return L" " ;
    }
  }
} ;

#endif
