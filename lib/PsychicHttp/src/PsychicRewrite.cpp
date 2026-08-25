#include "PsychicRewrite.h"
#include "PsychicRequest.h"

PsychicRewrite::PsychicRewrite(const char* from, const char* to) : _fromPath(from),
                                                                   _toUri(to),
                                                                   _toPath(""),
                                                                   _toParams(""),
                                                                   _filter(nullptr)
{
  size_t index = _toUri.find('?');
  if (index != std::string::npos) {
    _toParams = _toUri.substr(index + 1);
    _toPath = _toUri.substr(0, index);
  } else
    _toPath = _toUri;
}

PsychicRewrite::~PsychicRewrite()
{
}

PsychicRewrite* PsychicRewrite::setFilter(PsychicRequestFilterFunction fn)
{
  _filter = fn;
  return this;
}

bool PsychicRewrite::filter(PsychicRequest* request) const
{
  return _filter == nullptr || _filter(request);
}

#ifdef ARDUINO
String PsychicRewrite::from(void) const
{
  return String(_fromPath.c_str());
}
String PsychicRewrite::toUrl(void) const
{
  return String(_toUri.c_str());
}
String PsychicRewrite::params(void) const
{
  return String(_toParams.c_str());
}
#else
const char* PsychicRewrite::from(void) const
{
  return _fromPath.c_str();
}
const char* PsychicRewrite::toUrl(void) const
{
  return _toUri.c_str();
}

const char* PsychicRewrite::params(void) const
{
  return _toParams.c_str();
}
#endif

const char* PsychicRewrite::toUrlCStr(void) const
{
  return _toUri.c_str();
}

bool PsychicRewrite::match(PsychicRequest* request)
{
  if (!filter(request))
    return false;

  return _fromPath == request->pathCStr();
}