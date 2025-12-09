/*
 * Kamek Standard Library
 * Wii game patching engine
 * (c) Treeki 2010-2018
 */

#ifndef __KAMEK_H
#define __KAMEK_H
#include <types.hpp>

#include <include/c_stdlib.h>
#include <include/c_math.h>
#include <include/c_stdio.h>
#include <include/c_wchar.h>
#include <include/c_string.h>
#include "hooks.hpp"

#include <MarioKartWii/Math/Matrix.hpp>
#include <MarioKartWii/Math/Vector.hpp>
#include <core/nw4r/ut/LinkList.hpp>
#include <core/nw4r/ut/List.hpp>
// just for usability in other structs

/*
template<int size>
struct StringStack {
    operator const char* () const { return &cString[0]; }
    operator char* () { return &cStringPtr[0]; }
    const char& operator[](u32 pos) const { return cString[pos]; }
    char& operator[](u32 pos) { return cString[pos]; }

    char cString[size];
};

template<int size>
struct StringHeap {
    operator const char* () const { return cStringPtr; }
    operator char* () { return cStringPtr; }
    const char& operator[](u32 pos) const { return cStringPtr[pos]; }
    char& operator[](u32 pos) { return cStringPtr[pos]; }

    char* cStringPtr;
};

template<bool B, class T, class F>
struct conditional { typedef T type; };

template<class T, class F>
struct conditional<false, T, F> { typedef F type; };

template<int size>
struct String {
    typedef typename conditional<size >= 100, StringHeap<size>, StringStack<size> >::type Impl;
};
*/

namespace aux {
typedef char yes[1];
typedef char no[2];

template <typename Parent, typename Child>
struct Host {
    operator Parent*() const;
    operator Child*();
};
}  // namespace aux
template <typename Parent, typename Child>
struct is_base_of {
    template <typename T>
    static aux::yes& check(Child*, T);
    static aux::no& check(Parent*, int);

    static const bool value = sizeof(check(aux::Host<Parent, Child>(), int())) == sizeof(aux::yes);
};

/*
CW implements ptmfs with this pseudo-struct:
struct Ptmf{
    u32 passedArgDelta; (this + subject is the arg passed into the func)
    s32 offsetToVtable; (offset to the vtable of the subject, also serves to specify if the call is virtual (set to -1 if it isn't)
    u32 funcData; (if the member func is not virtual, this is its address; if the func is virtual, this is its offset in the vtable)
};*/

template <class Subject, typename Ret>
struct Ptmf_0A {
    template <class FromSubject, typename FromRet>
    void operator=(FromRet (FromSubject::* const& src)()) {
        this->ptr = static_cast<Ret (Subject::*)()>(src);
    }
    Ret (Subject::*ptr)();
};  // total size 0xc

template <class Subject, typename Ret, typename A1>
struct Ptmf_1A {
    template <class FromSubject, typename FromRet, typename FromA1>
    void operator=(FromRet (FromSubject::* const& src)(FromA1)) {
        this->ptr = static_cast<Ret (Subject::*)(FromA1)>(src);
    }
    Ret (Subject::*ptr)(A1);
};  // total size 0xc

template <class Subject, typename Ret, typename A1, typename A2>
struct Ptmf_2A {
    template <class FromSubject, typename FromRet, typename FromA1, typename FromA2>
    void operator=(FromRet (FromSubject::* const& src)(FromA1, FromA2)) {
        this->ptr = static_cast<Ret (Subject::*)(FromA1, FromA2)>(src);
    }
    Ret (Subject::*ptr)(A1, A2);
};  // total size 0xc

template <class Subject, typename Ret, typename A1, typename A2, typename A3>
struct Ptmf_3A {
    template <class FromSubject, typename FromRet, typename FromA1, typename FromA2, typename FromA3>
    void operator=(FromRet (FromSubject::* const& src)(FromA1, FromA2, FromA3)) {
        this->ptr = static_cast<Ret (Subject::*)(FromA1, FromA2, FromA3)>(src);
    }
    Ret (Subject::*ptr)(A1, A2, A3);
};  // total size 0xc

template <typename Ret>
struct PtmfHolderBase_0A {
    virtual Ret operator()() const = 0;
};

template <class Subject, typename Ret>
struct PtmfHolder_0A : PtmfHolderBase_0A<Ret> {
    virtual Ret operator()() const {
        return (subject->*ptmf.ptr)();
    }
    Subject* subject;
    Ptmf_0A<Subject, Ret> ptmf;
    template <class ToSubject, typename ToRet>
    operator PtmfHolder_0A<ToSubject, ToRet>&() {
        return reinterpret_cast<PtmfHolder_0A<ToSubject, ToRet>&>(*this);
    }

