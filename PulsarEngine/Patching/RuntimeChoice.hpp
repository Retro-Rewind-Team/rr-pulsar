#ifndef PULSAR_ENGINE_PATCHING_RUNTIME_CHOICE_HPP
#define PULSAR_ENGINE_PATCHING_RUNTIME_CHOICE_HPP

#include <kamek.hpp>
#include <runtimeWrite.hpp>

#define RuntimeChoice_CallSite(address, hook) kmCall((address), hook)
#define RuntimeChoice_LeafReturn(address, hook) kmBranch((address), hook)

#define RuntimeChoice_LoadCachedU32(destReg, valueSymbol) \
    stwu r1, -0x10(r1);                                   \
    stw r12, 0x8(r1);                                      \
    lis r12, valueSymbol @ha;                              \
    lwz destReg, valueSymbol @l(r12);                      \
    lwz r12, 0x8(r1);                                      \
    addi r1, r1, 0x10

#define RuntimeChoice_RestoreScratchAndCR() \
    lwz r12, 0xC(r1);                       \
    mtcrf 0xff, r12;                        \
    lwz r12, 0x8(r1);                       \
    addi r1, r1, 0x20

// Generates a hook that loads a cached u32 into a register and returns through LR.
#define RuntimeChoice_CachedInstruction(name, address, destReg, valueSymbol) \
    asmFunc name() {                                                         \
        ASM(                                                                 \
            nofralloc;                                                       \
            RuntimeChoice_LoadCachedU32(destReg, valueSymbol);               \
            blr;)                                                            \
    }                                                                        \
    kmCall((address), name)

// Generates a hook that loads two cached u32 values into two registers.
#define RuntimeChoice_CachedInstruction2(name, address, destRegA, valueSymbolA, destRegB, valueSymbolB) \
    asmFunc name() {                                                                                   \
        ASM(                                                                                           \
            nofralloc;                                                                                 \
            stwu r1, -0x10(r1);                                                                        \
            stw r12, 0x8(r1);                                                                          \
            lis r12, valueSymbolA @ha;                                                                 \
            lwz destRegA, valueSymbolA @l(r12);                                                        \
            lis r12, valueSymbolB @ha;                                                                 \
            lwz destRegB, valueSymbolB @l(r12);                                                        \
            lwz r12, 0x8(r1);                                                                          \
            addi r1, r1, 0x10;                                                                         \
            blr;)                                                                                      \
    }                                                                                                  \
    kmCall((address), name)

// Generates a hook for tiny leaf-return functions. It branches instead of calls
// so the original caller's LR is preserved.
#define RuntimeChoice_ReturnValue(name, hookKind, address, destReg, valueSymbol) \
    asmFunc name() {                                                             \
        ASM(                                                                     \
            nofralloc;                                                           \
            RuntimeChoice_LoadCachedU32(destReg, valueSymbol);                   \
            blr;)                                                                \
    }                                                                            \
    hookKind((address), name)

// if flag != 0: destReg = 0; otherwise: destReg = *(baseReg + offset)
#define RuntimeChoice_ConditionalLoadOrZero(name, address, flagSymbol, destReg, baseReg, offset) \
    asmFunc name() {                                                                                \
        ASM(                                                                                        \
            nofralloc;                                                                              \
            stwu r1, -0x20(r1);                                                                     \
            stw r12, 0x8(r1);                                                                       \
            mfcr r12;                                                                               \
            stw r12, 0xC(r1);                                                                       \
            lis r12, flagSymbol @ha;                                                                \
            lwz r12, flagSymbol @l(r12);                                                            \
            cmpwi r12, 0;                                                                           \
            bne useChoice;                                                                          \
            RuntimeChoice_RestoreScratchAndCR();                                                    \
            lwz destReg, offset(baseReg);                                                           \
            blr;                                                                                    \
            useChoice:;                                                                             \
            RuntimeChoice_RestoreScratchAndCR();                                                    \
            li destReg, 0;                                                                          \
            blr;)                                                                                   \
    }                                                                                               \
    kmCall((address), name)

