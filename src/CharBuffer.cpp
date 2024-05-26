#include "CharBuffer.h"

void CharBuffer::initialize(char16_t buffer_size)
{
    _buffer = new char[buffer_size];
}

char *CharBuffer::get()
{
    return _buffer;
}

char* CharBuffer::_buffer;
