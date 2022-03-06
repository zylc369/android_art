/* Copyright 2016 buwai */

#include "stdafx.h"
#include "BWArtUtils.h"

namespace art {

std::string InstToString(const ShadowFrame& shadow_frame, const Instruction* inst,
    const uint32_t dex_pc, MethodHelper& mh) {
    std::ostringstream oss;

    // 生成方法名和smali指令。
    oss << PrettyMethod(shadow_frame.GetMethod())
        << StringPrintf("\n0x%x: ", dex_pc)
        << inst->DumpString(mh.GetMethod()->GetDexFile()) << "\n";

    // 生成寄存器信息。
    for (uint32_t i = 0; i < shadow_frame.NumberOfVRegs(); ++i) {
        uint32_t raw_value = shadow_frame.GetVReg(i);
        mirror::Object* ref_value = shadow_frame.GetVRegReference(i);
        oss << StringPrintf(" vreg%u=0x%08X", i, raw_value);
        if (ref_value != NULL) {
            if (ref_value->GetClass()->IsStringClass() &&
                ref_value->AsString()->GetCharArray() != NULL) {
                oss << "/java.lang.String \"" << ref_value->AsString()->ToModifiedUtf8() << "\"";
            } else {
                oss << "/" << PrettyTypeOf(ref_value);
            }
        }
    }
    return oss.str();
}

void PrintCallStack(Thread* self) {
  ScopedObjectAccess soa(self);
  mirror::ObjectArray<mirror::Object>* array = soa.Self()->CreateInternalStackTraceToArray<false>(soa);
  int32_t arrayLength = array->GetLength();
  /**
   数组长度减一的理由：
   该数组最后一个元素保存的是pc数组，详情可参考Thread::CreateInternalStackTraceToArray函数中的代码，
   在该函数中有语句：build_trace_visitor.Init(depth)。
   进入到BuildInternalStackTraceVisitor::Init函数，这个函数在文件art/runtime/thread.cc中，
   该函数中有这么一条语句：mirror::IntArray* dex_pc_trace = mirror::IntArray::Alloc(self_, depth);，其中depth变量在该语句中表示数组的一个索引，并且该值索引的是数组最后一个元素。

   在这里数组长度减一，就是为了不获得最后的pc数组，因为我需要的是方法调用堆栈信息，所以不需要pc数组中的数据。
   */
  arrayLength--;

  BWDUMPI("[*] ------ 开始 - 打印调用堆栈 ------");
  for (int32_t i = 0; i < arrayLength; i++) {
    mirror::Object* obj = array->Get(i);
    if (NULL == obj) {
      BWDUMPE("[-] %d. obj=%p", i, obj);
      continue;
    }
    mirror::ArtMethod* m = down_cast<mirror::ArtMethod*>(obj);
    std::string prettyMethod = PrettyMethod(m);
    BWDUMPI("[*] %s", prettyMethod.c_str());
  }
  BWDUMPI("[*] ------ 结束 - 打印调用堆栈 ------");
}

}  // namespace art
