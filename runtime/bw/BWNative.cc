/* Copyright 2016 buwai */

#include "InstrumentationExt.h"
#include "runtime.h"
#include "BWLog.h"
#include "BWNative.h"

void* GetInstrumentationExt() {
    return art::Runtime::Current()->GetInstrumentationExt();
}

bool InsertOrUpdateTraceMethodInfo(void* traceMethodInfo) {
    Lsp<TraceMethodInfoBase>* tmp = static_cast< Lsp<TraceMethodInfoBase>* >(traceMethodInfo);
    // BWLOGI("[*] BWNative.cpp InsertOrUpdateTraceMethodInfo - tmp=%p, traceMethodInfo=%p, hash=%d",
    //         tmp, traceMethodInfo, (*tmp)->methodLocation->methodIDBase->hash);
    // BWLOGI("[*] BWNative.cpp InsertOrUpdateTraceMethodInfo - methodLocation addr=%p, "
    //         "methodIDBase addr=%p, hash addr=%p",
    //         (*tmp)->methodLocation.Get(), (*tmp)->methodLocation->methodIDBase.Get(),
    //         &((*tmp)->methodLocation->methodIDBase->hash));
    // BWLOGI("[*] BWNative.cpp InsertOrUpdateTraceMethodInfo - sizeof(std::string)=%u,"
    //     "sizeof(CString)=%u", (unsigned)sizeof(std::string), (unsigned)sizeof(CString));
    return art::Runtime::Current()->GetInstrumentationExt()->
        InsertOrUpdateTraceMethodInfo(*tmp);
}

bool DeleteTraceMethodInfo(int hash) {
    return art::Runtime::Current()->GetInstrumentationExt()->
        DeleteTraceMethodInfo(hash);
}

bool QueryTraceMethodInfo(int hash, void* traceMethodInfo) {
    Lsp<TraceMethodInfoBase>* tmp = static_cast< Lsp<TraceMethodInfoBase>* >(traceMethodInfo);
    return art::Runtime::Current()->GetInstrumentationExt()->
        QueryTraceMethodInfo(hash, *tmp);
}

bool InsertOrUpdateHookMethodInstInfo(void* hookMethodInstInfo) {
    Lsp<HookMethodInstInfoBase>* tmp = static_cast< Lsp<HookMethodInstInfoBase>* >(hookMethodInstInfo);
    return art::Runtime::Current()->GetInstrumentationExt()->
        InsertOrUpdateHookMethodInstInfo(*tmp);
}

unsigned int DeleteHookMethodInstInfo(int hash, int64_t instLineNum) {
    return art::Runtime::Current()->GetInstrumentationExt()->
        DeleteHookMethodInstInfo(hash, instLineNum);
}

unsigned int DeleteHookMethodInstInfoInMethod(int hash) {
    return art::Runtime::Current()->GetInstrumentationExt()->
        DeleteHookMethodInstInfo(hash);
}

unsigned int DeleteHookMethodInstInfoInPackage() {
    return art::Runtime::Current()->GetInstrumentationExt()->
        DeleteHookMethodInstInfo();
}

bool QueryHookMethodInstInfoInMethod(int hash, void* hookMethodInstInfos) {
    return art::Runtime::Current()->GetInstrumentationExt()->
        QueryHookMethodInstInfo(hash, (std::vector< Lsp<HookMethodInstInfoBase> >*)(hookMethodInstInfos));
}

bool QueryHookMethodInstInfo(int hash, int64_t instLineNum, void* hookMethodInstInfo) {
    Lsp<HookMethodInstInfoBase>* tmp = static_cast< Lsp<HookMethodInstInfoBase>* >(hookMethodInstInfo);
    (*tmp) = art::Runtime::Current()->GetInstrumentationExt()->QueryHookMethodInstInfo(hash, instLineNum);
    return tmp->IsEmpty();
}

bool SetBWDumpFlags(int flags) {
    return art::Runtime::Current()->GetInstrumentationExt()->SetBWDumpFlags(flags);
}

int GetBWDumpFlags() {
    return art::Runtime::Current()->GetInstrumentationExt()->GetBWDumpFlags();
}

bool InitWhenChildStart(void* initParams, uint32_t arraySize) {
    return art::Runtime::Current()->GetInstrumentationExt()->
        InitWhenChildStart(static_cast<SimpleKeyValuePair*>(initParams), arraySize);
}
