#pragma once

#include <cstdint>
#include "NukiBle.h"

// Keeps the Bluetooth connection to a Nuki device open for a while: after a command of a
// client (the linger time of the settings) or for as long as a client asked for on the topic
// connection/hold. The next command then finds the connection open and does not wait for the
// device to advertise first, which is most of the time a command after a pause takes.
//
// The device ends a connection that is silent for about ten seconds, so something small is
// exchanged every five. All of this costs the device battery while it lasts.
class NukiConnectionHold
{
public:
    void setLinger(int seconds)
    {
        _lingerMs = seconds > 0 ? (int64_t)(seconds > 600 ? 600 : seconds) * 1000 : 0;
    }

    // A client's command was taken up
    void touch(int64_t ts)
    {
        if(_lingerMs > 0 && ts + _lingerMs > _lingerUntil)
        {
            _lingerUntil = ts + _lingerMs;
        }
        // The command itself is traffic: nothing more is needed for the next five seconds
        _lastKeepAlive = ts;
    }

    // Asked for by a client: seconds from now
    void hold(int64_t ts, int seconds)
    {
        _holdUntil = seconds > 0 ? ts + (int64_t)(seconds > 3600 ? 3600 : seconds) * 1000 : 0;
        // Without a connection, open one now
        _lastOpen = ts - 30000;
    }

    // A lock action ends the linger: what follows it is read the way it always was. What a
    // client asked for stays.
    void endLinger()
    {
        _lingerUntil = 0;
    }

    bool active(int64_t ts) const
    {
        return ts < _lingerUntil || ts < _holdUntil;
    }

    // Called from the wrapper's update(), which runs between commands. Keeps an open
    // connection open. True when the state of the device is to be read to open one: only for
    // a connection a client asked for, and not more often than every 30 seconds. `kept` is
    // set when the device just answered a keep-alive (it is there, whatever it advertises).
    bool service(Nuki::NukiBle& device, int64_t ts, bool& kept)
    {
        kept = false;
        if(!active(ts))
        {
            return false;
        }
        if(!device.isConnected())
        {
            if(ts >= _holdUntil || ts - _lastOpen < 30000)
            {
                return false;
            }
            _lastOpen = ts;
            _lastKeepAlive = ts;
            return true;
        }
        device.holdConnection();
        if(ts - _lastKeepAlive >= 5000)
        {
            _lastKeepAlive = ts;
            kept = device.keepAlive();
        }
        return false;
    }

private:
    int64_t _lingerMs = 0;
    int64_t _lingerUntil = 0;
    int64_t _holdUntil = 0;
    int64_t _lastKeepAlive = 0;
    int64_t _lastOpen = 0;
};
