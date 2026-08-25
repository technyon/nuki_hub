#include "PsychicWebSocket.h"
#ifdef PSYCHIC_WS_PSRAM_PAYLOAD
#include <esp_heap_caps.h>
#endif
#ifdef PSYCHIC_WS_RX_STATIC_BUFFER
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
#include <string.h>

// Static RX buffer: pre-allocate once at server start (heap still fresh) so
// that each incoming WS frame reuses the same region instead of calloc/free.
// Repeated calloc/free per frame fragments internal SRAM on no-PSRAM boards —
// after hundreds of frames the largest free block drops below a per-frame alloc
// even when total free heap is healthy, causing spurious WS disconnects.
// Pre-alloc happens in PsychicHttpServer::start() via psychic_ws_preinit_rx_buf().
static SemaphoreHandle_t s_ws_rx_mutex = NULL;
static uint8_t* s_ws_rx_buf = NULL;

extern "C" void psychic_ws_preinit_rx_buf() {
  if (!s_ws_rx_mutex) s_ws_rx_mutex = xSemaphoreCreateMutex();
  if (s_ws_rx_mutex && !s_ws_rx_buf) {
    s_ws_rx_buf = (uint8_t*)heap_caps_malloc(PSYCHIC_WS_MAX_FRAME_SIZE + 1,
                                              MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!s_ws_rx_buf)
      ESP_LOGE("PH", "psychic_ws_preinit_rx_buf: alloc %u B failed",
               (unsigned)PSYCHIC_WS_MAX_FRAME_SIZE + 1);
    else
      ESP_LOGI("PH", "WS RX static buf pre-allocated (%u B)", (unsigned)PSYCHIC_WS_MAX_FRAME_SIZE + 1);
  }
}
#endif

/*************************************/
/*  PsychicWebSocketRequest      */
/*************************************/

PsychicWebSocketRequest::PsychicWebSocketRequest(PsychicRequest* req) : PsychicRequest(req->server(), req->request()),
                                                                        _client(req->client())
{
}

PsychicWebSocketRequest::~PsychicWebSocketRequest()
{
}

PsychicWebSocketClient* PsychicWebSocketRequest::client()
{
  return &_client;
}

esp_err_t PsychicWebSocketRequest::reply(httpd_ws_frame_t* ws_pkt)
{
  return httpd_ws_send_frame(this->_req, ws_pkt);
}

esp_err_t PsychicWebSocketRequest::reply(httpd_ws_type_t op, const void* data, size_t len)
{
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

  ws_pkt.payload = (uint8_t*)data;
  ws_pkt.len = len;
  ws_pkt.type = op;

  return this->reply(&ws_pkt);
}

esp_err_t PsychicWebSocketRequest::reply(const char* buf)
{
  return this->reply(HTTPD_WS_TYPE_TEXT, buf, strlen(buf));
}

/*************************************/
/*  PsychicWebSocketClient   */
/*************************************/

#if PSYCHIC_WS_MAX_PENDING_FRAMES > 0
// Context handed to the async-send completion callback.  Bundles the frame to
// free with a shared reference to the issuing client's pending-frame counter so
// the counter can be decremented even if the client itself has been destroyed.
struct PsychicWsSendCtx {
  httpd_ws_frame_t* ws_pkt;
  std::shared_ptr<std::atomic<int>> pending;
};
#endif

PsychicWebSocketClient::PsychicWebSocketClient(PsychicClient* client)
    : PsychicClient(client->server(), client->socket())
#if PSYCHIC_WS_MAX_PENDING_FRAMES > 0
    , _pendingFrames(std::make_shared<std::atomic<int>>(0))
#endif
{
}

PsychicWebSocketClient::~PsychicWebSocketClient()
{
}

