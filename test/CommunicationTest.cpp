/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
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

#include <gtest/gtest.h>

#include "../src/shared/AutoHandle.h"
#include "../src/shared/Communication.h"

const std::wstring pipeName(L"\\\\.\\pipe\\adblockplustests");

namespace
{
  DWORD WINAPI CreatePipe(LPVOID param)
  {
    Communication::Pipe pipe(pipeName, Communication::Pipe::MODE_CREATE);
    return 0;
  }

  DWORD WINAPI ReceiveSend(LPVOID param)
  {
    Communication::Pipe pipe(pipeName, Communication::Pipe::MODE_CREATE);

    Communication::InputBuffer message = pipe.ReadMessage();

    std::string stringValue;
    std::wstring wstringValue;
    int64_t int64Value;
    int32_t int32Value;
    bool boolValue;
    message >> stringValue >> wstringValue >> int64Value >> int32Value >> boolValue;

    stringValue += " Received";
    wstringValue += L" \u043f\u0440\u0438\u043d\u044f\u0442\u043e";
    int64Value += 1;
    int32Value += 2;
    boolValue = !boolValue;

    Communication::OutputBuffer response;
    response << stringValue << wstringValue << int64Value << int32Value << boolValue;
    pipe.WriteMessage(response);

    return 0;
  }
}

TEST(CommunicationTest, ConnectPipe)
{
  AutoHandle thread(CreateThread(0, 0, CreatePipe, 0, 0, 0));

  Sleep(100);

  ASSERT_NO_THROW(Communication::Pipe pipe(pipeName, Communication::Pipe::MODE_CONNECT));
}

TEST(CommunicationTest, SendReceive)
{
  AutoHandle thread(CreateThread(0, 0, ReceiveSend, 0, 0, 0));

  Sleep(100);

  Communication::Pipe pipe(pipeName, Communication::Pipe::MODE_CONNECT);

  Communication::OutputBuffer message;
  message << std::string("Foo") << std::wstring(L"Bar") << int64_t(9876543210L) << int32_t(5) << true;
  pipe.WriteMessage(message);

  Communication::InputBuffer response = pipe.ReadMessage();

  std::string stringValue;
  std::wstring wstringValue;
  int64_t int64Value;
  int32_t int32Value;
  bool boolValue;

  ASSERT_ANY_THROW(response >> wstringValue);
  ASSERT_ANY_THROW(response >> int64Value);
  ASSERT_ANY_THROW(response >> int32Value);
  ASSERT_ANY_THROW(response >> boolValue);

  response >> stringValue >> wstringValue;

  ASSERT_ANY_THROW(response >> stringValue);
  ASSERT_ANY_THROW(response >> wstringValue);
  ASSERT_ANY_THROW(response >> int32Value);
  ASSERT_ANY_THROW(response >> boolValue);

  response >> int64Value >> int32Value >> boolValue;

  ASSERT_ANY_THROW(response >> stringValue);
  ASSERT_ANY_THROW(response >> wstringValue);
  ASSERT_ANY_THROW(response >> int64Value);
  ASSERT_ANY_THROW(response >> int32Value);
  ASSERT_ANY_THROW(response >> boolValue);

  ASSERT_EQ("Foo Received", stringValue);
  ASSERT_EQ(L"Bar \u043f\u0440\u0438\u043d\u044f\u0442\u043e", wstringValue);
  ASSERT_EQ(9876543211L, int64Value);
  ASSERT_EQ(7, int32Value);
  ASSERT_FALSE(boolValue);
}

void SendReceiveStrings(const std::vector<std::string>& src)
{
  Communication::OutputBuffer outputBuffer;
  outputBuffer << src;
  Communication::InputBuffer inputBuffer(outputBuffer.Get());
  std::vector<std::string> dst;
  inputBuffer >> dst;
  auto dstSize = dst.size();
  ASSERT_EQ(dstSize, src.size());
  for (auto i = 0; i < dstSize; ++i)
  {
    EXPECT_EQ(dst[i], src[i]);
  }
}

TEST(InputOutputBuffersTests, EmptyStrings)
{
  SendReceiveStrings(std::vector<std::string>());
}

TEST(InputOutputBuffersTests, StringsWithOneValue)
{
  std::vector<std::string> src;
  src.emplace_back("string1");
  SendReceiveStrings(src);
}

TEST(InputOutputBuffersTests, MultivalueStrings)
{
  std::vector<std::string> src;
  src.emplace_back("string1");
  src.emplace_back("str2");
  src.emplace_back("value");
  SendReceiveStrings(src);
}