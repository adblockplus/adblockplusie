/**
* \file abp_ca.cpp Top-level source for custom actions. Includes DLL initialization.
*/
#include "DLL.h"
#include <stdexcept>

std::shared_ptr< DLL_Module > DLL_Module::singleton = 0 ;

DLL_Module & DLL_Module::module()
{ 
  if ( singleton )
  {
    return * singleton;
  }
  throw std::runtime_error( "DLL_Module::module() called when DLL module was not attached." );
}

/**
* The attachment function is the _de facto_ equivalent of initialization. Under ordinary circumstances, this should 
* only be called once, and called before everything else.
*/
void DLL_Module::attach( HINSTANCE handle )
{
  if ( singleton )
  {
    throw std::runtime_error( "May not call DLL_Module::attach() in an attached state." );
  }
  singleton = std::shared_ptr< DLL_Module >( new DLL_Module( handle ) ) ;
}

/**
* The detachment function is the _de facto_ equivalent of finalization. Under ordinary circumstances, this should 
* only be called once, and called after everything else.
*/
void DLL_Module::detach()
{
  singleton.reset();
}

/**
* Since this class is mostly a replacement for a global variable, it's no surprising the constructor is almost trivial.
*/
DLL_Module::DLL_Module( HINSTANCE handle )
  : handle( handle )
{
}

std::wstring DLL_Module::name() 
{
  if ( _name )
    return *_name ;
  throw std::runtime_error( "Not yet implemented" );
}