void PsychicWebSocketClient::_sendMessageCallback(esp_err_t err, int socket, void* arg)
{
#if PSYCHIC_WS_MAX_PENDING_FRAMES > 0
  // free our frame and release one slot from the client's in-flight counter.
  PsychicWsSendCtx* ctx = (PsychicWsSendCtx*)arg;
  ctx->pending->fetch_sub(1);
  free(ctx->ws_pkt->payload);
  free(ctx->ws_pkt);
  delete ctx;
#else
  // free our frame.
  httpd_ws_frame_t* ws_pkt = (httpd_ws_frame_t*)arg;
  free(ws_pkt->payload);
  free(ws_pkt);
#endif

  if (err == ESP_OK)
    return;
  else if (err == ESP_FAIL)
    ESP_LOGE(PH_TAG, "Websocket: send - socket error (#%d)", socket);
  else if (err == ESP_ERR_INVALID_STATE)
    ESP_LOGE(PH_TAG, "Websocket: Handshake was already done beforehand (#%d)", socket);
  else if (err == ESP_ERR_INVALID_ARG)
    ESP_LOGE(PH_TAG, "Websocket: Argument is invalid (null or non-WebSocket) (#%d)", socket);
  else
    ESP_LOGE(PH_TAG, "Websocket: Send message unknown error. (#%d)", socket);
}

esp_err_t PsychicWebSocketClient::sendMessage(httpd_ws_frame_t* ws_pkt)
{
  return sendMessage(ws_pkt->type, ws_pkt->payload, ws_pkt->len);
}

esp_err_t PsychicWebSocketClient::sendMessage(httpd_ws_type_t op, const void* data, size_t len)
{
#if PSYCHIC_WS_MAX_PENDING_FRAMES > 0
  // Backpressure: a client whose TCP connection has stalled (WiFi roam, out of
  // range, half-open socket) stops draining its send queue, so async frames
  // pile up in the heap until the device runs out of memory.  Refuse to queue
  // any more once this client already has too many frames in flight.
  int pending = _pendingFrames->load();
  if (pending >= PSYCHIC_WS_MAX_PENDING_FRAMES) {
    ESP_LOGW(PH_TAG, "Websocket: client #%d send queue full (%d pending) — dropping frame", socket(), pending);
    return ESP_ERR_NO_MEM;
  }
#endif

  // init our frame.
  httpd_ws_frame_t* ws_pkt = (httpd_ws_frame_t*)malloc(sizeof(httpd_ws_frame_t));
  if (ws_pkt == NULL) {
    ESP_LOGE(PH_TAG, "Websocket: out of memory");
    return ESP_ERR_NO_MEM;
  }
  memset(ws_pkt, 0, sizeof(httpd_ws_frame_t)); // zero the datastructure out

  // allocate for event text
#ifdef PSYCHIC_WS_PSRAM_PAYLOAD
  // The payload copy is only read by lwip while the send drains — it has no
  // need to live in scarce internal DRAM.  Prefer PSRAM, fall back to internal
  // heap on boards without it (heap_caps_malloc returns NULL there).
  ws_pkt->payload = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (ws_pkt->payload == NULL)
    ws_pkt->payload = (uint8_t*)malloc(len);
#else
  ws_pkt->payload = (uint8_t*)malloc(len);
#endif
  if (ws_pkt->payload == NULL) {
    ESP_LOGE(PH_TAG, "Websocket: out of memory");
    free(ws_pkt); // free our other memory
    return ESP_ERR_NO_MEM;
  }
  memcpy(ws_pkt->payload, data, len);

  ws_pkt->len = len;
  ws_pkt->type = op;

#if PSYCHIC_WS_MAX_PENDING_FRAMES > 0
  PsychicWsSendCtx* ctx = new (std::nothrow) PsychicWsSendCtx{ws_pkt, _pendingFrames};
  if (ctx == NULL) {
    ESP_LOGE(PH_TAG, "Websocket: out of memory");
    free(ws_pkt->payload);
    free(ws_pkt);
    return ESP_ERR_NO_MEM;
  }

  // count this frame as in flight before queuing; the callback (success or
  // failure) decrements, and we roll back here if the queue call itself fails.
  _pendingFrames->fetch_add(1);
  esp_err_t err = httpd_ws_send_data_async(server(), socket(), ws_pkt, PsychicWebSocketClient::_sendMessageCallback, ctx);
  if (err != ESP_OK) {
    _pendingFrames->fetch_sub(1);
    free(ws_pkt->payload);
    free(ws_pkt);
    delete ctx;
  }
#else
  esp_err_t err = httpd_ws_send_data_async(server(), socket(), ws_pkt, PsychicWebSocketClient::_sendMessageCallback, ws_pkt);

  // take care of memory
  if (err != ESP_OK) {
    free(ws_pkt->payload);
    free(ws_pkt);
  }
#endif

  return err;
}

