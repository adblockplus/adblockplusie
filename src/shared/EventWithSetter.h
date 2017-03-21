#pragma once
/*
* This file is part of Adblock Plus <https://adblockplus.org/>,
* Copyright (C) 2006-2017 eyeo GmbH
*
* Adblock Plus is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 3 as
* published by the Free Software Foundation.
*
* Adblock Plus is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "AutoHandle.h"
#include <cstdint>
#include <memory>

/// Wrapper around unnamed manual event windows object.
class Event {
public:
  enum {
    InfiniteTimeout = 0xFFFFFFFF
  };
  /// Creates manual event with nonsignaled initial state.
  Event();
  ~Event();
  void Set();
  void Reset();
  bool operator!() const {
    return m_handle == nullptr;
  }
  bool Wait(int32_t timeoutMsec = InfiniteTimeout);
private:
  Event(const Event&);
  void operator=(const Event&);
private:
  HANDLE m_handle;
};

/// This class is a temporary replacement of std::promise/std::future.
///
/// Before scheduling a task to a worker thread, the caller creates
/// `EventWithSetter` and obtains `EventWithSetter::Setter`. The task at the
/// end should call `EventWithSetter::Setter::Set` to notify the caller about
/// finishing.
/// If the the task is not finished because it has been canceled or it has
/// thrown an exception then the `EventWithSetter::Wait` returns `false`.
/// Example:
/// EventWithSetter event;
/// { // this scope is important because without it if `doWork()` throws
///   // `setter` won't be destroyed and the caller will wait forever.
///   auto setter = event.CreateSetter();
///   dispatchTask([setter]{
///     doWork();
///     setter->Set();
///   });
/// }
/// if (event.Wait()) onSuccess();
/// else onFail();
class EventWithSetter
{
public:
  class Setter;
private:
  struct Data
  {
    Data() : isSetCalled(false), isSetterCreated(false)
    {}
    Event event;
    std::weak_ptr<Setter> eventSetter;
    bool isSetCalled;
    bool isSetterCreated;
  };
  typedef std::shared_ptr<Data> DataPtr;
public:
  class Setter
  {
  public:
    Setter(const DataPtr& data);
    ~Setter();
    void Set();
  private:
    Setter(const Setter&);
    void operator=(const Setter&);
  private:
    DataPtr m_data;
  };
  EventWithSetter();
  std::shared_ptr<Setter> CreateSetter();
  bool Wait(int32_t timeoutMsec = Event::InfiniteTimeout);
private:
  EventWithSetter(const EventWithSetter&);
  void operator=(const EventWithSetter&);
private:
  DataPtr m_data;
};