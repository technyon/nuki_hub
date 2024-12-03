// ArduinoJson - https://arduinojson.org
// Copyright © 2014-2024, Benoit BLANCHON
// MIT License

#include <Arduino.h>

#include <ArduinoJson/Strings/IsString.hpp>
#include <ArduinoJson/Strings/StringAdapters.hpp>

#include <catch.hpp>

#include "custom_string.hpp"
#include "weird_strcmp.hpp"

using ArduinoJson::JsonString;
using namespace ArduinoJson::detail;

TEST_CASE("adaptString()") {
  SECTION("null const char*") {
    auto s = adaptString(static_cast<const char*>(0));

    CHECK(s.isNull() == true);
    CHECK(s.size() == 0);
    CHECK(s.isLinked() == true);
  }

  SECTION("non-null const char*") {
    auto s = adaptString("bravo");

    CHECK(s.isNull() == false);
    CHECK(s.size() == 5);
    CHECK(s.isLinked() == true);
  }

  SECTION("null const char* + size") {
    auto s = adaptString(static_cast<const char*>(0), 10);

    CHECK(s.isNull() == true);
    CHECK(s.isLinked() == false);
  }

  SECTION("non-null const char* + size") {
    auto s = adaptString("bravo", 5);

    CHECK(s.isNull() == false);
    CHECK(s.size() == 5);
    CHECK(s.isLinked() == false);
  }

  SECTION("null Flash string") {
    auto s = adaptString(static_cast<const __FlashStringHelper*>(0));

    CHECK(s.isNull() == true);
    CHECK(s.size() == 0);
    CHECK(s.isLinked() == false);
  }

  SECTION("non-null Flash string") {
    auto s = adaptString(F("bravo"));

    CHECK(s.isNull() == false);
    CHECK(s.size() == 5);
    CHECK(s.isLinked() == false);
  }

  SECTION("std::string") {
    std::string orig("bravo");
    auto s = adaptString(orig);

    CHECK(s.isNull() == false);
    CHECK(s.size() == 5);
    CHECK(s.isLinked() == false);
  }

  SECTION("Arduino String") {
    ::String orig("bravo");
    auto s = adaptString(orig);

    CHECK(s.isNull() == false);
    CHECK(s.size() == 5);
    CHECK(s.isLinked() == false);
  }

  SECTION("custom_string") {
    custom_string orig("bravo");
    auto s = adaptString(orig);

    CHECK(s.isNull() == false);
    CHECK(s.size() == 5);
    CHECK(s.isLinked() == false);
  }

  SECTION("JsonString linked") {
    JsonString orig("hello", JsonString::Ownership::Linked);
    auto s = adaptString(orig);

    CHECK(s.isNull() == false);
    CHECK(s.size() == 5);
    CHECK(s.isLinked() == true);
  }

  SECTION("JsonString copied") {
    JsonString orig("hello", JsonString::Ownership::Copied);
    auto s = adaptString(orig);

    CHECK(s.isNull() == false);
    CHECK(s.size() == 5);
    CHECK(s.isLinked() == false);
  }
}

struct EmptyStruct {};

TEST_CASE("IsString<T>") {
  CHECK(IsString<std::string>::value == true);
  CHECK(IsString<std::basic_string<wchar_t>>::value == false);
  CHECK(IsString<custom_string>::value == true);
  CHECK(IsString<const __FlashStringHelper*>::value == true);
  CHECK(IsString<const char*>::value == true);
  CHECK(IsString<const char[8]>::value == true);
  CHECK(IsString<::String>::value == true);
  CHECK(IsString<::StringSumHelper>::value == true);
  CHECK(IsString<const EmptyStruct*>::value == false);
  CHECK(IsString<JsonString>::value == true);
}

