# PsychicHandler

- [x] create addMiddleware()
- [x] create runMiddleware()
- [ ] move all the handler::canHandle() stuff into filter();
  - [ ] canHandle should be declared static

# PsychicEndpoint

- [ ] convert setAuthentication() to add AuthMiddleware instead.

## PsychicHttpServer

- [ ] add _chain
- [ ] create addMiddleware()
- [ ] create runMiddleware()
- [ ] create removeMiddleware(name)
- [ ] _filters -> _middleware
- [ ] destructor / cleanup

# PsychicRequest

- [ ] add _response pointer to PsychicRequest, created in constructor
- [ ] request->beginReply() should return existing _response pointer
- [ ] requestAuthentication() -> should move to response?

# PsychicResponse

- how do we have extended classes when we have a pre-declared base PsychicResponse object?
  - the delegation style is really ugly and causes problems with inheritance
