/* Copyright 2016 buwai */

#ifndef ART_RUNTIME_BW_INSTRUMENTATIONEXT_H_
#define ART_RUNTIME_BW_INSTRUMENTATIONEXT_H_

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#include <string>
#include <map>
#include <vector>

#include "base/macros.h"
#include "base/mutex.h"
#include "dex_file.h"
#include <BWNativeHelper/BWCommon.h>
#include "BWMethodFilter.h"

namespace art {

namespace mirror {
    class ArtMethod;
}

//////////////////////////////////////////////////////////////////////////
// MethodPCCatch

/**
 * 方法中smali指令行号与PC对应值的缓存。
 */
class MethodPCCatch {
 public:
    /**
     * 方法哈希，用来确定某个方法。
     */
    int hash;
    /**
     * 方法的指令行号。
     */
    int64_t instLineNum;
    /**
     * dex方法的pc值。
     */
    int64_t dexPC;

    MethodPCCatch();

    MethodPCCatch& operator=(const MethodPCCatch& other);

    /**
     * 是否有效。
     * @return 有效，则返回true；无效，则返回false。
     */
    bool IsValid();
};

//////////////////////////////////////////////////////////////////////////
// DexPCCatch

class DexPCCatch {
 public:
    /**
     * 获得行号对应的PC偏移。
     * @param[in] hash 方法哈希。
     * @param[in] instLineNum 行号。
     * @param[in] code_item 当还未读取方法的PC偏移与行号的对应关系时，这个参数才起作用。
     * @return 返回行号对应的PC偏移。
     */
    int64_t GetPC(int hash, int64_t instLineNum, const DexFile::CodeItem* code_item);

 private:
    std::map<int, std::vector<MethodPCCatch>*> mMethodPCCatches;

    bool InitMethodPCCatch(int hash, const DexFile::CodeItem* code_item);
};

//////////////////////////////////////////////////////////////////////////
// InstrumentationExt

class InstrumentationExt {
 public:
    DexPCCatch mDexPCCatch;

    Lsp<BWMethodFilter> mBWMethodFilter;

