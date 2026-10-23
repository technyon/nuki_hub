#pragma once

#include <ArduinoJson.h>
#include "esp_heap_caps.h"

struct PsramJsonAllocator : ArduinoJson::Allocator
{
    void* allocate(size_t size) override
    {
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
        return ptr != nullptr ? ptr : malloc(size);
    }

    void deallocate(void* ptr) override
    {
        heap_caps_free(ptr);
    }

    void* reallocate(void* ptr, size_t size) override
    {
        void* newPtr = heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM);
        return newPtr != nullptr ? newPtr : realloc(ptr, size);
    }
};
