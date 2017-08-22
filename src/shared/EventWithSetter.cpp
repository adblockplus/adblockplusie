/*
* This file is part of Adblock Plus <https://adblockplus.org/>,
* Copyright (C) 2006-present eyeo GmbH
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
#include <cassert>
#include "EventWithSetter.h"

Event::Event()
  : m_handle(CreateEvent(/*eventAttributes*/nullptr, /*manualReset*/TRUE,
/*initState*/FALSE, /*name*/nullptr))
{
  assert(!!*this && "Handle should be successfully created");
}

Event::~Event()
{
  if (!!*this) {
    CloseHandle(m_handle);
  }
}

void Event::Set()
{
  SetEvent(m_handle);
}

void Event::Reset()
{
  ResetEvent(m_handle);
}

bool Event::Wait(int32_t timeoutMsec /*= InfiniteTimeout*/)
{
  return WaitForSingleObject(m_handle, timeoutMsec) == WAIT_OBJECT_0;
}

EventWithSetter::Setter::Setter(const DataPtr& data)
  : m_data(data)
{
}

EventWithSetter::Setter::~Setter()
{
  if (!m_data->isSetCalled)
    m_data->event.Set();
}

void EventWithSetter::Setter::Set()
{
  m_data->isSetCalled = true;
  m_data->event.Set();
}

EventWithSetter::EventWithSetter()
  : m_data(std::make_shared<Data>())
{
}

std::shared_ptr<EventWithSetter::Setter> EventWithSetter::CreateSetter()
{
  std::shared_ptr<Setter> retValue = m_data->eventSetter.lock();
  assert(!m_data->isSetterCreated && !retValue && "Setter is already created however it's supposed to be created only once");
  if (!retValue)
  {
    retValue = std::make_shared<Setter>(m_data);
  }
  m_data->isSetterCreated = true;
  return retValue;
}

bool EventWithSetter::Wait(int32_t timeoutMsec /*= Event::InfiniteTimeout*/)
{
  assert(m_data->isSetterCreated && "Wrong class usage. The setter should be created before calling Wait.");
  return m_data->isSetterCreated && m_data->event.Wait(timeoutMsec) && m_data->isSetCalled;
}