TEST_CASE("stringCompare") {
  SECTION("ZeroTerminatedRamString vs ZeroTerminatedRamString") {
    CHECK(stringCompare(adaptString("bravo"), adaptString("alpha")) > 0);
    CHECK(stringCompare(adaptString("bravo"), adaptString("bravo")) == 0);
    CHECK(stringCompare(adaptString("bravo"), adaptString("charlie")) < 0);
  }

  SECTION("ZeroTerminatedRamString vs SizedRamString") {
    CHECK(stringCompare(adaptString("bravo"), adaptString("alpha?", 5)) > 0);
    CHECK(stringCompare(adaptString("bravo"), adaptString("bravo?", 4)) > 0);
    CHECK(stringCompare(adaptString("bravo"), adaptString("bravo?", 5)) == 0);
    CHECK(stringCompare(adaptString("bravo"), adaptString("bravo?", 6)) < 0);
    CHECK(stringCompare(adaptString("bravo"), adaptString("charlie?", 7)) < 0);
  }

  SECTION("SizedRamString vs SizedRamString") {
    // clang-format off
    CHECK(stringCompare(adaptString("bravo!", 5), adaptString("alpha?", 5)) > 0);
    CHECK(stringCompare(adaptString("bravo!", 5), adaptString("bravo?", 5)) == 0);
    CHECK(stringCompare(adaptString("bravo!", 5), adaptString("charlie?", 7)) < 0);

    CHECK(stringCompare(adaptString("bravo!", 5), adaptString("bravo!", 4)) > 0);
    CHECK(stringCompare(adaptString("bravo!", 5), adaptString("bravo!", 5)) == 0);
    CHECK(stringCompare(adaptString("bravo!", 5), adaptString("bravo!", 6)) < 0);
    // clang-format on
  }

  SECTION("FlashString vs FlashString") {
    // clang-format off
    CHECK(stringCompare(adaptString(F("bravo")), adaptString(F("alpha"))) > 0);
    CHECK(stringCompare(adaptString(F("bravo")), adaptString(F("bravo"))) == 0);
    CHECK(stringCompare(adaptString(F("bravo")), adaptString(F("charlie"))) < 0);
    // clang-format on
  }

  SECTION("FlashString vs SizedRamString") {
    // clang-format off
    CHECK(stringCompare(adaptString(F("bravo")), adaptString("alpha?", 5)) > 0);
    CHECK(stringCompare(adaptString(F("bravo")), adaptString("bravo?", 5)) == 0);
    CHECK(stringCompare(adaptString(F("bravo")), adaptString("charlie?", 7)) < 0);

    CHECK(stringCompare(adaptString(F("bravo")), adaptString("bravo!", 4)) > 0);
    CHECK(stringCompare(adaptString(F("bravo")), adaptString("bravo!", 5)) == 0);
    CHECK(stringCompare(adaptString(F("bravo")), adaptString("bravo!", 6)) < 0);
    // clang-format on
  }

  SECTION("ZeroTerminatedRamString vs FlashString") {
    // clang-format off
    CHECK(stringCompare(adaptString("bravo"), adaptString(F("alpha?"), 5)) > 0);
    CHECK(stringCompare(adaptString("bravo"), adaptString(F("bravo?"), 4)) > 0);
    CHECK(stringCompare(adaptString("bravo"), adaptString(F("bravo?"), 5)) == 0);
    CHECK(stringCompare(adaptString("bravo"), adaptString(F("bravo?"), 6)) < 0);
    CHECK(stringCompare(adaptString("bravo"), adaptString(F("charlie?"), 7)) < 0);
    // clang-format on
  }
}

TEST_CASE("stringEquals()") {
  SECTION("ZeroTerminatedRamString vs ZeroTerminatedRamString") {
    CHECK(stringEquals(adaptString("bravo"), adaptString("brav")) == false);
    CHECK(stringEquals(adaptString("bravo"), adaptString("bravo")) == true);
    CHECK(stringEquals(adaptString("bravo"), adaptString("bravo!")) == false);
  }

  SECTION("ZeroTerminatedRamString vs SizedRamString") {
    // clang-format off
    CHECK(stringEquals(adaptString("bravo"), adaptString("bravo!", 4)) == false);
    CHECK(stringEquals(adaptString("bravo"), adaptString("bravo!", 5)) == true);
    CHECK(stringEquals(adaptString("bravo"), adaptString("bravo!", 6)) == false);
    // clang-format on
  }

  SECTION("FlashString vs SizedRamString") {
    // clang-format off
    CHECK(stringEquals(adaptString(F("bravo")), adaptString("bravo!", 4)) == false);
    CHECK(stringEquals(adaptString(F("bravo")), adaptString("bravo!", 5)) == true);
    CHECK(stringEquals(adaptString(F("bravo")), adaptString("bravo!", 6)) == false);
    // clang-format on
  }

  SECTION("SizedRamString vs SizedRamString") {
    // clang-format off
    CHECK(stringEquals(adaptString("bravo?", 5), adaptString("bravo!", 4)) == false);
    CHECK(stringEquals(adaptString("bravo?", 5), adaptString("bravo!", 5)) == true);
    CHECK(stringEquals(adaptString("bravo?", 5), adaptString("bravo!", 6)) == false);
    // clang-format on
  }
}
