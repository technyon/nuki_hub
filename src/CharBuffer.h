#pragma once

class CharBuffer
{
public:
    static void initialize(char16_t buffer_size);
    static char* get();

private:
    static char* _buffer;
};