#include <ArduinoJson.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Make a copy to ensure the input is null-terminated
  std::string str(reinterpret_cast<const char*>(data), size);

  ArduinoJson::detail::parseNumber(str.c_str());

  return 0;
}
