/**
* \file interaction.h User interaction classes. Message boxes and translations.
*/

#ifndef INTERACTION_H
#define INTERACTION_H

#include <string>

#include "session.h"

#include <Windows.h>
#include <Msi.h>
#include <MsiQuery.h>

/**
* A modal dialog box as displayable from within a custom action.
*
* The only fully user interface element that the Windows Installer supports for use within custom actions is a small set of modal dialog boxes.
* The Windows Installer provides the call MsiProcessMessage, overloaded by a set of message type constants.
* This class represents those messages with user-provided messages; these ultimately call MessageBox.
*
* \sa 
*    * MSDN [MsiProcessMessage function](http://msdn.microsoft.com/en-us/library/windows/desktop/aa370354%28v=vs.85%29.aspx)
*    * MSDN [Sending Messages to Windows Installer Using MsiProcessMessage](http://msdn.microsoft.com/en-us/library/windows/desktop/aa371614%28v=vs.85%29.aspx)
*/
class Installer_Message_Box
  : public Message
{
public:
  typedef enum 
  {
    default_box = 0,
    error_box = INSTALLMESSAGE::INSTALLMESSAGE_ERROR,
    warning_box = INSTALLMESSAGE::INSTALLMESSAGE_WARNING,
    user_box = INSTALLMESSAGE::INSTALLMESSAGE_USER
  }
  box_type ;

  typedef enum
  {
    default_buttonset = 0,
    ok = MB_OK, 
    ok_cancel = MB_OKCANCEL, 
    abort_retry_ignore = MB_ABORTRETRYIGNORE, 
    yes_no_cancel = MB_YESNOCANCEL, 
    yes_no = MB_YESNO, 
    retry_cancel = MB_RETRYCANCEL
  }
  buttonset_type ;

  typedef enum
  {
    default_default_button = 0,	    ///< use the default button
    default_button_one = MB_DEFBUTTON1,
    default_button_two = MB_DEFBUTTON2,
    default_button_three = MB_DEFBUTTON3
  }
  default_button_type ;

  typedef enum
  {
    default_icon = 0,			    ///< use the default icon associated with the box_type
    warning_icon = MB_ICONWARNING,	    ///< exclamation point
    information_icon = MB_ICONINFORMATION,  ///< lowercase letter "i" in a circle
    error_icon = MB_ICONERROR		    ///< stop sign
  }
  icon_type ;

  /**
  * Ordinary constructor, wide string
  */
  Installer_Message_Box(
    std::wstring message, 
    box_type box = box_type::user_box, 
    buttonset_type buttonset = buttonset_type::default_buttonset, 
    default_button_type default_button = default_button_type::default_default_button,
    icon_type icon = icon_type::default_icon
    ) ;

  /**
  * Ordinary constructor, regular string
  */
  Installer_Message_Box(
    std::string message, 
    box_type box = box_type::user_box, 
    buttonset_type buttonset = buttonset_type::default_buttonset, 
    default_button_type default_button = default_button_type::default_default_button,
    icon_type icon = icon_type::default_icon
    ) ;
} ;

/**
* Error for any non-handled return value from Session.write_message().
*/
struct unexpected_return_value_from_message_box
  : std::logic_error
{
  unexpected_return_value_from_message_box()
    : std::logic_error( "Unexpected return value from message box." )
  {}
} ;

#endif
