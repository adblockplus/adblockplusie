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
class InstallerMessageBox
  : public Message
{
public:
  enum class Box : long
  {
    defaultBox = 0,
    error = INSTALLMESSAGE::INSTALLMESSAGE_ERROR,
    warning = INSTALLMESSAGE::INSTALLMESSAGE_WARNING,
    user = INSTALLMESSAGE::INSTALLMESSAGE_USER
  } ;

  enum class ButtonSet : long
  {
    defaultButtonSet = 0,
    ok = MB_OK, 
    okCancel = MB_OKCANCEL, 
    abortRetryIgnore = MB_ABORTRETRYIGNORE, 
    yesNoCancel = MB_YESNOCANCEL, 
    yesNo = MB_YESNO, 
    retryCancel = MB_RETRYCANCEL
  } ;

  enum class DefaultButton : long
  {
    defaultButton = 0, ///< use the default button
    one = MB_DEFBUTTON1,
    two = MB_DEFBUTTON2,
    three = MB_DEFBUTTON3
  } ;

  enum class Icon : long
  {
    defaultIcon = 0,                        ///< use the default icon associated with the box type
    warningIcon = MB_ICONWARNING,           ///< exclamation point
    informationIcon = MB_ICONINFORMATION,   ///< lowercase letter "i" in a circle
    errorIcon = MB_ICONERROR                ///< stop sign
  } ;

  /**
   * Ordinary constructor, wide string
   */
  InstallerMessageBox(
    std::wstring message, 
    Box box = Box::user, 
    ButtonSet buttonset = ButtonSet::defaultButtonSet, 
    DefaultButton defaultButton = DefaultButton::defaultButton,
    Icon icon = Icon::defaultIcon
    ) ;

  /**
   * Ordinary constructor, regular string
   */
  InstallerMessageBox(
    std::string message, 
    Box box = Box::user, 
    ButtonSet buttonset = ButtonSet::defaultButtonSet, 
    DefaultButton defaultButton = DefaultButton::defaultButton,
    Icon icon = Icon::defaultIcon
    ) ;
} ;

/**
 * Error for any non-handled return value from Session.WriteMessage().
 */
struct UnexpectedReturnValueFromMessageBox
  : std::logic_error
{
  UnexpectedReturnValueFromMessageBox()
    : std::logic_error( "Unexpected return value from message box." )
  {}
} ;

#endif
