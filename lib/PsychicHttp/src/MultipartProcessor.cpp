#include "MultipartProcessor.h"
#include "PsychicRequest.h"

enum
{
  EXPECT_BOUNDARY,
  PARSE_HEADERS,
  WAIT_FOR_RETURN1,
  EXPECT_FEED1,
  EXPECT_DASH1,
  EXPECT_DASH2,
  BOUNDARY_OR_DATA,
  DASH3_OR_RETURN2,
  EXPECT_FEED2,
  PARSING_FINISHED,
  PARSE_ERROR
};

MultipartProcessor::MultipartProcessor(PsychicRequest* request, PsychicUploadCallback uploadCallback) : _request(request),
                                                                                                        _uploadCallback(uploadCallback),
                                                                                                        _temp(),
                                                                                                        _parsedLength(0),
                                                                                                        _multiParseState(EXPECT_BOUNDARY),
                                                                                                        _boundaryPosition(0),
                                                                                                        _itemStartIndex(0),
                                                                                                        _itemSize(0),
                                                                                                        _itemName(),
                                                                                                        _itemFilename(),
                                                                                                        _itemType(),
                                                                                                        _itemValue(),
                                                                                                        _itemBuffer(0),
                                                                                                        _itemBufferIndex(0),
                                                                                                        _itemIsFile(false)
{
}
MultipartProcessor::~MultipartProcessor() {}

esp_err_t MultipartProcessor::process()
{
  esp_err_t err = ESP_OK;

  _parsedLength = 0;

  String value = _request->header("Content-Type");
  if (value.startsWith("multipart/"))
  {
    _boundary = value.substring(value.indexOf('=') + 1);
    _boundary.replace("\"", "");
  }
  else
  {
    ESP_LOGE(PH_TAG, "No multipart boundary found.");
    return ESP_ERR_HTTPD_INVALID_REQ;
  }

  char* buf = (char*)malloc(FILE_CHUNK_SIZE);
  int received;
  unsigned long index = 0;

  /* Content length of the request gives the size of the file being uploaded */
  int remaining = _request->contentLength();

  while (remaining > 0)
  {
#ifdef ENABLE_ASYNC
    httpd_sess_update_lru_counter(request->server()->server, request->client()->socket());
#endif

    /* Receive the file part by part into a buffer */
    if ((received = httpd_req_recv(_request->request(), buf, min(remaining, FILE_CHUNK_SIZE))) <= 0)
    {
      /* Retry if timeout occurred */
      if (received == HTTPD_SOCK_ERR_TIMEOUT)
        continue;
      // bail if we got an error
      else if (received == HTTPD_SOCK_ERR_FAIL)
      {
        ESP_LOGE(PH_TAG, "Socket error");
        err = ESP_FAIL;
        break;
      }
    }

    // parse it 1 byte at a time.
    for (int i = 0; i < received; i++)
    {
      /* Keep track of remaining size of the file left to be uploaded */
      remaining--;
      index++;

      // send it to our parser
      _parseMultipartPostByte(buf[i], !remaining);
      _parsedLength++;
    }
  }

  // dont forget to free our buffer
  free(buf);

  return err;
}

esp_err_t MultipartProcessor::process(const char* body)
{
  esp_err_t err = ESP_OK;
  _parsedLength = 0;

  String value = _request->header("Content-Type");
  if (value.startsWith("multipart/"))
  {
    _boundary = value.substring(value.indexOf('=') + 1);
    _boundary.replace("\"", "");
  }
  else
  {
    ESP_LOGE(PH_TAG, "No multipart boundary found.");
    return ESP_ERR_HTTPD_INVALID_REQ;
  }

  // loop over the whole string
  unsigned int size = strlen(body);
  for (unsigned i = 0; i < size; i++)
  {
    // send it to our parser
    _parseMultipartPostByte(body[i], i == size - 1);
    _parsedLength++;
  }

  return err;
}

