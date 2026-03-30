#ifndef _PUL_Debug_
#define _PUL_Debug_

#include <include/c_stdio.h>
#include <core/rvl/OS/OSContext.hpp>
#include <core/rvl/OS/OSError.hpp>

namespace Pulsar {

void Pulsar_LogMessage(const char* message);
bool Pulsar_FlushLogs();

namespace Debug {

void FatalError(const char* string);
void LaunchSoftware();
struct GPR {
    void Set(const OS::Context& context, u32 idx, u32 regValue) {
        gpr = context.gpr[idx];
        name = 'r00:' + regValue;
    }
    u32 name;
    u32 gpr;
};

struct FPR {
    void Set(const OS::Context& context, u32 idx, u32 regValue) {
        fpr = context.fpr[idx];
        name = 'f00:' + regValue;
    }
    u32 name;
    double fpr;
};

struct StackFrame {
    StackFrame() : spName('sp: '), sp(0), lrName('lr: '), lr(0) {};
    u32 spName;
    u32 sp;
    u32 lrName;
    u32 lr;
};

struct ExceptionFile {
    explicit ExceptionFile(const OS::Context& context);

    u32 magic;
    u32 region;
    u32 reserved;
    OS::Error error;
    GPR srr0;
    GPR srr1;
    GPR msr;
    GPR cr;
    GPR lr;
    GPR gprs[32];
    FPR fprs[32];
    FPR fpscr;
    StackFrame frames[10];
};

}  // namespace Debug
}  // namespace Pulsar

#define Pulsar_Log(...)                                            \
    do {                                                           \
        char pulsarLogBuffer[1024];                                \
        snprintf(pulsarLogBuffer, sizeof(pulsarLogBuffer), __VA_ARGS__); \
        ::Pulsar::Pulsar_LogMessage(pulsarLogBuffer);              \
    } while (0)

#endif
