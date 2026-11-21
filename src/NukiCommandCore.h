#pragma once

#include <Preferences.h>
#include <list>
#include <vector>
#include "NukiBle.h"
#include "NukiNetworkEntries.h"
#include "util/NukiRetryHandler.h"

struct NukiCommandContext
{
    void* _self;
    bool (*_isPinValid)(void* self);
    bool (*_hasKeypad)(void* self);
    void (*_updateKeypad)(void* self, bool retrieved);
    void (*_updateAuth)(void* self, bool retrieved);
    Nuki::NukiBle& _device;
    NukiNetworkEntries* _network;
    NukiRetryHandler* _nukiRetryHandler;
    Preferences* _preferences;
    std::vector<uint16_t>& _keypadCodeIds;
    std::vector<uint32_t>& _keypadCodes;
    std::vector<uint32_t>& _authIds;
    int& _nrOfRetries;
    int& _retryDelay;
    int& _invalidCount;
    int64_t& _lastCodeCheck;
    bool& _nukiConfigValid;
    bool& _keypadEnabled;
    bool& _keypadCacheDirty;
    bool& _disableNonJSON;
    int64_t& _nextKeypadUpdateTs;
    std::list<Nuki::KeypadEntry>& _keypadEntriesCache;
    std::vector<std::pair<uint16_t, int64_t>>& _keypadDeleted;

    bool isPinValid()
    {
        return _isPinValid(_self);
    }
    bool hasKeypad()
    {
        return _hasKeypad(_self);
    }
    void updateKeypad(bool retrieved)
    {
        _updateKeypad(_self, retrieved);
    }
    void updateAuth(bool retrieved)
    {
        _updateAuth(_self, retrieved);
    }
    Nuki::NukiBle& nukiDevice()
    {
        return _device;
    }
};

namespace NukiCommandCore
{
void executeKeypadJsonCommand(NukiCommandContext& d, const char *value);
void executeAuthCommand(NukiCommandContext& d, const char *value);
void executeKeypadCommand(NukiCommandContext& d, const char *command, const uint &id, const String &name, const String &code, const int& enabled);
}
