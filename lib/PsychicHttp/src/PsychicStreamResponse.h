#ifndef PsychicStreamResponse_h
#define PsychicStreamResponse_h

#include "ChunkPrinter.h"
#include "PsychicCore.h"
#include "PsychicResponse.h"

class PsychicRequest;

class PsychicStreamResponse : public PsychicResponseDelegate, public Print
{
  private:
    ChunkPrinter* _printer;
    uint8_t* _buffer;

  public:
#ifdef ARDUINO
    PsychicStreamResponse(PsychicResponse* response, const String& contentType);
    PsychicStreamResponse(PsychicResponse* response, const String& contentType, const String& name); // Download
#else
    PsychicStreamResponse(PsychicResponse* response, const char* contentType);
    PsychicStreamResponse(PsychicResponse* response, const char* contentType, const char* name); // Download
#endif

    ~PsychicStreamResponse();

    esp_err_t beginSend();
    esp_err_t endSend();

    void flush() override;

    size_t write(uint8_t data) override;
    size_t write(const uint8_t* buffer, size_t size) override;

#ifdef ARDUINO
    size_t copyFrom(Stream& stream);
#endif

    using Print::write;
};

#endif // PsychicStreamResponse_h
