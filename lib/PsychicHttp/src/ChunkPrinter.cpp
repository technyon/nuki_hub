
#include "ChunkPrinter.h"

ChunkPrinter::ChunkPrinter(PsychicResponse *response, uint8_t *buffer, size_t len) :
  _response(response),
  _buffer(buffer),
  _length(len),
  _pos(0)
{}

ChunkPrinter::~ChunkPrinter()
{
  flush();
}

size_t ChunkPrinter::write(uint8_t c)
{
  esp_err_t err;
  
  //if we're full, send a chunk
  if (_pos == _length)
  {
    _pos = 0;
    err = _response->sendChunk(_buffer, _length);
	
    if (err != ESP_OK)
      return 0;
  }    

  _buffer[_pos] = c;
  _pos++;
  return 1;
}

size_t ChunkPrinter::write(const uint8_t *buffer, size_t size)
{
  size_t written = 0;
  
  while (written < size)
  {
    size_t space = _length - _pos;
    size_t blockSize = std::min(space, size - written);
    
    memcpy(_buffer + _pos, buffer + written, blockSize);
    _pos += blockSize;
    
    if (_pos == _length)
    {
      _pos = 0;

      if (_response->sendChunk(_buffer, _length) != ESP_OK)
        return written;
    }
    written += blockSize; //Update if sent correctly.
  }
  return written;
}

void ChunkPrinter::flush()
{
  if (_pos)
  {
    _response->sendChunk(_buffer, _pos);
    _pos = 0;
  }
}

size_t ChunkPrinter::copyFrom(Stream &stream)
{
  size_t count = 0;

  while (stream.available()){
    
    if (_pos == _length)
    {
      _response->sendChunk(_buffer, _length);
      _pos = 0;
    }
    
    size_t readBytes = stream.readBytes(_buffer + _pos, _length - _pos);
    _pos += readBytes;
    count += readBytes;
  }
  return count;
}