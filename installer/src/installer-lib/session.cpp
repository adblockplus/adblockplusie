/**
* \file session.cpp Implementation of Session class.
*/

#include "installer-lib.h"
#include "session.h"
#include "property.h"
#include "msiquery.h"

//-----------------------------------------------------------------------------------------
// Message
//-----------------------------------------------------------------------------------------
Message::Message(std::string message, INSTALLMESSAGE messageType)
  : r(1), MessageTypeCode(messageType)
{
  r.AssignString(0, message);
}

Message::Message(std::wstring message, INSTALLMESSAGE messageType)
  : r(1), MessageTypeCode(messageType)
{
  r.AssignString(0, message);
}

//-----------------------------------------------------------------------------------------
// Session
//-----------------------------------------------------------------------------------------
Session::Session(MSIHANDLE handle, std::string name)
  : handle(handle),
    logPrefix(name + ": ")
{
  logPrefixW.assign(name.begin(), name.end());
  logPrefixW += L": ";
  LogNoexcept("Entering custom action");
}

Session::~Session()
{
  LogNoexcept("Exiting custom action");
}

/**
* A message for the installation log.
*
* Writing to the installation log uses MsiProcessMessage just like interactive dialog boxes do.
*
* This class is not exposed outside this compilation unit because everything it can do is already exposed by the log functions.
*/
struct LogMessage
  : public Message
{
  LogMessage(std::wstring message)
    : Message(message, INSTALLMESSAGE_INFO)
  {}

  LogMessage(std::string message)
    : Message(message, INSTALLMESSAGE_INFO)
  {}
};

void Session::Log(std::string message)
{
  WriteMessage(LogMessage(logPrefix + message));
}

void Session::Log(std::wstring message)
{
  WriteMessage(LogMessage(logPrefixW + message));
}

void Session::LogNoexcept(std::string message)
{
  WriteMessageNoexcept(LogMessage(logPrefix + message));
}

int Session::WriteMessage(Message& m)
{
  int x = WriteMessageNoexcept(m);
  if (x == -1)
  {
    throw WindowsApiError("MsiProcessMessage", x, "attempt to write to log file");
  }
  return x;
}

int Session::WriteMessageNoexcept(Message& m)
{
  return MsiProcessMessage(handle, m.MessageTypeCode, m.r.Handle());
}

//-----------------------------------------------------------------------------------------
// ImmediateSession
//-----------------------------------------------------------------------------------------
ImmediateSession::ImmediateSession(MSIHANDLE handle, std::string name)
  : Session(handle, name)
{}
