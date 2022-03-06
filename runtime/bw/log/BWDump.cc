/* Copyright 2016 buwai */

#include "stdafx.h"
#include <unistd.h>
#include <sys/types.h>
#include "BWArtUtils.h"
#include "runtime.h"
#include "BWLog.h"
#include "BWDump.h"

// buf needs to store 30 characters
static int TimeToStr(char *buf, uint len, struct timespec *ts) {
    int ret;
    struct tm t;

    tzset();
    if (localtime_r(&(ts->tv_sec), &t) == NULL)
        return -1;

    ret = strftime(buf, len, "%F %T", &t);
    if (ret == 0)
        return -2;
    len -= ret - 1;

    ret = snprintf(&buf[strlen(buf)], len, ".%09ld", ts->tv_nsec);
    if (ret >= 0 && (uint)ret < len) {
        return 0;
    } else {
        return -3;
    }
}

static std::string GetCurrentTime() {
    std::string currentTime = "";
    int clk_id = CLOCK_REALTIME;
    const uint TIME_FMT = 50;
    char timestr[TIME_FMT];

    struct timespec ts;
    clock_gettime(clk_id, &ts);

    if (TimeToStr(timestr, TIME_FMT, &ts) != 0) {
        BWLOGE("[-] BWDump.GetCurrentTime - 时间转换为字符串失败！");
    } else {
        currentTime = timestr;
    }
    return currentTime;
}