esp_err_t PsychicWebSocketClient::sendMessage(const char* buf)
{
  return this->sendMessage(HTTPD_WS_TYPE_TEXT, buf, strlen(buf));
}

PsychicWebSocketHandler::PsychicWebSocketHandler() : PsychicHandler(),
                                                     _onOpen(NULL),
                                                     _onFrame(NULL),
                                                     _onClose(NULL)
{
}

PsychicWebSocketHandler::~PsychicWebSocketHandler()
{
}

PsychicWebSocketClient* PsychicWebSocketHandler::getClient(int socket)
{
  PsychicClient* client = PsychicHandler::getClient(socket);
  if (client == NULL)
    return NULL;

  if (client->_friend == NULL) {
    return NULL;
  }

  return (PsychicWebSocketClient*)client->_friend;
}

PsychicWebSocketClient* PsychicWebSocketHandler::getClient(PsychicClient* client)
{
  return getClient(client->socket());
}

void PsychicWebSocketHandler::addClient(PsychicClient* client)
{
  client->_friend = new PsychicWebSocketClient(client);
  PsychicHandler::addClient(client);
}

void PsychicWebSocketHandler::removeClient(PsychicClient* client)
{
  PsychicHandler::removeClient(client);
  delete (PsychicWebSocketClient*)client->_friend;
  client->_friend = NULL;
}

void PsychicWebSocketHandler::openCallback(PsychicClient* client)
{
  PsychicWebSocketClient* buddy = getClient(client);
  if (buddy == NULL) {
    return;
  }

  if (_onOpen != NULL)
    _onOpen(getClient(buddy));
}

void PsychicWebSocketHandler::closeCallback(PsychicClient* client)
{
  PsychicWebSocketClient* buddy = getClient(client);
  if (buddy == NULL) {
    return;
  }

  if (_onClose != NULL)
    _onClose(getClient(buddy));
}

bool PsychicWebSocketHandler::isWebSocket() { return true; }

