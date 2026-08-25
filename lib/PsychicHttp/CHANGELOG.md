# v2.0

* Modified the request handling to bring initial url matching and filtering into PsychicHttpServer itself.
    * Fixed a bug with filter() where endpoint is matched, but filter fails and it doesn't continue matching further endpoints on same uri (checks were in different codebases)
    * HTTP_ANY support
    * unlimited endpoints (no more need to manually set config.max_uri_handlers)
    * much more flexibility for future
* Endpoint Matching Updates
    * Endpoint matching functions can be set on server level (```server.setURIMatchFunction()```) or endpoint level (```endpoint.setURIMatchFunction()```)
    * Added convenience macros MATCH_SIMPLE, MATCH_WILDCARD, and MATCH_REGEX
    * Added regex matching of URIs, enable it with define PSY_ENABLE_REGEX
    * On regex matched requests, you can get match data with request->getRegexMatches()
* Ported URL rewrite functionality from ESPAsyncWS

## Changes required from v1.x to v2.0:

* add a ```server.begin()``` or ```server.start()``` after all your ```server.on()``` calls
* remove any calls to ```config.max_uri_handlers```
* if you are using a custom ```server.config.uri_match_fn``` to match uris, change it to ```server.setURIMatchFunction()```

# v1.2.1

* Fix bug with missing include preventing the HTTPS server from compiling.

# v1.2

* Added TemplatePrinter from https://github.com/Chris--A/PsychicHttp/tree/templatePrint
* Support using as ESP IDF component
* Optional using https server in ESP IDF
* Fixed bug with headers
* Add ESP IDF example + CI script
* Added Arduino Captive Portal example and OTAUpdate from @06GitHub
* HTTPS fix for ESP-IDF v5.0.2+ from @06GitHub
* lots of bugfixes from @mathieucarbou

Thanks to @Chris--A, @06GitHub, and @dzungpv for your contributions.

# v1.1

* Changed the internal structure to support request handlers on endpoints and generic requests that do not match an endpoint
    * websockets, uploads, etc should now create an appropriate handler and attach to an endpoint with the server.on() syntax
* Added PsychicClient to abstract away some of the internals of ESP-IDF sockets + add convenience
    * onOpen and onClose callbacks have changed as a result
* Added support for EventSource / SSE
* Added support for multipart file uploads
* changed getParam() to return a PsychicWebParameter in line with ESPAsyncWebserver
* Renamed various classes / files:
    * PsychicHttpFileResponse -> PsychicFileResponse
    * PsychicHttpServerEndpoint -> PsychicEndpoint
    * PsychicHttpServerRequest -> PsychicRequest
    * PsychicHttpServerResponse -> PsychicResponse
    * PsychicHttpWebsocket.h -> PsychicWebSocket.h
    * Websocket => WebSocket
* Quite a few bugfixes from the community. Thank you @glennsky, @gb88, @KastanEr, @kstam, and @zekageri