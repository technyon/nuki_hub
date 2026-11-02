#pragma once

#include <stdint.h>

// All state kept in RTC memory across restarts (RTC_NOINIT_ATTR), shared by the
// main firmware and the updater, which hand restart reasons to each other.
//
// It must be a single object: as separate RTC_NOINIT globals the linker drops
// the ones a build does not use (e.g. the bootloop detection is not compiled
// into the updater) and shifts the others, so the two firmwares ended up
// reading and writing the same variable at different addresses. A struct has
// one fixed layout in both builds. Keep it free of #ifdefs for that reason, and
// only append new fields at the end.
#define RTC_STATE_RUNNING_DETECT 0x6e756b69

struct RtcNoInitState
{
    uint64_t restartReasonValidDetect;
    uint64_t bootloopValidDetect;
    int restartReason;
    int espRunning;
    int8_t bootloopCounter;
    bool rebuildGpioRequested;
    bool forceEnableWebServer;
    bool disableNetwork;
    bool wifiFallback;
    bool ethCriticalFailure;
};

extern RtcNoInitState rtcState;
