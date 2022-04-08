#pragma once

class SpiffsCookie
{
public:
    SpiffsCookie();
    virtual ~SpiffsCookie() = default;

    void set();
    void clear();
    const bool isSet();

};
