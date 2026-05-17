// Anchor-and-offset resolution for OEM symbols inside blmjciaapa.so.
//
// The shipped blmjciaapa.so (FW 74.00.324A NA) exports exactly one
// symbol in its dynamic symbol table: GetServiceInterfaces. Every
// other symbol we need is LOCAL in the static symbol table and is
// not reachable via dlsym. We resolve them by:
//
//   base = (uintptr_t)&GetServiceInterfaces
//          - blm_offsets_FW_74_00_324A::GetServiceInterfaces
//   addr = base + <target offset>
//
// All offsets here were harvested with the cross-toolchain nm against
// the shipped /jci/aapa/blmjciaapa.so from
// cmu150_NA_74.00.324A_update/. Re-run the harvest after any OEM
// firmware update before trusting these numbers.

#ifndef LIBPATCH_BLMJCIAAPA_OFFSETS_H
#define LIBPATCH_BLMJCIAAPA_OFFSETS_H

#include <stdint.h>

// Runtime-resolved address of blmjciaapa.so's GetServiceInterfaces
// symbol. Set once by main.cpp's constructor via
// dlsym(RTLD_DEFAULT, "GetServiceInterfaces"). NULL if we're not in
// the {L_jciAAPA} PID.
//
// We deliberately avoid declaring `extern "C" int GetServiceInterfaces()`
// here and taking `&GetServiceInterfaces` directly, because that would
// create an unresolved compile-time symbol reference that the
// dynamic linker would have to satisfy from blmjciaapa.so at load
// time — which works *iff* blmjciaapa.so is already mapped, but
// breaks `-Wl,--no-undefined` and any process that doesn't host the
// BLM. The dlsym lookup is one-shot and cheaper to reason about.
extern void *g_oem_anchor_GetServiceInterfaces;

namespace blm_offsets_FW_74_00_324A {

// Anchor — the only dynamically-exported symbol in blmjciaapa.so.
constexpr uintptr_t GetServiceInterfaces           = 0x00061768;

// Touch-side targets (5).
//   _ZN9SingletonI7AapProcE11GetInstanceEv
constexpr uintptr_t Singleton_AapProc_GetInstance  = 0x00062670;
//   _ZN7AapProc15GetVideoManagerEv
constexpr uintptr_t AapProc_GetVideoManager        = 0x0007f664;
//   _ZN7AapProc10GetRaceAapEv
constexpr uintptr_t AapProc_GetRaceAap             = 0x0007f614;
//   _ZN12VideoManager16IsAAVideoInFocusEv
constexpr uintptr_t VideoManager_IsAAVideoInFocus  = 0x000b3af4;
//   _ZN7RaceAap14SendTouchInputEP14AAP_TouchEvent
constexpr uintptr_t RaceAap_SendTouchInput         = 0x0008e3a8;

} // namespace blm_offsets_FW_74_00_324A

// Resolve a target offset through the GetServiceInterfaces anchor.
// Returns NULL if the anchor hasn't been set (caller should refuse
// to install hooks in that case — see main.cpp's self-gate).
//
// ARM EABI passes the implicit `this` pointer in r0 — identical to
// a regular first argument — so non-static C++ methods can be
// invoked through a plain function pointer with `void *` as the
// first param.
template <typename Fn>
static inline Fn *resolve_blm(uintptr_t target_offset)
{
    if (!g_oem_anchor_GetServiceInterfaces) return nullptr;
    uintptr_t base = reinterpret_cast<uintptr_t>(g_oem_anchor_GetServiceInterfaces)
                     - blm_offsets_FW_74_00_324A::GetServiceInterfaces;
    return reinterpret_cast<Fn *>(base + target_offset);
}

#endif  // LIBPATCH_BLMJCIAAPA_OFFSETS_H
