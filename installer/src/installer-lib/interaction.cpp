/**
* \file interaction.cpp Implementations of user interaction classes.
*/

#include "interaction.h"

/*
* The two constructors are identical except for the type of argument 'message'
* They rely on overloads of the Message constructor
*/
Installer_Message_Box::Installer_Message_Box( 
  std::wstring message, 
  box_type box,
  buttonset_type buttonset, 
  default_button_type default_button,
  icon_type icon
  )
  : Message( message, INSTALLMESSAGE( box | buttonset | default_button | icon ) )
{}

Installer_Message_Box::Installer_Message_Box( 
  std::string message, 
  box_type box, 
  buttonset_type buttonset, 
  default_button_type default_button,
  icon_type icon
  )
  : Message( message, INSTALLMESSAGE( box | buttonset | default_button | icon ) )
{}