    template <class ToSubject, typename ToRet>
    operator const PtmfHolder_0A<ToSubject, ToRet>&() const {
        return reinterpret_cast<const PtmfHolder_0A<ToSubject, ToRet>&>(*this);
    }
};

template <typename Ret, typename A1>
struct PtmfHolderBase_1A {
    virtual Ret operator()(A1 a1) const = 0;
};

template <class Subject, typename Ret, typename A1>
struct PtmfHolder_1A : PtmfHolderBase_1A<Ret, A1> {
    virtual Ret operator()(A1 a1) const {
        return (subject->*ptmf.ptr)(a1);
    }
    Subject* subject;
    Ptmf_1A<Subject, Ret, A1> ptmf;
    template <class ToSubject, typename ToRet, typename ToA1>
    operator PtmfHolder_1A<ToSubject, ToRet, ToA1>&() {
        return reinterpret_cast<PtmfHolder_1A<ToSubject, ToRet, ToA1>&>(*this);
    }

    template <class ToSubject, typename ToRet, typename ToA1>
    operator const PtmfHolder_1A<ToSubject, ToRet, ToA1>&() const {
        return reinterpret_cast<const PtmfHolder_1A<ToSubject, ToRet, ToA1>&>(*this);
    }
};

template <typename Ret, typename A1, typename A2>
struct PtmfHolderBase_2A {
    virtual Ret operator()(A1 a1, A2 a2) const = 0;
};
template <class Subject, typename Ret, typename A1, typename A2>
struct PtmfHolder_2A : PtmfHolderBase_2A<Ret, A1, A2> {
    virtual Ret operator()(A1 a1, A2 a2) const {
        return (subject->*ptmf.ptr)(a1, a2);
    }
    Subject* subject;
    Ptmf_2A<Subject, Ret, A1, A2> ptmf;

    template <class ToSubject, typename ToRet, typename ToA1, typename ToA2>
    operator PtmfHolder_2A<ToSubject, ToRet, ToA1, ToA2>&() {
        // static_cast<const ToSubject *>((const Subject *)nullptr);
        return reinterpret_cast<PtmfHolder_2A<ToSubject, ToRet, ToA1, ToA2>&>(*this);
    }

    template <class ToSubject, typename ToRet, typename ToA1, typename ToA2>
    operator const PtmfHolder_2A<ToSubject, ToRet, ToA1, ToA2>&() const {
        // static_cast<const ToSubject *>((const Subject *)nullptr);
        return reinterpret_cast<const PtmfHolder_2A<ToSubject, ToRet, ToA1, ToA2>&>(*this);
    }
};

template <typename Ret, typename A1, typename A2, typename A3>
struct PtmfHolderBase_3A {
    virtual Ret operator()(A1 a1, A2 a2, A3 a3) const = 0;
};
template <class Subject, typename Ret, typename A1, typename A2, typename A3>
struct PtmfHolder_3A : PtmfHolderBase_3A<Ret, A1, A2, A3> {
    virtual Ret operator()(A1 a1, A2 a2, A3 a3) const {
        return (subject->*ptmf.ptr)(a1, a2, a3);
    }
    Subject* subject;
    Ptmf_3A<Subject, Ret, A1, A2, A3> ptmf;
    template <class ToSubject, typename ToRet, typename ToA1, typename ToA2, typename ToA3>
    operator PtmfHolder_3A<ToSubject, ToRet, ToA1, ToA2, ToA3>&() {
        return reinterpret_cast<PtmfHolder_3A<ToSubject, ToRet, ToA1, ToA2, ToA3>&>(*this);
    }
    template <class ToSubject, typename ToRet, typename ToA1, typename ToA2, typename ToA3>
    operator const PtmfHolder_3A<ToSubject, ToRet, ToA1, ToA2, ToA3>&() const {
        return reinterpret_cast<const PtmfHolder_3A<ToSubject, ToRet, ToA1, ToA2, ToA3>&>(*this);
    }
};

class DoFuncsHook {
   protected:
    typedef void(Func)();
    typedef void (*Invoker)(void* funcPtr, void* a1, void* a2, void* a3);

    void* funcPtr;
    Invoker invoker;
    DoFuncsHook* next;

    static void Append(DoFuncsHook** prev, DoFuncsHook* self) {
        self->next = *prev;
        *prev = self;
    }

    static void Invoke0(void* f, void*, void*, void*) {
        reinterpret_cast<void (*)()>(f)();
    }

