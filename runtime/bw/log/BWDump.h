/* Copyright 2016 buwai */

#ifndef ART_RUNTIME_BW_LOG_BWDUMP_H_
#define ART_RUNTIME_BW_LOG_BWDUMP_H_

#include <stdio.h>
#include <stdarg.h>
#include <string>

#include <android/log.h>

#include "stack.h"
#include "dex_instruction.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "mirror/art_method.h"
#include "mirror/art_method-inl.h"
#include "method_helper.h"

namespace art {

/**
 * dump指令。
 */
class BWDump {
 public:
    static bool Dump(const ShadowFrame& shadow_frame, const Instruction* inst,
        const uint32_t dex_pc, MethodHelper& mh) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

    static bool Dump(int prio, const char* fmt, va_list ap);
    static bool Dump(int prio, const char* fmt, ...);

    static void PrintMethodInfo(ShadowFrame& shadow_frame) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
};

#ifndef BWDUMP
#define BWDUMP(priority, fmt, ...) \
    BWDump::Dump(ANDROID_##priority, fmt, ##__VA_ARGS__)
#endif

#define BWDUMPV(fmt, ...) \
    BWDUMP(LOG_VERBOSE, fmt, ##__VA_ARGS__)

#define BWDUMPD(fmt, ...) \
    BWDUMP(LOG_DEBUG, fmt, ##__VA_ARGS__)

#define BWDUMPI(fmt, ...) \
    BWDUMP(LOG_INFO, fmt, ##__VA_ARGS__)

#define BWDUMPW(fmt, ...) \
    BWDUMP(LOG_WARN, fmt, ##__VA_ARGS__)

#define BWDUMPE(fmt, ...) \
    BWDUMP(LOG_ERROR, fmt, ##__VA_ARGS__)

}  // namespace art

#endif  // ART_RUNTIME_BW_LOG_BWDUMP_H_
