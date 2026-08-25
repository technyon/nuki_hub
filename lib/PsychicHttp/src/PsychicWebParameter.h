#ifndef PsychicWebParameter_h
#define PsychicWebParameter_h

#include <string>

/*
 * PARAMETER :: Chainable object to hold GET/POST and FILE parameters
 * */

class PsychicWebParameter
{
  private:
    std::string _name;
    std::string _value;
    size_t _size;
    bool _isForm;
    bool _isFile;

  public:
    PsychicWebParameter(const char* name, const char* value, bool form = false, bool file = false, size_t size = 0) : _name(name), _value(value), _size(size), _isForm(form), _isFile(file) {}
#ifdef ARDUINO
    PsychicWebParameter(const String& name, const String& value, bool form = false, bool file = false, size_t size = 0) : _name(name.c_str()), _value(value.c_str()), _size(size), _isForm(form), _isFile(file) {}
#endif
#ifdef ARDUINO
    String name() const { return String(_name.c_str()); }
    String value() const { return String(_value.c_str()); }
#else
    const char* name() const { return _name.c_str(); }
    const char* value() const { return _value.c_str(); }
#endif
    // Always returns const char* regardless of platform — use in library internals.
    const char* nameCStr() const { return _name.c_str(); }
    const char* valueCStr() const { return _value.c_str(); }
    size_t size() const { return _size; }
    bool isPost() const { return _isForm; }
    bool isFile() const { return _isFile; }
};

#endif // PsychicWebParameter_h