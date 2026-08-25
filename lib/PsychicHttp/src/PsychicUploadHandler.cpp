#include "PsychicUploadHandler.h"

PsychicUploadHandler::PsychicUploadHandler() : PsychicWebHandler(), _uploadCallback(nullptr)
{
}
PsychicUploadHandler::~PsychicUploadHandler() {}

bool PsychicUploadHandler::canHandle(PsychicRequest* request)
{
  return true;
}

esp_err_t PsychicUploadHandler::handleRequest(PsychicRequest* request, PsychicResponse* response)
{
  esp_err_t err = ESP_OK;

  /* File cannot be larger than a limit */
  if (request->contentLength() > request->server()->maxUploadSize)
  {
    ESP_LOGE(PH_TAG, "File too large : %d bytes", request->contentLength());

    /* Respond with 400 Bad Request */
    char error[50];
    sprintf(error, "File size must be less than %lu bytes!", request->server()->maxUploadSize);
    return httpd_resp_send_err(request->request(), HTTPD_400_BAD_REQUEST, error);
  }

  // TODO: support for the 100 header.  not sure if we can do it.
  //  if (request->header("Expect").equals("100-continue"))
  //  {
  //    char response[] = "100 Continue";
  //    httpd_socket_send(self->server, httpd_req_to_sockfd(req), response, strlen(response), 0);
  //  }

  // 2 types of upload requests
  if (request->isMultipart())
    err = _multipartUploadHandler(request);
  else
    err = _basicUploadHandler(request);

  // we can also call onRequest for some final processing and response
  if (err == ESP_OK)
  {
    if (_requestCallback != NULL)
      err = _requestCallback(request, response);
    else
      err = response->send("Upload Successful.");
  }
  else if (err == ESP_ERR_HTTPD_INVALID_REQ)
    response->send(400, "text/html", "No multipart boundary found.");
  else
    response->send(500, "text/html", "Error processing upload.");

  return err;
}

esp_err_t PsychicUploadHandler::_basicUploadHandler(PsychicRequest* request)
{
  esp_err_t err = ESP_OK;

  String filename = request->getFilename();

  /* Retrieve the pointer to scratch buffer for temporary storage */
  char* buf = (char*)malloc(FILE_CHUNK_SIZE);
  int received;
  unsigned long index = 0;

  /* Content length of the request gives the size of the file being uploaded */
  int remaining = request->contentLength();

  while (remaining > 0)
  {
#ifdef ENABLE_ASYNC
    httpd_sess_update_lru_counter(request->server()->server, request->client()->socket());
#endif

    // ESP_LOGD(PH_TAG, "Remaining size : %d", remaining);

    /* Receive the file part by part into a buffer */
    if ((received = httpd_req_recv(request->request(), buf, min(remaining, FILE_CHUNK_SIZE))) <= 0)
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

    // call our upload callback here.
    if (_uploadCallback != NULL)
    {
      err = _uploadCallback(request, filename, index, (uint8_t*)buf, received, (remaining - received == 0));
      if (err != ESP_OK)
        break;
    }
    else
    {
      ESP_LOGE(PH_TAG, "No upload callback specified!");
      err = ESP_FAIL;
      break;
    }

    /* Keep track of remaining size of the file left to be uploaded */
    remaining -= received;
    index += received;
  }

  // dont forget to free our buffer
  free(buf);

  return err;
}

esp_err_t PsychicUploadHandler::_multipartUploadHandler(PsychicRequest* request)
{
  MultipartProcessor mpp(request, _uploadCallback);
  return mpp.process();
}

PsychicUploadHandler* PsychicUploadHandler::onUpload(PsychicUploadCallback fn)
{
  _uploadCallback = fn;
  return this;
}
