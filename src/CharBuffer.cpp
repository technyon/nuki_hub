#include "CharBuffer.h"

void CharBuffer::initialize()
{
    _buffer = new char[CHAR_BUFFER_SIZE];
}

char *CharBuffer::get()
{
    return _buffer;
}

char* CharBuffer::_buffer;
