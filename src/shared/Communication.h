#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <memory>
#include <sstream>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <vector>
#include <Windows.h>

namespace Communication
{
  extern const std::wstring pipeName;
  const int bufferSize = 1024;

  enum {TYPE_STRING, TYPE_WSTRING, TYPE_INT64, TYPE_INT32, TYPE_BOOL};
  typedef int32_t ValueType;
  typedef uint32_t SizeType;

  class InputBuffer
  {
  public:
    InputBuffer(const std::string& data) : buffer(data), hasType(false) {}
    InputBuffer& operator>>(std::string& value) { return ReadString(value, TYPE_STRING); }
    InputBuffer& operator>>(std::wstring& value) { return ReadString(value, TYPE_WSTRING); }
    InputBuffer& operator>>(int64_t& value) { return Read(value, TYPE_INT64); }
    InputBuffer& operator>>(int32_t& value) { return Read(value, TYPE_INT32); }
    InputBuffer& operator>>(bool& value) { return Read(value, TYPE_BOOL); }
  private:
    std::istringstream buffer;
    ValueType currentType;
    bool hasType;

    void CheckType(ValueType expectedType)
    {
      if (!hasType)
        ReadBinary(currentType);

      if (currentType != expectedType)
      {
        // Make sure we don't attempt to read the type again
        hasType = true;
        throw new std::runtime_error("Unexpected type found in input buffer");
      }
      else
        hasType = false;
    }

    template<class T>
    InputBuffer& ReadString(T& value, ValueType expectedType)
    {
      CheckType(expectedType);

      SizeType length;
      ReadBinary(length);

      std::auto_ptr<T::value_type> data(new T::value_type[length]);
      buffer.read(reinterpret_cast<char*>(data.get()), sizeof(T::value_type) * length);
      if (buffer.fail())
        throw new std::runtime_error("Unexpected end of input buffer");

      value.assign(data.get(), length);
      return *this;
    }

    template<class T>
    InputBuffer& Read(T& value, ValueType expectedType)
    {
      CheckType(expectedType);
      ReadBinary(value);
      return *this;
    }

    template<class T>
    void ReadBinary(T& value)
    {
      buffer.read(reinterpret_cast<char*>(&value), sizeof(T));
      if (buffer.fail())
        throw new std::runtime_error("Unexpected end of input buffer");
    }
  };

  class OutputBuffer
  {
  public:
    OutputBuffer() {}

    // Explicit copy constructor to allow returning OutputBuffer by value
    OutputBuffer(const OutputBuffer& copy) : buffer(copy.buffer.str()) {}

    std::string Get()
    {
      return buffer.str();
    }
    OutputBuffer& operator<<(const std::string& value) { return WriteString(value, TYPE_STRING); }
    OutputBuffer& operator<<(const std::wstring& value) { return WriteString(value, TYPE_WSTRING); }
    OutputBuffer& operator<<(int64_t value) { return Write(value, TYPE_INT64); }
    OutputBuffer& operator<<(int32_t value) { return Write(value, TYPE_INT32); }
    OutputBuffer& operator<<(bool value) { return Write(value, TYPE_BOOL); }
  private:
    std::ostringstream buffer;

    template<class T>
    OutputBuffer& WriteString(const T& value, ValueType type)
    {
      WriteBinary(type);

      SizeType length = value.size();
      WriteBinary(length);

      buffer.write(reinterpret_cast<const char*>(value.c_str()), sizeof(T::value_type) * length);
      if (buffer.fail())
        throw new std::runtime_error("Unexpected error writing to output buffer");

      return *this;
    }

    template<class T>
    OutputBuffer& Write(const T value, ValueType type)
    {
      WriteBinary(type);
      WriteBinary(value);
      return *this;
    }

    template<class T>
    void WriteBinary(const T& value)
    {
      buffer.write(reinterpret_cast<const char*>(&value), sizeof(T));
      if (buffer.fail())
        throw new std::runtime_error("Unexpected error writing to output buffer");
    }
  };

  class PipeConnectionError : public std::runtime_error
  {
  public:
    PipeConnectionError();
  };

  class Pipe
  {
  public:
    enum Mode {MODE_CREATE, MODE_CONNECT};

    Pipe(const std::wstring& name, Mode mode);
    ~Pipe();

    InputBuffer ReadMessage();
    void WriteMessage(OutputBuffer& message);

  protected:
    HANDLE pipe;
  };
}

#endif
