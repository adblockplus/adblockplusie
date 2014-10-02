/**
* \file abp_ca.cpp Top-level source for custom actions. Includes DLL initialization.
*/
#include "DLL.h"
#include <stdexcept>

std::shared_ptr< DllModule > DllModule::singleton = 0 ;

DllModule & DllModule::module()
{ 
  if ( singleton )
  {
    return * singleton;
  }
  throw std::runtime_error( "DllModule::module() called when DLL module was not attached." );
}

/**
* The attachment function is the _de facto_ equivalent of initialization. Under ordinary circumstances, this should 
* only be called once, and called before everything else.
*/
void DllModule::Attach( HINSTANCE handle )
{
  if ( singleton )
  {
    throw std::runtime_error( "May not call DllModule::attach() in an attached state." );
  }
  singleton = std::shared_ptr< DllModule >( new DllModule( handle ) ) ;
}

/**
* The detachment function is the _de facto_ equivalent of finalization. Under ordinary circumstances, this should 
* only be called once, and called after everything else.
*/
void DllModule::Detach()
{
  singleton.reset();
}

/**
* Since this class is mostly a replacement for a global variable, it's no surprising the constructor is almost trivial.
*/
DllModule::DllModule( HINSTANCE handle )
  : handle( handle )
{
}

std::wstring DllModule::name() 
{
  if ( _name )
    return *_name ;
  throw std::runtime_error( "Not yet implemented" );
}