namespace art {

bool BWDump::Dump(const ShadowFrame& shadow_frame, const Instruction* inst,
    const uint32_t dex_pc, MethodHelper& mh) {
    InstrumentationExt* instrumentationExt = Runtime::Current()->GetInstrumentationExt();
    if (instrumentationExt->IsBWDumpPrint()) {
        std::string instString = InstToString(shadow_frame, inst, dex_pc, mh);
        instString = "[*] " + instString;
        BWLOGI("%s", instString.c_str());
    }

    if (instrumentationExt->IsBWDumpWriteFile()) {
        FILE* bwInstLogFile = instrumentationExt->GetBWInstLogFile();
        if (NULL == bwInstLogFile) {
            BWLOGE("[-] BWDump::Dump - 指令日志文件句柄等于NULL。");
            return false;
        }
        FILE* bwRegLogFile = instrumentationExt->GetBWRegLogFile();
        if (NULL == bwRegLogFile) {
            BWLOGE("[-] BWDump::Dump - 寄存器状态日志文件句柄等于NULL。");
            return false;
        }

        std::string strCurrentTime = GetCurrentTime();
        std::string content = "";
        std::ostringstream oss;
        size_t writeNum;

        std::string strDexPC = StringPrintf("\n0x%x: ", dex_pc);
        // 生成方法名和smali指令。
        oss << strCurrentTime
            << "  " << getpid() << "-" << ::art::GetTid() << "\n";
        oss << PrettyMethod(shadow_frame.GetMethod())
            << strDexPC
            << inst->DumpString(mh.GetMethod()->GetDexFile()) << "\n";
        content = oss.str();
        oss.str("");

        // 方法名和smali指令写入日志文件。
        writeNum = fwrite(content.c_str(), 1, content.size(), bwInstLogFile);
        if (writeNum != content.size()) {
            BWLOGE("[-] 1. BWDump::Dump - 写BW日志文件失败，期望写入的字节数与实际写入的字节数不符。"
                "期望写入的字节数：%zu，实际写入的字节数：%zu。包名：%s，UID：%d。错误信息：%d-%s。",
                content.size(), writeNum, instrumentationExt->GetPackageName(), instrumentationExt->GetUID(),
                errno, strerror(errno));
            return false;
        }
        if (0 != fflush(bwInstLogFile)) {
            BWLOGE("[-] 1. BWDump::Dump - 刷新缓冲区失败。错误信息：%d-%s。", errno, strerror(errno));
            return false;
        }

        oss << strCurrentTime
            << "  " << getpid() << "-" << ::art::GetTid() << "\n";
        // 生成寄存器信息。
        for (uint32_t i = 0; i < shadow_frame.NumberOfVRegs(); ++i) {
            uint32_t raw_value = shadow_frame.GetVReg(i);
            mirror::Object* ref_value = shadow_frame.GetVRegReference(i);
            oss << StringPrintf("vreg%u=0x%08X", i, raw_value);
            if (ref_value != NULL) {
                if (ref_value->GetClass()->IsStringClass() &&
                    ref_value->AsString()->GetCharArray() != NULL) {
                    oss << "/java.lang.String \"" << ref_value->AsString()->ToModifiedUtf8() << "\"";
                } else {
                    oss << "/" << PrettyTypeOf(ref_value);
                }
            }
            oss << "  ";
        }
        oss << "\n";
        content = oss.str();
        oss.str("");

        writeNum = fwrite(content.c_str(), 1, content.size(), bwRegLogFile);
        if (writeNum != content.size()) {
            BWLOGE("[-] BWDump::Dump - 2. 写BW日志文件失败，期望写入的字节数与实际写入的字节数不符。"
                "期望写入的字节数：%zu，实际写入的字节数：%zu。包名：%s，UID：%d。错误信息：%d-%s。",
                content.size(), writeNum, instrumentationExt->GetPackageName(), instrumentationExt->GetUID(),
                errno, strerror(errno));
            return false;
        }
        if (0 != fflush(bwRegLogFile)) {
            BWLOGE("[-] BWDump::Dump - 2. 刷新缓冲区失败。错误信息：%d-%s。", errno, strerror(errno));
            return false;
        }
    }
    return true;
}

bool BWDump::Dump(int prio, const char* fmt, va_list ap) {
    InstrumentationExt* instrumentationExt = Runtime::Current()->GetInstrumentationExt();
    if (instrumentationExt->IsBWDumpPrint()) {
        BWLOG_VPRINT(prio, fmt, ap);
    }

    if (instrumentationExt->IsBWDumpWriteFile()) {
        const char* strPrio = NULL;
        switch (prio) {
            case ANDROID_LOG_VERBOSE:
                strPrio = "V";
                break;
            case ANDROID_LOG_DEBUG:
                strPrio = "D";
                break;
            case ANDROID_LOG_INFO:
                strPrio = "I";
                break;
            case ANDROID_LOG_WARN:
                strPrio = "W";
                break;
            case ANDROID_LOG_ERROR:
                strPrio = "E";
                break;
            case ANDROID_LOG_FATAL:
                strPrio = "F";
                break;
            case ANDROID_LOG_SILENT:
                strPrio = "S";
                break;
            default:
                strPrio = "X";
                break;
        }

        std::string result;
        StringAppendV(&result, fmt, ap);

        FILE* bwInstLogFile = instrumentationExt->GetBWInstLogFile();
        if (NULL == bwInstLogFile) {
            BWLOGE("[-] BWDump::Dump(int,const char*,...) - 指令日志文件句柄等于NULL。");
            return false;
        }

        std::string strCurrentTime = GetCurrentTime();
        std::string content = "";
        std::ostringstream oss;
        size_t writeNum;

        oss << strCurrentTime
            << "  " << getpid() << "-" << ::art::GetTid() << " " << strPrio << " "
            << result << "\n";
        content = oss.str();
        oss.str("");

        writeNum = fwrite(content.c_str(), 1, content.size(), bwInstLogFile);
        if (writeNum != content.size()) {
            BWLOGE("[-] BWDump::Dump(int,const char*,...) - 向BW日志文件写时间戳失败，期望写入的字节数与实际写入的字节数不符。"
                "期望写入的字节数：%zu，实际写入的字节数：%zu。包名：%s，UID：%d。错误信息：%d-%s。",
                content.size(), writeNum, instrumentationExt->GetPackageName(), instrumentationExt->GetUID(),
                errno, strerror(errno));
            return false;
        }
    }
    return true;
}

bool BWDump::Dump(int prio, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    bool result = Dump(prio, fmt, ap);
    va_end(ap);
    return result;
}

void BWDump::PrintMethodInfo(ShadowFrame& shadow_frame) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
    mirror::ArtMethod* method = shadow_frame.GetMethod();
    std::ostringstream oss;

    Lsp<TraceMethodInfoBase> traceMethodInfoBase = shadow_frame.GetTraceMethodInfo();
    for (uint32_t i = 0; i < traceMethodInfoBase->deep; i++) {
        oss << "-";
    }
    std::string prettyMethod = PrettyMethod(method);
    oss << prettyMethod << "  ";
    for (uint32_t i = 0; i < shadow_frame.NumberOfVRegs(); ++i) {
            uint32_t raw_value = shadow_frame.GetVReg(i);
            mirror::Object* ref_value = shadow_frame.GetVRegReference(i);
            oss << StringPrintf("vreg%u=0x%08X", i, raw_value);
            if (ref_value != NULL) {
                if (ref_value->GetClass()->IsStringClass() &&
                    ref_value->AsString()->GetCharArray() != NULL) {
                    oss << "/java.lang.String \"" << ref_value->AsString()->ToModifiedUtf8() << "\"";
                } else {
                    oss << "/" << PrettyTypeOf(ref_value);
                }
            }
            oss << "  ";
        }
    std::string result = oss.str();
    // BWLOGI("[*] BWDump::PrintMethodInfo - result=%s", result.c_str());
    BWDUMPI("[*] %s", result.c_str());
}

}  // namespace art