    template <typename A1>
    struct CastArg {
        static A1 from(void* p) { return reinterpret_cast<A1>(p); }
    };

    template <typename T>
    struct CastArg<T&> {
        static T& from(void* p) { return *reinterpret_cast<T*>(p); }
    };

    template <typename A1>
    static void Invoke1(void* f, void* a1, void*, void*) {
        reinterpret_cast<void (*)(A1)>(f)(CastArg<A1>::from(a1));
    }

    template <typename A1, typename A2>
    static void Invoke2(void* f, void* a1, void* a2, void*) {
        reinterpret_cast<void (*)(A1, A2)>(f)(CastArg<A1>::from(a1), CastArg<A2>::from(a2));
    }

    template <typename A1, typename A2, typename A3>
    static void Invoke3(void* f, void* a1, void* a2, void* a3) {
        reinterpret_cast<void (*)(A1, A2, A3)>(f)(CastArg<A1>::from(a1), CastArg<A2>::from(a2), CastArg<A3>::from(a3));
    }

    // 0-arg
    DoFuncsHook(void (*f)(), DoFuncsHook** prev) { Init(reinterpret_cast<void*>(f), &Invoke0, prev); }

    // 1-arg
    template <typename A1>
    DoFuncsHook(void (*f)(A1), DoFuncsHook** prev) { Init(reinterpret_cast<void*>(f), &Invoke1<A1>, prev); }

    // 2-arg
    template <typename A1, typename A2>
    DoFuncsHook(void (*f)(A1, A2), DoFuncsHook** prev) { Init(reinterpret_cast<void*>(f), &Invoke2<A1, A2>, prev); }

    // 3-arg
    template <typename A1, typename A2, typename A3>
    DoFuncsHook(void (*f)(A1, A2, A3), DoFuncsHook** prev) { Init(reinterpret_cast<void*>(f), &Invoke3<A1, A2, A3>, prev); }

    void Init(void* f, Invoker inv, DoFuncsHook** prev);

    static void Exec(DoFuncsHook* first, void* a1 = nullptr, void* a2 = nullptr, void* a3 = nullptr);
};

class RaceLoadHook : public DoFuncsHook {
    static DoFuncsHook* raceLoadHooks;

   public:
    template <typename F>
    RaceLoadHook(F f) : DoFuncsHook(f, &raceLoadHooks) {}
    static void Exec(void* a1 = nullptr, void* a2 = nullptr, void* a3 = nullptr) { DoFuncsHook::Exec(raceLoadHooks, a1, a2, a3); }
};

class PageLoadHook : public DoFuncsHook {
    static DoFuncsHook* pageLoadHooks;

   public:
    template <typename F>
    PageLoadHook(F f) : DoFuncsHook(f, &pageLoadHooks) {}
    static void Exec(void* a1 = nullptr, void* a2 = nullptr, void* a3 = nullptr) { DoFuncsHook::Exec(pageLoadHooks, a1, a2, a3); }
};

class RaceFrameHook : public DoFuncsHook {
    static DoFuncsHook* raceFrameHooks;

   public:
    template <typename F>
    RaceFrameHook(F f) : DoFuncsHook(f, &raceFrameHooks) {}
    static void Exec(void* a1 = nullptr, void* a2 = nullptr, void* a3 = nullptr) { DoFuncsHook::Exec(raceFrameHooks, a1, a2, a3); }
};

class SectionLoadHook : public DoFuncsHook {
    static DoFuncsHook* sHooks;

   public:
    template <typename F>
    SectionLoadHook(F f) : DoFuncsHook(f, &sHooks) {}
    static void Exec(void* a1 = nullptr, void* a2 = nullptr, void* a3 = nullptr) { DoFuncsHook::Exec(sHooks, a1, a2, a3); }
};

// REL has NOT loaded yet, so do NOT do anything with REL addr, it will not work
class BootHook {
   public:
    typedef void(Func)();
    Func* func;
    nw4r::ut::Link link;
    static nw4r::ut::List list;

   public:
    BootHook(Func* f, u16 position) {
        this->func = f;
        Func* obj = (Func*)nw4r::ut::List_GetNth(&list, position);
        if (obj == nullptr || position > list.count)
            nw4r::ut::List_Append(&list, this);
        else {
            nw4r::ut::List_Insert(&list, obj, this);
        }
    }

    static void Exec() {
        BootHook* next = nullptr;
        BootHook* cur = (BootHook*)nw4r::ut::List_GetNth(&list, 0);
        for (cur; cur != nullptr; cur = next) {
            cur->func();
            next = (BootHook*)nw4r::ut::List_GetNext(&list, cur);
        }
    }
};
#endif