void MultipartProcessor::_handleUploadByte(uint8_t data, bool last)
{
  _itemBuffer[_itemBufferIndex++] = data;

  if (last || _itemBufferIndex == FILE_CHUNK_SIZE)
  {
    if (_uploadCallback)
      _uploadCallback(_request, _itemFilename, _itemSize - _itemBufferIndex, _itemBuffer, _itemBufferIndex, last);

    _itemBufferIndex = 0;
  }
}

#define itemWriteByte(b)          \
  do                              \
  {                               \
    _itemSize++;                  \
    if (_itemIsFile)              \
      _handleUploadByte(b, last); \
    else                          \
      _itemValue += (char)(b);    \
  } while (0)

void MultipartProcessor::_parseMultipartPostByte(uint8_t data, bool last)
{
  if (_multiParseState == PARSE_ERROR)
  {
    // not sure we can end up with an error during buffer fill, but jsut to be safe
    if (_itemBuffer != NULL)
    {
      free(_itemBuffer);
      _itemBuffer = NULL;
    }

    return;
  }

  if (!_parsedLength)
  {
    _multiParseState = EXPECT_BOUNDARY;
    _temp = String();
    _itemName = String();
    _itemFilename = String();
    _itemType = String();
  }

  if (_multiParseState == WAIT_FOR_RETURN1)
  {
    if (data != '\r')
    {
      itemWriteByte(data);
    }
    else
    {
      _multiParseState = EXPECT_FEED1;
    }
  }
  else if (_multiParseState == EXPECT_BOUNDARY)
  {
    if (_parsedLength < 2 && data != '-')
    {
      ESP_LOGE(PH_TAG, "Multipart: No boundary");
      _multiParseState = PARSE_ERROR;
      return;
    }
    else if (_parsedLength - 2 < _boundary.length() && _boundary.c_str()[_parsedLength - 2] != data)
    {
      ESP_LOGE(PH_TAG, "Multipart: Multipart malformed");
      _multiParseState = PARSE_ERROR;
      return;
    }
    else if (_parsedLength - 2 == _boundary.length() && data != '\r')
    {
      ESP_LOGE(PH_TAG, "Multipart: Multipart missing carriage return");
      _multiParseState = PARSE_ERROR;
      return;
    }
    else if (_parsedLength - 3 == _boundary.length())
    {
      if (data != '\n')
      {
        ESP_LOGE(PH_TAG, "Multipart: Multipart missing newline");
        _multiParseState = PARSE_ERROR;
        return;
      }
      _multiParseState = PARSE_HEADERS;
      _itemIsFile = false;
    }
  }
  else if (_multiParseState == PARSE_HEADERS)
  {
    if ((char)data != '\r' && (char)data != '\n')
      _temp += (char)data;
    if ((char)data == '\n')
    {
      if (_temp.length())
      {
        if (_temp.length() > 12 && _temp.substring(0, 12).equalsIgnoreCase("Content-Type"))
        {
          _itemType = _temp.substring(14);
          _itemIsFile = true;
        }
        else if (_temp.length() > 19 && _temp.substring(0, 19).equalsIgnoreCase("Content-Disposition"))
        {
          _temp = _temp.substring(_temp.indexOf(';') + 2);
          while (_temp.indexOf(';') > 0)
          {
            String name = _temp.substring(0, _temp.indexOf('='));
            String nameVal = _temp.substring(_temp.indexOf('=') + 2, _temp.indexOf(';') - 1);
            if (name == "name")
            {
              _itemName = nameVal;
            }
            else if (name == "filename")
            {
              _itemFilename = nameVal;
              _itemIsFile = true;
            }
            _temp = _temp.substring(_temp.indexOf(';') + 2);
          }
          String name = _temp.substring(0, _temp.indexOf('='));
          String nameVal = _temp.substring(_temp.indexOf('=') + 2, _temp.length() - 1);
          if (name == "name")
          {
            _itemName = nameVal;
          }
          else if (name == "filename")
          {
            _itemFilename = nameVal;
            _itemIsFile = true;
          }
        }
        _temp = String();
      }
      else
      {
        _multiParseState = WAIT_FOR_RETURN1;
        // value starts from here
        _itemSize = 0;
        _itemStartIndex = _parsedLength;
        _itemValue = String();
        if (_itemIsFile)
        {
          if (_itemBuffer)
            free(_itemBuffer);
          _itemBuffer = (uint8_t*)malloc(FILE_CHUNK_SIZE);
          if (_itemBuffer == NULL)
          {
            ESP_LOGE(PH_TAG, "Multipart: Failed to allocate buffer");
            _multiParseState = PARSE_ERROR;
            return;
          }
          _itemBufferIndex = 0;
        }
      }
    }
  }
  else if (_multiParseState == EXPECT_FEED1)
  {
    if (data != '\n')
    {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      _parseMultipartPostByte(data, last);
    }
    else
    {
      _multiParseState = EXPECT_DASH1;
    }
  }
  else if (_multiParseState == EXPECT_DASH1)
  {
    if (data != '-')
    {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      itemWriteByte('\n');
      _parseMultipartPostByte(data, last);
    }
    else
    {
      _multiParseState = EXPECT_DASH2;
    }
  }
  else if (_multiParseState == EXPECT_DASH2)
  {
    if (data != '-')
    {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      itemWriteByte('\n');
      itemWriteByte('-');
      _parseMultipartPostByte(data, last);
    }
    else
    {
      _multiParseState = BOUNDARY_OR_DATA;
      _boundaryPosition = 0;
    }
  }
  else if (_multiParseState == BOUNDARY_OR_DATA)
  {
    if (_boundaryPosition < _boundary.length() && _boundary.c_str()[_boundaryPosition] != data)
    {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      itemWriteByte('\n');
      itemWriteByte('-');
      itemWriteByte('-');
      uint8_t i;
      for (i = 0; i < _boundaryPosition; i++)
        itemWriteByte(_boundary.c_str()[i]);
      _parseMultipartPostByte(data, last);
    }
    else if (_boundaryPosition == _boundary.length() - 1)
    {
      _multiParseState = DASH3_OR_RETURN2;
      if (!_itemIsFile)
      {
        // External - Add parameter!
        _request->addParam(_itemName, _itemValue);
      }
      else
      {
        if (_itemSize)
        {
          if (_uploadCallback)
          {
            _uploadCallback(_request, _itemFilename, _itemSize - _itemBufferIndex, _itemBuffer, _itemBufferIndex, true);
          }
          _itemBufferIndex = 0;

          // External - Add parameter!
          _request->addParam(new PsychicWebParameter(_itemName, _itemFilename, true, true, _itemSize));
        }
        free(_itemBuffer);
        _itemBuffer = NULL;
      }
    }
    else
    {
      _boundaryPosition++;
    }
  }
  else if (_multiParseState == DASH3_OR_RETURN2)
  {
    if (data == '-' && (_request->contentLength() - _parsedLength - 4) != 0)
    {
      ESP_LOGE(PH_TAG, "ERROR: The parser got to the end of the POST but is expecting more bytes!");
      _multiParseState = PARSE_ERROR;
      return;
    }
    if (data == '\r')
    {
      _multiParseState = EXPECT_FEED2;
    }
    else if (data == '-' && _request->contentLength() == (_parsedLength + 4))
    {
      _multiParseState = PARSING_FINISHED;
    }
    else
    {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      itemWriteByte('\n');
      itemWriteByte('-');
      itemWriteByte('-');
      uint8_t i;
      for (i = 0; i < _boundary.length(); i++)
        itemWriteByte(_boundary.c_str()[i]);
      _parseMultipartPostByte(data, last);
    }
  }
  else if (_multiParseState == EXPECT_FEED2)
  {
    if (data == '\n')
    {
      _multiParseState = PARSE_HEADERS;
      _itemIsFile = false;
    }
    else
    {
      _multiParseState = WAIT_FOR_RETURN1;
      itemWriteByte('\r');
      itemWriteByte('\n');
      itemWriteByte('-');
      itemWriteByte('-');
      uint8_t i;
      for (i = 0; i < _boundary.length(); i++)
        itemWriteByte(_boundary.c_str()[i]);
      itemWriteByte('\r');
      _parseMultipartPostByte(data, last);
    }
  }
}