#include "AdPluginStdAfx.h"

#include "AdPluginByteBuffer.h"

ByteBuffer::ByteBuffer(void)
{
	m_pos = 0;
	m_step = 30000;
	m_curBufferSize = m_step;
	m_byteBuffer = new char[m_curBufferSize];
}

ByteBuffer::~ByteBuffer(void)
{
	delete []m_byteBuffer;
}

void ByteBuffer::Write(void* src, ULONG len)
{
	if ((m_pos + len) > m_curBufferSize)		//enlarge the buffer
	{
		ULONG step = max(m_step, len);

		char* newBuf = new char[m_curBufferSize + step];
		memcpy(newBuf, m_byteBuffer, m_pos);
		delete []m_byteBuffer;

		m_curBufferSize += step;
		m_byteBuffer = newBuf;
	}

	memcpy(&m_byteBuffer[m_pos], src, len);
	m_pos += len;
}

