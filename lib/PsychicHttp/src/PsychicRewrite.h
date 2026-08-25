#ifndef PsychicRewrite_h
#define PsychicRewrite_h

#include "PsychicCore.h"

/*
 * REWRITE :: One instance can be handle any Request (done by the Server)
 * */

class PsychicRewrite
{
  protected:
    std::string _fromPath;
    std::string _toUri;
    std::string _toPath;
    std::string _toParams;
    PsychicRequestFilterFunction _filter;

  public:
    PsychicRewrite(const char* from, const char* to);
    virtual ~PsychicRewrite();

    PsychicRewrite* setFilter(PsychicRequestFilterFunction fn);
    bool filter(PsychicRequest* request) const;
#ifdef ARDUINO
    String from(void) const;
    String toUrl(void) const;
    String params(void) const;
#else
    const char* from(void) const;
    const char* toUrl(void) const;
    const char* params(void) const;
#endif
    // Always returns const char* regardless of platform — use in library internals.
    const char* toUrlCStr(void) const;
    virtual bool match(PsychicRequest* request);
};

#endif