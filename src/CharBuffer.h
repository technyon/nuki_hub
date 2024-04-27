#pragma once

#define CHAR_BUFFER_SIZE 4096

class CharBuffer
{
public:
    static void initialize();
    static char* get();

private:
    static char* _buffer;
};