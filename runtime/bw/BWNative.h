/* Copyright 2016 buwai */

#ifndef ART_RUNTIME_BW_BWNATIVE_H_
#define ART_RUNTIME_BW_BWNATIVE_H_

#ifdef __cplusplus
extern "C" {
#endif

void* GetInstrumentationExt();

bool InsertOrUpdateTraceMethodInfo(void* traceMethodInfoExt);

bool DeleteTraceMethodInfo(int hash);

bool QueryTraceMethodInfo(int hash, void* traceMethodInfoExt);

bool InsertOrUpdateHookMethodInstInfo(void* hookMethodInstInfo);

unsigned int DeleteHookMethodInstInfo(int hash, int64_t instLineNum);

unsigned int DeleteHookMethodInstInfoInMethod(int hash);

unsigned int DeleteHookMethodInstInfoInPackage();

/**
 * 查询指定方法的Hook指令信息。
 * @param[in] hash 哈希。
 * @param[out] hookMethodInstInfo 如果当前方法有Hook指令信息，则返回Hook指令信息数组（一个方法中可以Hook多个指令）。
 *             参数必须已经经过初始化。
 * @return 查询成功，则返回true；否则返回false。
 */
bool QueryHookMethodInstInfoInMethod(int hash, void* hookMethodInstInfos);

/**
 * 查询指定方法的指定行的Hook指令信息。
 * @param[in] hash 哈希。
 * @param[in] instLineNum 指令行号。
 * @param[out] hookMethodInstInfo 查询成功，则返回Hook指令信息。
 * @return 查询成功，则返回true；查询失败，则返回false。
 */
bool QueryHookMethodInstInfo(int hash, int64_t instLineNum, void* hookMethodInstInfo);

bool SetBWDumpFlags(int flags);

int GetBWDumpFlags();

bool InitWhenChildStart(void* initParams, uint32_t arraySize);

#ifdef __cplusplus
}
#endif

#endif  // ART_RUNTIME_BW_BWNATIVE_H_