// if flag != 0: destReg = 0; otherwise: destReg = baseReg + immediate
#define RuntimeChoice_ConditionalAddOrZero(name, address, flagSymbol, destReg, baseReg, immediate) \
    asmFunc name() {                                                                               \
        ASM(                                                                                       \
            nofralloc;                                                                             \
            stwu r1, -0x20(r1);                                                                    \
            stw r12, 0x8(r1);                                                                      \
            mfcr r12;                                                                              \
            stw r12, 0xC(r1);                                                                      \
            lis r12, flagSymbol @ha;                                                               \
            lwz r12, flagSymbol @l(r12);                                                           \
            cmpwi r12, 0;                                                                          \
            bne useChoice;                                                                         \
            RuntimeChoice_RestoreScratchAndCR();                                                   \
            addi destReg, baseReg, immediate;                                                      \
            blr;                                                                                   \
            useChoice:;                                                                            \
            RuntimeChoice_RestoreScratchAndCR();                                                   \
            li destReg, 0;                                                                         \
            blr;)                                                                                  \
    }                                                                                              \
    kmCall((address), name)

// if flag != 0: destReg = immediate; otherwise: destReg = *(u8*)(baseReg + offset)
#define RuntimeChoice_ConditionalByteOrImmediate(name, address, flagSymbol, destReg, baseReg, offset, immediate) \
    asmFunc name() {                                                                                              \
        ASM(                                                                                                      \
            nofralloc;                                                                                            \
            stwu r1, -0x20(r1);                                                                                   \
            stw r12, 0x8(r1);                                                                                     \
            mfcr r12;                                                                                             \
            stw r12, 0xC(r1);                                                                                     \
            lis r12, flagSymbol @ha;                                                                              \
            lwz r12, flagSymbol @l(r12);                                                                          \
            cmpwi r12, 0;                                                                                         \
            bne useChoice;                                                                                        \
            RuntimeChoice_RestoreScratchAndCR();                                                                  \
            lbz destReg, offset(baseReg);                                                                         \
            blr;                                                                                                  \
            useChoice:;                                                                                           \
            RuntimeChoice_RestoreScratchAndCR();                                                                  \
            li destReg, immediate;                                                                                \
            blr;)                                                                                                 \
    }                                                                                                             \
    kmCall((address), name)

// if flag != 0: *(baseReg + offset) = 0; otherwise: *(baseReg + offset) = sourceReg
#define RuntimeChoice_ConditionalStoreRegOrZero(name, address, flagSymbol, sourceReg, baseReg, offset) \
    asmFunc name() {                                                                                   \
        ASM(                                                                                           \
            nofralloc;                                                                                 \
            stwu r1, -0x20(r1);                                                                        \
            stw r12, 0x8(r1);                                                                          \
            stw r0, 0x10(r1);                                                                          \
            mfcr r12;                                                                                  \
            stw r12, 0xC(r1);                                                                          \
            lis r12, flagSymbol @ha;                                                                   \
            lwz r12, flagSymbol @l(r12);                                                               \
            cmpwi r12, 0;                                                                              \
            bne useChoice;                                                                             \
            lwz r12, 0xC(r1);                                                                          \
            mtcrf 0xff, r12;                                                                           \
            lwz r0, 0x10(r1);                                                                          \
            lwz r12, 0x8(r1);                                                                          \
            addi r1, r1, 0x20;                                                                         \
            stw sourceReg, offset(baseReg);                                                            \
            blr;                                                                                       \
            useChoice:;                                                                                \
            lwz r12, 0xC(r1);                                                                          \
            mtcrf 0xff, r12;                                                                           \
            lwz r12, 0x8(r1);                                                                          \
            li r0, 0;                                                                                  \
            stw r0, offset(baseReg);                                                                   \
            lwz r0, 0x10(r1);                                                                          \
            addi r1, r1, 0x20;                                                                         \
            blr;)                                                                                      \
    }                                                                                                  \
    kmCall((address), name)

#define RuntimeChoice_Continuation(continuationAddress) kmRuntimeUse(continuationAddress)
#define RuntimeChoice_BranchBack(address, hook) kmBranch((address), hook)
#define RuntimeChoice_FunctionDispatch(address, hook) kmBranch((address), hook)

#endif
