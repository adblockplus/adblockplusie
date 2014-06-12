/**
* \file property.h Installer property, whether from a live installation session or directly from a package or installed product.
*/

#ifndef PROPERTY_H
#define PROPERTY_H

#include <string>
#include "windows.h"
#include "msi.h"

/*
* Forward declaration of Session class required to break what's otherwise a cyclic definition.
*/
class Session ;

/**
* Class representing an MSI property.
*
* MSI properties arise from three places:
* - Live installations (seen in custom actions)
* - Packages (MSI files)
* - Products (as installed on a machine)
* All of these access an underlying MSI database at some remove, though the details vary.
* The underlying API calls, MsiGetProperty and MsiSetProperty, are overloaded,
*   in the sense that they take a single handle regardless of what it represents.
* Constructors for this class, therefore, require both a name and one of these places.
*
* Handles are not user-visible in this library by policy.
* Therefore this class has no public constructors.
* Constructors are private and made available to the classes surrounding a handle with a 'friend' declaration.
* These class provide factory access to property objects.
* We use the default copy constructor and assignment operator (both implicitly declared) to make the factory function work.
*
* The semantics of properties is that they always appear as defined.
* Properties not explicitly defined are considered to have the empty string (zero-length) as their value.
* The return values of the API functions, for example, do not have an error code of "property not found".
* 
* Rather than getter/setter functions, this class allows Property instances to appear exactly as strings.
* Instead of a getter, we provide a string conversion operator.
* Instead of a setter, we provide an overloaded assignment operator.
*
* \remark
* This class is specialized to std::wstring for property names and values.
* A more general library class would have these as template arguments, whether on the class or on functions.
*
* \remark
* The class makes a copy of the handle of the underlying object rather than keeping a reference to that object.
* This approach has the drawback that the user must ensure that the underlying object remains open for the lifetime of one of its derived Property instances.
* For single-threaded custom actions (the ordinary case), this is never a problem, 
*   because the entry point constructs a Session that lasts the entire duration of the CA.
* For other tools using the library, this may not be the case.
* Nevertheless, for a typical case where the scope of a Property is a single function, there's no problem.
*
* \sa MSDN on <a href="http://msdn.microsoft.com/en-us/library/windows/desktop/aa370889%28v=vs.85%29.aspx">Windows Installer Properties</a>.
*/
class Property
{
public:
  /**
  * Conversion operator to std::wstring provides rvalue access to the property.
  */
  operator std::wstring() const ;

  /**
  * Assignment operator from std::wstring provides lvalue access to the property.
  *
  * \par[in] value
  *   Value to be assigned to the property
  */
  void operator=( const std::wstring & value ) ;

  /**
  * Constructor from a session.
  *
  * The Windows Installer API uses a single handle type for all kinds of sessions.
  * Deferred sessions, though, have access only to a limited set of property values.
  * It's the responsibility of the user to ensure that property names refer to properties that contain meaningful data.
  * As a result, this constructor has base Session class as an argument, and we use this argument for both immediate and deferred sessions.
  *
  * \sa MSDN "Obtaining Context Information for Deferred Execution Custom Actions"
  *     http://msdn.microsoft.com/en-us/library/windows/desktop/aa370543%28v=vs.85%29.aspx
  *     for a list of properties that are available to deferred custom actions.
  */
  Property( Session & session, std::wstring name ) ;

private:
  /**
  * Handle to the installation, product, or package.
  * Any of these is permissible; the API does not distinguish these as types.
  */
  MSIHANDLE handle ;

  /**
  * Name of the property.
  *
  * \sa http://msdn.microsoft.com/en-us/library/windows/desktop/aa371245%28v=vs.85%29.aspx for more on property names,
  * including valid syntax and the internal scoping that the installer uses.
  */
  std::wstring name ;
} ;

/*
* We need a couple of ancillary addition operators to concatenate properties and constants strings.
* While not strictly necessary, they eliminate the need for an explicit conversion operator.
* The compiler needs a means to infer that "+" refers to string operations directly;
*   it doesn't search all possible chains of conversions to locate an operator.
* Support isn't complete, as we're not declaring concatenation for characters nor for rvalue references (the other meaning of &&).
*/
/**
* Concatenation operator for a constant-string plus a property
*/
inline std::wstring operator+( const wchar_t * left,  const Property & right )
{
  return left + std::wstring( right ) ;
}

/**
* Concatenation operator for a property and a constant-string
*/
inline std::wstring operator+( const Property & left, const wchar_t * right )
{
  return std::wstring( left ) + right ;
}

#endif
