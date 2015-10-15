/**
 * \file interaction.cpp Implementations of user interaction classes.
 */

#include "interaction.h"

/*
 * The two constructors are identical except for the type of argument 'message'
 * They rely on overloads of the Message constructor
 */
InstallerMessageBox::InstallerMessageBox(
  std::wstring message,
  Box box,
  ButtonSet buttonSet,
  DefaultButton defaultButton,
  Icon icon
)
  : Message(message, INSTALLMESSAGE(long(box) | long(buttonSet) | long(defaultButton) | long(icon)))
{}

InstallerMessageBox::InstallerMessageBox(
  std::string message,
  Box box,
  ButtonSet buttonSet,
  DefaultButton defaultButton,
  Icon icon
)
  : Message(message, INSTALLMESSAGE(long(box) | long(buttonSet) | long(defaultButton) | long(icon)))
{}
