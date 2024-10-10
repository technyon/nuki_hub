#ifndef ChunkPrinter_h
#define ChunkPrinter_h

#include "PsychicResponse.h"
#include <Print.h>

class ChunkPrinter : public Print
{
  private:
    PsychicResponse *_response;
    uint8_t *_buffer;
    size_t _length;
    size_t _pos;

  public:
    ChunkPrinter(PsychicResponse *response, uint8_t *buffer, size_t len);
    ~ChunkPrinter();
  
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;

    size_t copyFrom(Stream &stream);

    void flush() override;
};

#endif