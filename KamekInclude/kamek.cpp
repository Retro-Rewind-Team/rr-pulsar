#include <kamek.hpp>

SectionLoadHook* SectionLoadHook::sHooks = nullptr;
DoFuncsHook* RaceLoadHook::raceLoadHooks = nullptr;
DoFuncsHook* PageLoadHook::pageLoadHooks = nullptr;
DoFuncsHook* RaceFrameHook::raceFrameHooks = nullptr;

void DoFuncsHook::Init(void* f, Invoker inv, DoFuncsHook** prev) {
    this->funcPtr = f;
    this->invoker = inv;
    this->next = *prev;
    *prev = this;
}

void DoFuncsHook::Exec(DoFuncsHook* first, void* a1, void* a2, void* a3) {
    for (DoFuncsHook* p = first; p; p = p->next) {
        p->invoker(p->funcPtr, a1, a2, a3);
    }
}

nw4r::ut::List BootHook::list = {nullptr, nullptr, 0, offsetof(BootHook, link)};

// kmBranch(0x80001500, BootHook2::Exec);
kmCall(0x80543bb4, BootHook::Exec);  // 800074d4
kmBranch(0x80554728, RaceLoadHook::Exec);
kmBranch(0x8083822C, RaceLoadHook::Exec);
kmBranch(0x80601C5C, PageLoadHook::Exec);
kmBranch(0x8053369c, RaceFrameHook::Exec);  // Raceinfo::Update()
kmBranch(0x8063507c, SectionLoadHook::Exec);
