/* Copyright 2016 buwai */

#include "stdafx.h"

#include <string.h>
#include <functional>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/casts.h"
#include "base/logging.h"
#include "base/scoped_flock.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "base/stringprintf.h"
#include "class_linker-inl.h"
#include "compiler_callbacks.h"
#include "debugger.h"
#include "dex_file-inl.h"
#include "gc_root-inl.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/accounting/heap_bitmap.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "handle_scope.h"
#include "intern_table.h"
#include "interpreter/interpreter.h"
#include "leb128.h"
#include "method_helper-inl.h"
#include "oat.h"
#include "oat_file.h"
#include "object_lock.h"
#include "mirror/art_field-inl.h"
#include "mirror/art_method-inl.h"
#include "mirror/class.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache-inl.h"
#include "mirror/iftable-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "mirror/proxy.h"
#include "mirror/reference-inl.h"
#include "mirror/stack_trace_element.h"
#include "mirror/string-inl.h"
#include "os.h"
#include "runtime.h"
#include "entrypoints/entrypoint_utils.h"
#include "ScopedLocalRef.h"
#include "scoped_thread_state_change.h"
#include "handle_scope-inl.h"
#include "thread.h"
#include "utils.h"
#include "verifier/method_verifier.h"
#include "well_known_classes.h"
#include "mapping_table.h"
#include "arch/context.h"

#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
#endif

#include "log/BWDump.h"
#include "InstrumentationExt.h"

#include <BWLog.h>

