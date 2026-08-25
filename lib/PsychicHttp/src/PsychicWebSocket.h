#ifndef PsychicWebSocket_h
#define PsychicWebSocket_h

#include "PsychicCore.h"
#include "PsychicRequest.h"

#if PSYCHIC_WS_MAX_PENDING_FRAMES > 0
  #include <atomic>
  #include <memory>
#endif

class PsychicWebSocketRequest;
class PsychicWebSocketClient;

#ifdef PSYCHIC_WS_RX_STATIC_BUFFER
// Pre-allocate the shared WS RX buffer once while the heap is still fresh.
// Called automatically from PsychicHttpServer::start(); idempotent.
extern "C" void psychic_ws_preinit_rx_buf();
#endif

// callback function definitions
typedef std::function<void(PsychicWebSocketClient* client)> PsychicWebSocketClientCallback;
typedef std::function<esp_err_t(PsychicWebSocketRequest* request, httpd_ws_frame* frame)> PsychicWebSocketFrameCallback;

class PsychicWebSocketClient : public PsychicClient
{
  protected:
    static void _sendMessageCallback(esp_err_t err, int socket, void* arg);

  public:
    PsychicWebSocketClient(PsychicClient* client);
    ~PsychicWebSocketClient();

#if PSYCHIC_WS_MAX_PENDING_FRAMES > 0
    // Number of frames this client has queued via httpd_ws_send_data_async()
    // that have not yet completed.  Incremented in sendMessage(), decremented
    // in _sendMessageCallback().  Held in a shared_ptr so an in-flight async
    // send can safely outlive the client object (the client is destroyed on
    // disconnect while its callbacks may still be pending on the httpd task).
    std::shared_ptr<std::atomic<int>> _pendingFrames;
#endif

    esp_err_t sendMessage(httpd_ws_frame_t* ws_pkt);
    esp_err_t sendMessage(httpd_ws_type_t op, const void* data, size_t len);
    esp_err_t sendMessage(const char* buf);
};

class PsychicWebSocketRequest : public PsychicRequest
{
  private:
    PsychicWebSocketClient _client;

  public:
    PsychicWebSocketRequest(PsychicRequest* req);
    virtual ~PsychicWebSocketRequest();

    PsychicWebSocketClient* client() override;

    esp_err_t reply(httpd_ws_frame_t* ws_pkt);
    esp_err_t reply(httpd_ws_type_t op, const void* data, size_t len);
    esp_err_t reply(const char* buf);
};

class PsychicWebSocketHandler : public PsychicHandler
{
  protected:
    PsychicWebSocketClientCallback _onOpen;
    PsychicWebSocketFrameCallback _onFrame;
    PsychicWebSocketClientCallback _onClose;

  public:
    PsychicWebSocketHandler();
    ~PsychicWebSocketHandler();

    PsychicWebSocketClient* getClient(int socket) override;
    PsychicWebSocketClient* getClient(PsychicClient* client) override;
    void addClient(PsychicClient* client) override;
    void removeClient(PsychicClient* client) override;
    void openCallback(PsychicClient* client) override;
    void closeCallback(PsychicClient* client) override;

    bool isWebSocket() override final;
    esp_err_t handleRequest(PsychicRequest* request, PsychicResponse* response) override;

    PsychicWebSocketHandler* onOpen(PsychicWebSocketClientCallback fn);
    PsychicWebSocketHandler* onFrame(PsychicWebSocketFrameCallback fn);
    PsychicWebSocketHandler* onClose(PsychicWebSocketClientCallback fn);

    void sendAll(httpd_ws_frame_t* ws_pkt);
    void sendAll(httpd_ws_type_t op, const void* data, size_t len);
    void sendAll(const char* buf);
};

#endif // PsychicWebSocket_h