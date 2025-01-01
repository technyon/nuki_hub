#ifndef PsychicRewrite_h
#define PsychicRewrite_h

#include "PsychicCore.h"

/*
 * REWRITE :: One instance can be handle any Request (done by the Server)
 * */

class PsychicRewrite {
  protected:
    String _fromPath;
    String _toUri;
    String _toPath;
    String _toParams;
    PsychicRequestFilterFunction _filter;

  public:
    PsychicRewrite(const char* from, const char* to);
    virtual ~PsychicRewrite();

    PsychicRewrite* setFilter(PsychicRequestFilterFunction fn);
    bool filter(PsychicRequest *request) const;
    const String& from(void) const;
    const String& toUrl(void) const;
    const String& params(void) const;
    virtual bool match(PsychicRequest *request);
};

#endif