namespace art {

pthread_mutex_t gMutexBWInstLogFile = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t gMutexBWRegLogFile = PTHREAD_MUTEX_INITIALIZER;

//////////////////////////////////////////////////////////////////////////
// MethodPCCatch

MethodPCCatch::MethodPCCatch() : hash(0), instLineNum(-1), dexPC(-1) {}

MethodPCCatch& MethodPCCatch::operator=(const MethodPCCatch& other) {
    this->hash = other.hash;
    this->instLineNum = other.instLineNum;
    this->dexPC = other.dexPC;
    return *this;
}

bool MethodPCCatch::IsValid() {
    if (0 == hash || -1 == instLineNum || -1 == dexPC) {
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////
// DexPCCatch

int64_t DexPCCatch::GetPC(int hash, int64_t instLineNum, const DexFile::CodeItem* code_item) {
    std::vector<MethodPCCatch>* methodPCCatches;
    methodPCCatches = mMethodPCCatches[hash];
    if (NULL == methodPCCatches) {
        if (!InitMethodPCCatch(hash, code_item)) {
            BWDUMPE("[-] DexPCCatch::GetMethodPCCatch - 初始化MethodPCCatch失败。");
            return -1;
        }
        BWDUMPI("[*] DexPCCatch::GetMethodPCCatch - 初始化MethodPCCatch成功。");
        std::vector<MethodPCCatch>* methodPCCatches = mMethodPCCatches[hash];
        if (NULL == methodPCCatches) {
            BWDUMPE("[-] DexPCCatch::GetMethodPCCatch - methodPCCatches == NULL这不科学，因为初始化已经成功。");
            return -1;
        }
    }

    if ((size_t)instLineNum >= methodPCCatches->size()) {
        BWDUMPE("[-] DexPCCatch::GetMethodPCCatch - 数组越界异常。行号：%ld，数组长度：%lu。",
            instLineNum, static_cast<uint64_t>(methodPCCatches->size()));
        return -1;
    }
    MethodPCCatch m = (*methodPCCatches)[instLineNum];
    return m.dexPC;
}

bool DexPCCatch::InitMethodPCCatch(int hash, const DexFile::CodeItem* code_item) {
    if (code_item->insns_size_in_code_units_ <= 0) {
        return false;
    }

    std::vector<MethodPCCatch>* methodPCCatches = new std::vector<MethodPCCatch>();
    int64_t count = 0;
    uint32_t dexPC = 0;
    const uint16_t* const insns = code_item->insns_;
    const Instruction* inst = Instruction::At(insns + dexPC);

    while (dexPC < code_item->insns_size_in_code_units_) {
        size_t sizeInCodeUnits = inst->SizeInCodeUnits();
        dexPC += sizeInCodeUnits;
        inst = Instruction::At(insns + dexPC);
        MethodPCCatch m;
        m.instLineNum = count;
        m.dexPC = dexPC;
        methodPCCatches->push_back(m);
        count++;
    }
    mMethodPCCatches[hash] = methodPCCatches;
    return true;
}

//////////////////////////////////////////////////////////////////////////
// InstrumentationExt

InstrumentationExt::InstrumentationExt()
    : mMainThreadID(-1), mUID(-1), mBWInstLogFile(NULL), mBWRegLogFile(NULL),
      mBWDumpFlags(BWDumpBase::BW_DUMP_FLAGS_ALL) {
    mBWMethodFilter = new BWMethodFilter();
}

InstrumentationExt::~InstrumentationExt() {
    CloseAllFiles();
}

// 生成方法哈希。
int InstrumentationExt::GenMethodHash(mirror::ArtMethod& method) {
    std::string str = method.GetDeclaringClassDescriptor();
    str += method.GetName();
    str += method.GetSignature().ToString();
    int hash = HashCode(str.c_str(), str.size());
    return hash;
}

// 是否跟踪。
bool InstrumentationExt::IsTrace(mirror::ArtMethod& method, Lsp<TraceMethodInfoBase>& traceInfo) {
    int methodHash = InstrumentationExt::GenMethodHash(method);
    Lsp<TraceMethodInfoBase> traceMethodInfoBase = mTraceMethodInfos[methodHash];
    if (traceMethodInfoBase.IsEmpty()) {
        return false;
    }
    traceInfo = traceMethodInfoBase;
    return !(traceInfo.IsEmpty());
}

bool InstrumentationExt::InsertOrUpdateTraceMethodInfo(Lsp<TraceMethodInfoBase> traceMethodInfoBase) {
    if (traceMethodInfoBase.IsEmpty()) {
        BWDUMPE("[-] InstrumentationExt::InsertOrUpdateTraceMethodInfo - traceMethodInfoBase为空。");
        return false;
    }
    // BWLOGI("[*] InstrumentationExt::InsertOrUpdateTraceMethodInfo - 1. hash=%d",
    //     traceMethodInfoBase->methodLocation->methodIDBase->hash);
    Lsp<MethodIDBase> methodIDBase = traceMethodInfoBase->methodLocation->methodIDBase;
    // BWLOGI("[*] InstrumentationExt::InsertOrUpdateTraceMethodInfo - 2. hash=%d", methodIDBase->hash);
    Lsp<TraceMethodInfoBase> oldObj = mTraceMethodInfos[methodIDBase->hash];
    if (oldObj.IsEmpty()) {
        // BWLOGI("[*] InstrumentationExt::InsertOrUpdateTraceMethodInfo - 插入方法。hash=%d", methodIDBase->hash);
        mTraceMethodInfos[methodIDBase->hash] = (Lsp<TraceMethodInfoBase>) traceMethodInfoBase->Clone();
        return true;
    }

    if (oldObj->Equals(traceMethodInfoBase)) {
        BWDUMPE("[-] InstrumentationExt::InsertOrUpdateTraceMethodInfo - 插入或更新一个重复的数据。");
        return false;
    }

    // BWLOGI("[*] InstrumentationExt::InsertOrUpdateTraceMethodInfo - 更新方法。hash=%d", methodIDBase->hash);
    mTraceMethodInfos[methodIDBase->hash] = (Lsp<TraceMethodInfoBase>) traceMethodInfoBase->Clone();
    return true;
}

bool InstrumentationExt::DeleteTraceMethodInfo(int hash) {
    Lsp<TraceMethodInfoBase> p = mTraceMethodInfos[hash];
    if (p.IsEmpty()) {
        return false;
    }
    mTraceMethodInfos.erase(hash);
    return true;
}

bool InstrumentationExt::QueryTraceMethodInfo(int hash, Lsp<TraceMethodInfoBase>& traceMethodInfoBase) {
    // BWLOGI("[*] InstrumentationExt::QueryTraceMethodInfo - hash=%d", hash);
    Lsp<TraceMethodInfoBase> o = mTraceMethodInfos[hash];
    if (o.IsEmpty()) {
        // BWLOGI("[*] InstrumentationExt::QueryTraceMethodInfo - 未查到跟踪方法信息。");
        return false;
    }
    traceMethodInfoBase = (Lsp<TraceMethodInfoBase>) mTraceMethodInfos[hash]->Clone();
    // BWLOGI("[*] InstrumentationExt::QueryTraceMethodInfo - 是否查询到跟踪方法信息：%d。",
    //     traceMethodInfoBase.IsEmpty());
    return !(traceMethodInfoBase.IsEmpty());
}

std::map<int, Lsp<TraceMethodInfoBase>>* InstrumentationExt::GetTraceMethodInfos() {
    return &mTraceMethodInfos;
}

bool InstrumentationExt::InsertOrUpdateHookMethodInstInfo(Lsp<HookMethodInstInfoBase> hookMethodInstInfoBase) {
    if (hookMethodInstInfoBase.IsEmpty()) {
        BWDUMPE("[-] InstrumentationExt::InsertOrUpdateHookMethodInstInfo - hookMethodInstInfoBase为空。");
        return false;
    }
    Lsp<InstructionLocation> instructionLocation = hookMethodInstInfoBase->instructionLocation;
    Lsp<MethodIDBase> methodIDBase = instructionLocation->methodIDBase;
    int64_t instLineNum = instructionLocation->instLineNum;

    Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > mapTmp = mHookMethodInstInfos[methodIDBase->hash];
    if (mapTmp.IsEmpty()) {
        Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > newMap = new std::map<int64_t, Lsp<HookMethodInstInfoBase>>;
        mHookMethodInstInfos[methodIDBase->hash] = newMap;
        // TODO: *(newMap.Get())这种写法感觉不好。
        (*(newMap.Get()))[instLineNum] = (Lsp<HookMethodInstInfoBase>) hookMethodInstInfoBase->Clone();
        return true;
    }

    // TODO: *(mapTmp.Get())这种写法感觉不好。
    Lsp<HookMethodInstInfoBase> oldObj = (*(mapTmp.Get()))[instLineNum];
    if (oldObj.IsEmpty()) {
        (*(mapTmp.Get()))[instLineNum] = (Lsp<HookMethodInstInfoBase>) hookMethodInstInfoBase->Clone();
        return true;
    }

    if (oldObj->Equals(hookMethodInstInfoBase)) {
        BWDUMPE("[-] InstrumentationExt::InsertOrUpdateHookMethodInstInfo - 插入或更新一个重复的数据。");
        return false;
    }

    (*(mapTmp.Get()))[instLineNum] = (Lsp<HookMethodInstInfoBase>) hookMethodInstInfoBase->Clone();
    return true;
}

// 删除哈希对应的方法中的某个Hook指令信息。
unsigned int InstrumentationExt::DeleteHookMethodInstInfo(int hash, int64_t instLineNum) {
    Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > tmp = mHookMethodInstInfos[hash];
    if (tmp.IsEmpty()) {
        BWDUMPW("[!] InstrumentationExt::DeleteHookMethodInstInfo(int, int64_t) - "
            "1. 数据不存在，无法删除。hash=%zu，instLineNum=%ld", hash, instLineNum);
        return 0;
    }
    // TODO: *(tmp.Get())这种写法感觉不好。
    Lsp<HookMethodInstInfoBase> hookMethodInstInfoBase = (*(tmp.Get()))[instLineNum];
    if (hookMethodInstInfoBase.IsEmpty()) {
        BWDUMPW("[!] InstrumentationExt::DeleteHookMethodInstInfo(int, int64_t) - "
            "2. 数据不存在，无法删除。hash=%zu，instLineNum=%ld", hash, instLineNum);
        return 0;
    }
    tmp->erase(instLineNum);

    if (0 == tmp->size()) {
        mHookMethodInstInfos.erase(hash);
    }
    return 1;
}

// 删除哈希对应的方法中所有的Hook指令信息。
unsigned int InstrumentationExt::DeleteHookMethodInstInfo(int hash) {
    Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > tmp = mHookMethodInstInfos[hash];
    if (tmp.IsEmpty()) {
        BWDUMPW("[!] InstrumentationExt::DeleteHookMethodInstInfo(int) - "
            "数据不存在，无法删除。hash=%zu", hash);
        return 0;
    }
    mHookMethodInstInfos.erase(hash);
    unsigned int count = 0;
    std::map<int64_t, Lsp<HookMethodInstInfoBase>>::iterator it = tmp->begin();
    for (; it != tmp->end();) {
        tmp->erase(it++);
        count++;
    }
    return count;
}

// 删除所有的Hook指令信息。
unsigned int InstrumentationExt::DeleteHookMethodInstInfo() {
    unsigned int count = 0;
    std::map<int, Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > >::iterator it1 = mHookMethodInstInfos.begin();
    for (; it1 != mHookMethodInstInfos.end();) {
        Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > hookMethodInstInfos = it1->second;
        std::map<int64_t, Lsp<HookMethodInstInfoBase>>::iterator it2 = hookMethodInstInfos->begin();
        for (; it2 != hookMethodInstInfos->end();) {
            hookMethodInstInfos->erase(it2++);
            count++;
        }
        mHookMethodInstInfos.erase(it1++);
    }
    return count;
}

// 查询指定方法的Hook指令信息。
bool InstrumentationExt::QueryHookMethodInstInfo(int hash, std::map<int64_t, Lsp<HookMethodInstInfoBase>>& hookMethodInstInfos) {
    Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > tmp = mHookMethodInstInfos[hash];
    if (tmp.IsEmpty()) {
        // BWDUMPW("[!] InstrumentationExt::QueryHookMethodInstInfo(int, std::map<int64_t, Lsp<HookMethodInstInfoBase>>&) - "
        //     "数据不存在。hash=%zu", hash);
        return false;
    }

    std::map<int64_t, Lsp<HookMethodInstInfoBase>>::iterator it = tmp->begin();
    for (; it != tmp->end(); ++it) {
        hookMethodInstInfos[it->first] = (Lsp<HookMethodInstInfoBase>) it->second->Clone();
    }
    return true;
}

// 查询指定方法的Hook指令信息。
bool InstrumentationExt::QueryHookMethodInstInfo(int hash, std::vector<Lsp<HookMethodInstInfoBase>>* hookMethodInstInfos) {
    Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > tmp = mHookMethodInstInfos[hash];
    if (tmp.IsEmpty()) {
        // BWDUMPW("[!] InstrumentationExt::QueryHookMethodInstInfo(int, std::vector<Lsp<HookMethodInstInfoBase>>*) - "
        //     "数据不存在。hash=%zu", hash);
        return false;
    }
    std::map<int64_t, Lsp<HookMethodInstInfoBase>>::iterator it = tmp->begin();
    for (uint32_t i = 0; it != tmp->end(); ++it, ++i) {
        (*hookMethodInstInfos)[i] = (Lsp<HookMethodInstInfoBase>) it->second->Clone();
    }
    return true;
}

// 查询指定方法的指定行的Hook指令信息。
Lsp<HookMethodInstInfoBase> InstrumentationExt::QueryHookMethodInstInfo(int hash, int64_t instLineNum) {
    Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > tmp = mHookMethodInstInfos[hash];
    if (tmp.IsEmpty()) {
        // BWDUMPW("[!] InstrumentationExt::QueryHookMethodInstInfo(int, int64_t) - "
        //     "1. 数据不存在。hash=%zu, instLineNum=%ld", hash, instLineNum);
        return NULL;
    }
    Lsp<HookMethodInstInfoBase> h = (*(tmp.Get()))[instLineNum];
    if (h.IsEmpty()) {
        // BWDUMPW("[!] InstrumentationExt::QueryHookMethodInstInfo(int, int64_t) - "
        //     "2. 数据不存在。hash=%zu, instLineNum=%ld", hash, instLineNum);
        return Lsp<HookMethodInstInfoBase> (reinterpret_cast<HookMethodInstInfoBase*>(NULL));
    }
    return (Lsp<HookMethodInstInfoBase>) h->Clone();
}

std::map<int, Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > >* InstrumentationExt::GetHookMethodInstInfos() {
    return &mHookMethodInstInfos;
}

bool InstrumentationExt::SetBWDumpFlags(int flags) {
    this->mBWDumpFlags = flags;
    return true;
}

int InstrumentationExt::GetBWDumpFlags() {
    return this->mBWDumpFlags;
}

bool InstrumentationExt::InitWhenChildStart(SimpleKeyValuePair initParams[], uint arraySize) {
    // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - entry");
    CloseAllFiles();    // 关闭所有文件句柄。

    pid_t mainThreadID = 0;
    uid_t uid = 0;
    std::string packageName;
    std::string bwLogRootDir;

    // 解析参数。

    for (uint i = 0; i < arraySize; ++i) {
        // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - i=%u", i);
        SimpleKeyValuePair* tmp = &(initParams[i]);
        // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - tmp=%p", tmp);
        const char* key = tmp->key;
        // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - key=%s", key);
        const char* value = tmp->value;
        // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - value=%s", value);
        if (NULL == key) {
            BWDUMPE("[-] InstrumentationExt::InitWhenChildStart - %d. 键为NULL！", i);
            return false;
        }
        if (NULL == value) {
            BWDUMPE("[-] InstrumentationExt::InitWhenChildStart - %d. 值为NULL！", i);
            return false;
        }

        if (0 == strncmp(key, "mainThreadID", strlen("mainThreadID"))) {
            // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - key");
            mainThreadID = (pid_t) strtol(value, NULL, 10);
            if (0 == mainThreadID) {
                BWDUMPE("[-] InstrumentationExt::InitWhenChildStart - mainThreadID值从字符串转换为数字失败。");
                return false;
            }
        } else if (0 == strncmp(key, "uid", strlen("uid"))) {
            // BWLOGI("[*] InstrumentationExt::InitWhenChildStart - uid");
            uid = (uid_t) strtol(value, NULL, 10);
            if (0 == uid) {
                BWDUMPE("[-] InstrumentationExt::InitWhenChildStart - uid值从字符串转换为数字失败。");
                return false;
            }
        } else if (0 == strncmp(key, "packageName", strlen("packageName"))) {
            // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - packageName");
            packageName = value;
        } else if (0 == strncmp(key, "bwLogRootDir", strlen("bwLogRootDir"))) {
            // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - bwLogRootDir");
            bwLogRootDir = value;
        } else if (0 == strncmp(key, "bwDumpFlags", strlen("bwDumpFlags"))) {
            // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - bwDumpFlags");
            uint32_t bwDumpFlags = (uint32_t) strtol(value, NULL, 10);
            mBWDumpFlags = bwDumpFlags;
        } else {
            BWDUMPE("[-] InstrumentationExt::InitWhenChildStart - 无法识别的键：%s。", key);
            return false;
        }
    }

    // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - parse finished");
    this->mMainThreadID = mainThreadID;
    this->mUID = uid;
    this->mPackageName = packageName;
    this->mBWLogRootDir = bwLogRootDir;
    // BWLOGI("[-] InstrumentationExt::InitWhenChildStart - leave");
    return true;
}

pid_t InstrumentationExt::GetMainThreadID() {
    return mMainThreadID;
}

uid_t InstrumentationExt::GetUID() {
    return mUID;
}

const char* InstrumentationExt::GetPackageName() {
    return mPackageName.c_str();
}

FILE* InstrumentationExt::GetBWInstLogFile() {
    if (NULL == mBWInstLogFile) {
        pthread_mutex_lock(&gMutexBWInstLogFile);
        if (NULL == mBWInstLogFile) {
            mBWInstLogFile = CreateBWLogFile("inst", mBWInstLogPath);
        }
        pthread_mutex_unlock(&gMutexBWInstLogFile);
    }
    return mBWInstLogFile;
}

FILE* InstrumentationExt::GetBWRegLogFile() {
    if (NULL == mBWRegLogFile) {
        pthread_mutex_lock(&gMutexBWRegLogFile);
        if (NULL == mBWRegLogFile) {
            mBWRegLogFile = CreateBWLogFile("reg", mBWRegLogPath);
        }
        pthread_mutex_unlock(&gMutexBWRegLogFile);
    }
    return mBWRegLogFile;
}

// 关闭所有文件句柄。
bool InstrumentationExt::CloseAllFiles() {
    bool result = true;
    if (NULL != this->mBWInstLogFile) {
        if (0 != fclose(this->mBWInstLogFile)) {
            result = false;
            BWDUMPE("[-] 关闭指令的日志文件失败！包名：%s，uid=%d，日志路径：%s。错误信息：%d-%s。",
                mPackageName.c_str(), mUID, mBWInstLogPath.c_str(), errno, strerror(errno));
        }
        this->mBWInstLogFile = NULL;
    }
    if (NULL != this->mBWRegLogFile) {
        if (0 != fclose(this->mBWRegLogFile)) {
            result = false;
            BWDUMPE("[-] 关闭指令的日志文件失败！包名：%s，uid=%d，日志路径：%s。错误信息：%d-%s。",
                mPackageName.c_str(), mUID, mBWRegLogPath.c_str(), errno, strerror(errno));
        }
        this->mBWRegLogFile = NULL;
    }
    return result;
}

// 创建BW日志文件。
FILE* InstrumentationExt::CreateBWLogFile(const char* suffix, std::string& filePath) {
    FILE* fp = NULL;
    std::string path;
    uint count = 0;
    while (1) {
        CString timeString  = GetCurrentTimeString();
        std::string bwLogNameBase = StringPrintf("%s_%s_%d_%d", mPackageName.c_str(),
            timeString.GetCString(), mUID, getpid());
        std::string p = StringPrintf("%s/%s_%s.log", mBWLogRootDir.c_str(),
            bwLogNameBase.c_str(), suffix);
        if (0 == access(p.c_str(), F_OK)) {
            BWLOGE("[-] InstrumentationExt::CreateBWLogFile - %d. 日志文件\"%s\"已存在，"
                "这不科学！", count, p.c_str());
        } else {
            path = p;
            break;
        }
        count++;
    }

    fp = fopen(path.c_str(), "w+e");
    if (NULL == fp) {
        BWLOGE("[-] InstrumentationExt::CreateBWLogFile - 创建bw日志文件失败！"
            "文件路径：%s，包名：%s，uid=%d。错误信息：%d-%s。",
            path.c_str(), mPackageName.c_str(), mUID, errno, strerror(errno));
    } else {
        filePath = path;
    }
    return fp;
}

}  // namespace art
