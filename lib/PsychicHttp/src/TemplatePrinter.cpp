/************************************************************

TemplatePrinter Class

A basic templating engine for a stream of text.
This wraps the Arduino Print interface and writes to any
Print interface.

Written by Christopher Andrews (https://github.com/Chris--A)

************************************************************/

#include "TemplatePrinter.h"

void TemplatePrinter::resetParam(bool flush)
{
  if (flush && _inParam)
  {
    _stream.write(_delimiter);

    if (_paramPos)
      _stream.print(_paramBuffer);
  }

  memset(_paramBuffer, 0, sizeof(_paramBuffer));
  _paramPos = 0;
  _inParam = false;
}

void TemplatePrinter::flush()
{
  resetParam(true);
  _stream.flush();
}

size_t TemplatePrinter::write(uint8_t data)
{

  if (data == _delimiter)
  {

    // End of parameter, send to callback
    if (_inParam)
    {

      // On false, return the parameter place holder as is: not a parameter
      // Bug fix: ignore parameters that are zero length.
      if (!_paramPos || !_cb(_stream, _paramBuffer))
      {
        resetParam(true);
        _stream.write(data);
      }
      else
      {
        resetParam(false);
      }

      // Start collecting parameter
    }
    else
    {
      _inParam = true;
    }
  }
  else
  {

    // Are we collecting
    if (_inParam)
    {

      // Is param still valid
      if (isalnum(data) || data == '_')
      {

        // Total param len must be 63, 1 for null.
        if (_paramPos < sizeof(_paramBuffer) - 1)
        {
          _paramBuffer[_paramPos++] = data;

          // Not a valid param
        }
        else
        {
          resetParam(true);
        }
      }
      else
      {
        resetParam(true);
        _stream.write(data);
      }

      // Just output
    }
    else
    {
      _stream.write(data);
    }
  }
  return 1;
}

size_t TemplatePrinter::copyFrom(Stream& stream)
{
  size_t count = 0;

  while (stream.available())
    count += this->write(stream.read());

  return count;
}
