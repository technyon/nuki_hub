#ifndef MULTIPART_PROCESSOR_H
#define MULTIPART_PROCESSOR_H

#include "PsychicCore.h"

/*
 * MultipartProcessor - handle parsing and processing a multipart form.
 * */

class MultipartProcessor
{
  protected:
    PsychicRequest* _request;
    PsychicUploadCallback _uploadCallback;

    String _temp;
    size_t _parsedLength;
    uint8_t _multiParseState;
    String _boundary;
    uint8_t _boundaryPosition;
    size_t _itemStartIndex;
    size_t _itemSize;
    String _itemName;
    String _itemFilename;
    String _itemType;
    String _itemValue;
    uint8_t* _itemBuffer;
    size_t _itemBufferIndex;
    bool _itemIsFile;

    void _handleUploadByte(uint8_t data, bool last);
    void _parseMultipartPostByte(uint8_t data, bool last);

  public:
    MultipartProcessor(PsychicRequest* request, PsychicUploadCallback uploadCallback = nullptr);
    ~MultipartProcessor();

    esp_err_t process();
    esp_err_t process(const char* body);
};

#endif