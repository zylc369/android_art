/* Copyright 2016 buwai */

#ifndef ART_RUNTIME_BW_BWARTUTILS_H_
#define ART_RUNTIME_BW_BWARTUTILS_H_

#include <string>

#include "stack.h"
#include "dex_instruction.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "mirror/art_method.h"
#include "mirror/art_method-inl.h"
#include "method_helper.h"
#include <BWNativeHelper/BWCommon.h>

namespace art {

/**
 * 将指令机器码转换为易读的指令。
 */
std::string InstToString(const ShadowFrame& shadow_frame, const Instruction* inst,
    const uint32_t dex_pc, MethodHelper& mh) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

/**
 * 打印方法的调用堆栈。
 */
void PrintCallStack(Thread* self);

}  // namespace art

#endif  // ART_RUNTIME_BW_BWARTUTILS_H_