esp_err_t PsychicWebSocketHandler::handleRequest(PsychicRequest* request, PsychicResponse* response)
{
  // lookup our client
  PsychicClient* client = checkForNewClient(request->client());

  // beginning of the ws URI handler and our onConnect hook
  if (request->method() == HTTP_GET) {
    if (client->isNew)
      openCallback(client);

    return ESP_OK;
  }

  // prep our request
  PsychicWebSocketRequest wsRequest(request);

  // init our memory for storing the packet
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
#ifdef PSYCHIC_WS_RX_STATIC_BUFFER
  bool ws_rx_locked = false;
#else
  uint8_t* buf = NULL;
#endif

  /* Set max_len = 0 to get the frame len */
  esp_err_t ret = httpd_ws_recv_frame(wsRequest.request(), &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(PH_TAG, "httpd_ws_recv_frame failed to get frame len with %s", esp_err_to_name(ret));
    return ret;
  }

  // okay, now try to load the packet
  // ESP_LOGD(PH_TAG, "frame len is %d", ws_pkt.len);
#ifdef PSYCHIC_WS_MAX_FRAME_SIZE
  if (ws_pkt.len > PSYCHIC_WS_MAX_FRAME_SIZE) {
    // Reject oversized frames before calloc to protect heap on constrained boards.
    ESP_LOGW(PH_TAG, "WS frame too big (%u > %u) — closing peer",
             (unsigned)ws_pkt.len, (unsigned)PSYCHIC_WS_MAX_FRAME_SIZE);
    return ESP_FAIL;
  }
#endif
#ifdef PSYCHIC_WS_RX_STATIC_BUFFER
  // Reuse one static RX buffer (allocated at server start) instead of
  // calloc/free per frame. The calloc/free pattern fragments internal SRAM
  // on no-PSRAM boards — after many frames the largest free block shrinks
  // below a per-frame alloc even when total free heap is still healthy.
  if (ws_pkt.len) {
    if (!s_ws_rx_mutex) psychic_ws_preinit_rx_buf();
    if (!s_ws_rx_mutex || !s_ws_rx_buf) {
      ESP_LOGE(PH_TAG, "WS RX static buf unavailable — dropping frame");
      return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(s_ws_rx_mutex, portMAX_DELAY);
    ws_rx_locked = true;
    memset(s_ws_rx_buf, 0, ws_pkt.len + 1);
    ws_pkt.payload = s_ws_rx_buf;
    ret = httpd_ws_recv_frame(wsRequest.request(), &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(PH_TAG, "httpd_ws_recv_frame failed with %s", esp_err_to_name(ret));
      xSemaphoreGive(s_ws_rx_mutex);
      ws_rx_locked = false;
      return ret;
    }
  }
#else
  if (ws_pkt.len) {
    /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
    buf = (uint8_t*)calloc(1, ws_pkt.len + 1);
    if (buf == NULL) {
      ESP_LOGE(PH_TAG, "Failed to calloc memory for buf");
      return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;
    /* Set max_len = ws_pkt.len to get the frame payload */
    ret = httpd_ws_recv_frame(wsRequest.request(), &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(PH_TAG, "httpd_ws_recv_frame failed with %s", esp_err_to_name(ret));
      free(buf);
      return ret;
    }
    // ESP_LOGD(PH_TAG, "Got packet with message: %s", ws_pkt.payload);
  }
#endif
  if (ws_pkt.type == HTTPD_WS_TYPE_PING) {
      // Respond to ping with pong using the same payload
      ret = wsRequest.reply(HTTPD_WS_TYPE_PONG, ws_pkt.payload, ws_pkt.len);
      if (ret != ESP_OK) {
          ESP_LOGE(PH_TAG, "Failed to send pong response: %s", esp_err_to_name(ret));
      }
  }
  // Text messages are our payload.
  if (ws_pkt.type == HTTPD_WS_TYPE_TEXT || ws_pkt.type == HTTPD_WS_TYPE_BINARY) {
    if (this->_onFrame != NULL)
      ret = this->_onFrame(&wsRequest, &ws_pkt);
  }

  // logging housekeeping
  if (ret != ESP_OK)
    ESP_LOGE(PH_TAG, "httpd_ws_send_frame failed with %s", esp_err_to_name(ret));
  // ESP_LOGD(PH_TAG, "ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d",
  //   request->server(),
  //   httpd_req_to_sockfd(request->request()),
  //   httpd_ws_get_fd_info(request->server()->server, httpd_req_to_sockfd(request->request())));

#ifdef PSYCHIC_WS_RX_STATIC_BUFFER
  if (ws_rx_locked) xSemaphoreGive(s_ws_rx_mutex);
#else
  // dont forget to release our buffer memory
  free(buf);
#endif

  return ret;
}

PsychicWebSocketHandler* PsychicWebSocketHandler::onOpen(PsychicWebSocketClientCallback fn)
{
  _onOpen = fn;
  return this;
}

PsychicWebSocketHandler* PsychicWebSocketHandler::onFrame(PsychicWebSocketFrameCallback fn)
{
  _onFrame = fn;
  return this;
}

PsychicWebSocketHandler* PsychicWebSocketHandler::onClose(PsychicWebSocketClientCallback fn)
{
  _onClose = fn;
  return this;
}

void PsychicWebSocketHandler::sendAll(httpd_ws_frame_t* ws_pkt)
{
  for (PsychicClient* client : _clients) {
    // ESP_LOGD(PH_TAG, "Active client (fd=%d) -> sending async message", client->socket());

    if (client->_friend == NULL) {
      continue;
    }

    // A failed send (socket error, or a full per-client queue when backpressure
    // is enabled) only concerns that one client — keep broadcasting to the rest.
    if (((PsychicWebSocketClient*)client->_friend)->sendMessage(ws_pkt) != ESP_OK)
      continue;
  }
}

void PsychicWebSocketHandler::sendAll(httpd_ws_type_t op, const void* data, size_t len)
{
  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

  ws_pkt.payload = (uint8_t*)data;
  ws_pkt.len = len;
  ws_pkt.type = op;

  this->sendAll(&ws_pkt);
}

void PsychicWebSocketHandler::sendAll(const char* buf)
{
  this->sendAll(HTTPD_WS_TYPE_TEXT, buf, strlen(buf));
}