    /**
     * 生成方法哈希。
     * @param[in] method 方法结构体。
     */
    static int GenMethodHash(mirror::ArtMethod& method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

    InstrumentationExt() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
    ~InstrumentationExt();
    /**
     * 是否跟踪/提示。
     * @param[in] method 方法结构体。
     * @param[out] traceInfo 返回跟踪信息。在此函数内部不会对它进行内存分配。
     * @return 返回true表示要进行跟踪，此时参数traceInfo的返回值有意义；
     *         返回false表示不需要进行跟踪，此时参数traceInfo的返回值无意义。
     */
    bool IsTrace(mirror::ArtMethod& method, Lsp<TraceMethodInfoBase>& traceInfo) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

    /**
     * 插入跟踪信息，当某个方法的跟踪信息已存在，则更新这个信息。
     * @param[in] traceMethodInfoExt 方法的跟踪信息。
     * @return 插入或更新成功，则返回true；否则，返回false。
     */
    bool InsertOrUpdateTraceMethodInfo(Lsp<TraceMethodInfoBase> traceMethodInfoBase);

    /**
     * 根据方法哈希删除跟踪方法信息。
     * @param[in] hash 方法哈希。
     * @return 删除成功，则返回true；删除失败或者没有删除，则返回false。
     */
    bool DeleteTraceMethodInfo(int hash);

    /**
     * 根据方法哈希查询跟踪方法信息。
     * @param[in]  hash 方法哈希。
     * @param[out] traceMethodInfoExt 如果查到相应的跟踪方法信息，则返回方法跟踪信息。
     * @return 查到相应的跟踪方法信息，则返回true；否则，返回false。
     */
    bool QueryTraceMethodInfo(int hash, Lsp<TraceMethodInfoBase>& traceMethodInfoExt);

    /**
     * 获得所有的跟踪方法信息。
     * @return 返回所有的跟踪方法信息。
     */
    std::map<int, Lsp<TraceMethodInfoBase>>* GetTraceMethodInfos();

    bool InsertOrUpdateHookMethodInstInfo(Lsp<HookMethodInstInfoBase> hookMethodInstInfoBase);

    /**
     * 删除哈希对应的方法中的某个Hook指令信息。
     * @param[in] hash 哈希。
     * @return 删除成功，则返回true；删除失败，则返回false。
     */
    unsigned int DeleteHookMethodInstInfo(int hash, int64_t instLineNum);

    /**
     * 删除哈希对应的方法中所有的Hook指令信息。
     * @param[in] hash 哈希。
     * @return 删除成功，则返回true；删除失败，则返回false。
     */
    unsigned int DeleteHookMethodInstInfo(int hash);

    /**
     * 删除所有的Hook指令信息。
     */
    unsigned int DeleteHookMethodInstInfo();

    /**
     * 查询指定方法的Hook指令信息。
     * @param[in] hash 哈希。
     * @param[out] hookMethodInstInfoBase 如果当前方法有Hook指令信息，则返回Hook指令信息集（一个方法中可以Hook多个指令）。
     *             返回一份拷贝数据。
     * @return 查询成功，则返回true；否则返回false。
     * 
     */
    bool QueryHookMethodInstInfo(int hash, std::map<int64_t, Lsp<HookMethodInstInfoBase>>& hookMethodInstInfos);

    /**
     * 查询指定方法的Hook指令信息。
     * @param[in] hash 哈希。
     * @param[out] hookMethodInstInfoBase 如果当前方法有Hook指令信息，则返回Hook指令信息数组（一个方法中可以Hook多个指令）。
     *                                    参数必须已经经过初始化。返回一份拷贝数据。
     * @return 查询成功，则返回true；否则返回false。
     */
    bool QueryHookMethodInstInfo(int hash, std::vector<Lsp<HookMethodInstInfoBase>>* hookMethodInstInfos);

    /**
     * 查询指定方法的指定行的Hook指令信息。
     * @param[in] hash 哈希。
     * @param[in] instLineNum 指令行号。
     * @return 查询成功，则返回Hook指令信息，返回一份拷贝数据；查询失败，则Lsp<HookMethodInstInfoBase>.IsEmpty()将返回true。
     */
    Lsp<HookMethodInstInfoBase> QueryHookMethodInstInfo(int hash, int64_t instLineNum);

    std::map<int, Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > >* GetHookMethodInstInfos();

    bool SetBWDumpFlags(int flags);

    int GetBWDumpFlags();

    /**
     * 当子进程启动时初始化。
     *
     * 这个函数在com_android_internal_os_Zygote.cpp文件的ForkAndSpecializeCommon函数中，
     * 通过调用BWNativeHelper中的函数被设置。
     *
     * 这个函数只能被调一次。
     *
     * @param[in] uid zygote fork出的子进程的uid。
     * @param[in] packageName zygote fork出的子进程的包名。（子进程通常应该是APP进程）
     * @param[in] bwLogPath bw日志路径。
     * @return 初始化成功，则返回true；初始化失败，则返回false。
     */
    bool InitWhenChildStart(SimpleKeyValuePair initParams[], uint arraySize);

    /**
     * 获得主线程ID。
     * @return 返回主线程ID。
     */
    pid_t GetMainThreadID();

    /**
     * 获得UID。
     * @return 返回UID。
     */
    uid_t GetUID();

    /**
     * 获得当前APP包名。
     * @reurn 返回当前APP包名。
     */
    const char* GetPackageName();
    FILE* GetBWInstLogFile();
    FILE* GetBWRegLogFile();

    /**
     * 是否打印bw日志。
     * @return 打印bw日志，则返回true；否则，返回false。
     */
    inline bool IsBWDumpPrint() {
        return (mBWDumpFlags & BWDumpBase::BW_DUMP_FLAGS_PRINT) != 0;
    }

    /**
     * 是否将bw日志写入文件。
     * @return 将bw日志写入文件，则返回true；否则，返回false。
     */
    inline bool IsBWDumpWriteFile() {
        return (mBWDumpFlags & BWDumpBase::BW_DUMP_FLAGS_WRITE_FILE) != 0;
    }

 private:
    /**
     * 关闭所有文件句柄。
     * 当某个文件关闭失败时仅提示错误信息，函数继续执行。
     * @return 所有文件均关闭成功，则返回true；否则，返回false。
     */
    bool CloseAllFiles();

    /**
     * 创建BW日志文件。
     * @param[in] suffix 文件名后缀。如：inst、reg等。
     * @param[out] filePath 创建成功，则输出文件路径。
     * @return 创建成功，则返回文件句柄；创建失败，则返回NULL。
     */
    FILE* CreateBWLogFile(const char* suffix, std::string& filePath);

    // TODO: 对mTraceMethodInfos、mHookMethodInstInfos的操作应该设置内存屏障。

    std::map<int, Lsp<TraceMethodInfoBase>> mTraceMethodInfos;

    /**
     * 外部map中的key类型为int，它代表被Hook方法的哈希。
     * 内部map中的key类型为int64，它代表被Hook指令的行号。
     */
    std::map<int, Lsp< std::map< int64_t, Lsp<HookMethodInstInfoBase> > > > mHookMethodInstInfos;

    /**
     * 主线程ID。
     */
    pid_t mMainThreadID;
    /**
     * 当前进程UID。
     */
    uid_t mUID;
    /**
     * 当前进程包名。
     */
    std::string mPackageName;
    /**
     * BW日志根目录。
     */
    std::string mBWLogRootDir;
    /**
     * 指令日志文件完整路径。
     */
    std::string mBWInstLogPath;
    /**
     * 寄存器日志文件完整路径。
     */
    std::string mBWRegLogPath;
    /**
     * 保存指令的日志文件。
     */
    FILE* mBWInstLogFile;
    /**
     * 保存寄存器状态的日志文件。
     */
    FILE* mBWRegLogFile;

    /**
     * 控制bw日志的标志。
     */
    uint32_t mBWDumpFlags;
};

}  // namespace art

#endif  // ART_RUNTIME_BW_INSTRUMENTATIONEXT_H_
