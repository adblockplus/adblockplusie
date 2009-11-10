#pragma once

class ByteBuffer
{
public:

	ByteBuffer(void);
	~ByteBuffer(void);

	void Write(void* src, ULONG len);

	char *m_byteBuffer;
	ULONG m_pos;

private:

	ULONG m_curBufferSize;
	ULONG m_step;
};
