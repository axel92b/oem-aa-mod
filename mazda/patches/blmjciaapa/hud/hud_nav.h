// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Adapted from headunit (https://github.com/Trevelopment/headunit),
// licensed under GNU AGPL v3.
// See NOTICE.md at the repo root for the full attribution.
//
// Shared HUD navigation mapping — the transport-agnostic translation
// between Android Auto nav fields and the Mazda HUD ECU's own
// encodings. Both HUD transports (vbs_tx and svcnavi_tx) need exactly
// the same glyph table and unit mapping (svcjcinavi forwards dirIcon
// and distUnit straight through as the HUD frame's nextManeuverInfo
// and distanceUnit, so the codes are identical on both paths), so it
// lives here instead of being duplicated.
//
// Header-only: everything is constexpr data or pure inline functions
// with no transport coupling and no global state, so there is no
// matching .cpp and no link-time concern.

#ifndef LIBPATCH_BLMJCIAAPA_HUD_NAV_H
#define LIBPATCH_BLMJCIAAPA_HUD_NAV_H

#include <stdint.h>

// === Mazda HUD turn-icon enum =================================
//
// Mirrors `NaviTurns` in reference hud.h. These are the integer
// codes the HUD ECU interprets — the OEM glyph atlas is baked
// into the ECU firmware and is out of our scope. Same numbering
// as the reference, no remapping.
enum MazdaIcon : uint8_t {
    HUD_STRAIGHT            = 1,
    HUD_LEFT                = 2,
    HUD_RIGHT               = 3,
    HUD_SLIGHT_LEFT         = 4,
    HUD_SLIGHT_RIGHT        = 5,
    HUD_UNDER_BRIDGE        = 6,
    HUD_OFF_RAMP_RIGHT      = 7,
    HUD_DESTINATION         = 8,
    HUD_SHARP_RIGHT         = 9,
    HUD_U_TURN_RIGHT        = 10,
    HUD_SHARP_LEFT          = 11,
    HUD_FLAG                = 12,
    HUD_U_TURN_LEFT         = 13,
    HUD_FORK_RIGHT          = 14,
    HUD_FORK_LEFT           = 15,
    HUD_MERGE_LEFT          = 16,
    HUD_MERGE_RIGHT         = 17,
    HUD_EMPTY               = 18,
    // 19 is empty
    HUD_CROSS_RIGHT         = 20,
    HUD_CROSS_LEFT          = 21,
    HUD_MEDIAN_U_TURN_LEFT  = 22,
    HUD_MEDIAN_U_TURN_RIGHT = 23,
    HUD_CAR                 = 24,
    HUD_NO_CAR              = 25,
    // 26..29 are empty
    HUD_OFF_RAMP_LEFT       = 30,
    HUD_T_LEFT              = 31,
    HUD_T_RIGHT             = 32,
    HUD_DESTINATION_LEFT    = 33,
    HUD_DESTINATION_RIGHT   = 34,
    HUD_FLAG_LEFT           = 35,
    HUD_FLAG_RIGHT          = 36,
};

// === Android turn_event → Mazda icon ==========================
//
// Lookup: kTurnIcons[android_turn_event][side_index]
//   side_index: 0=LEFT, 1=RIGHT, 2=UNSPECIFIED/STRAIGHT
//
// Indexed by hu.proto NAVTurnMessage.TURN_EVENT (0..19, sparse at
// 15 and 18). A `0` entry means "no glyph" — the HUD draws blank.
// All three roundabout events — ROUNDABOUT_ENTER (11),
// ROUNDABOUT_EXIT (12) and ROUNDABOUT_ENTER_AND_EXIT (13) — are
// handled separately by roundabout_icon() because the icon depends
// on the exit angle, so their rows below are left blank on purpose.
//
// This table is copied verbatim from reference hud.cpp's turns[][]
// (just renamed). The reference table has been validated on real
// cars; do not "improve" it without a road test.
constexpr uint8_t kTurnIcons[20][3] = {
    /*  0 TURN_UNKNOWN                  */ {0, 0, 0},
    /*  1 TURN_DEPART                   */ {HUD_FLAG_LEFT, HUD_FLAG_RIGHT, HUD_FLAG},
    /*  2 TURN_NAME_CHANGE              */ {HUD_STRAIGHT, HUD_STRAIGHT, HUD_STRAIGHT},
    /*  3 TURN_SLIGHT_TURN              */ {HUD_SLIGHT_LEFT, HUD_SLIGHT_RIGHT, HUD_STRAIGHT},
    /*  4 TURN_TURN                     */ {HUD_LEFT, HUD_RIGHT, HUD_STRAIGHT},
    /*  5 TURN_SHARP_TURN               */ {HUD_SHARP_LEFT, HUD_SHARP_RIGHT, HUD_STRAIGHT},
    /*  6 TURN_U_TURN                   */ {HUD_U_TURN_LEFT, HUD_U_TURN_RIGHT, HUD_STRAIGHT},
    /*  7 TURN_ON_RAMP                  */ {HUD_LEFT, HUD_RIGHT, HUD_STRAIGHT},
    /*  8 TURN_OFF_RAMP                 */ {HUD_OFF_RAMP_LEFT, HUD_OFF_RAMP_RIGHT, HUD_STRAIGHT},
    /*  9 TURN_FORK                     */ {HUD_FORK_LEFT, HUD_FORK_RIGHT, HUD_STRAIGHT},
    /* 10 TURN_MERGE                    */ {HUD_MERGE_LEFT, HUD_MERGE_RIGHT, HUD_STRAIGHT},
    /* 11 TURN_ROUNDABOUT_ENTER         */ {0, 0, 0},  // handled by roundabout_icon()
    /* 12 TURN_ROUNDABOUT_EXIT          */ {0, 0, 0},  // handled by roundabout_icon()
    /* 13 TURN_ROUNDABOUT_ENTER_AND_EXIT*/ {0, 0, 0},  // handled by roundabout_icon()
    /* 14 TURN_STRAIGHT                 */ {HUD_STRAIGHT, HUD_STRAIGHT, HUD_STRAIGHT},
    /* 15 unassigned in proto           */ {0, 0, 0},
    /* 16 TURN_FERRY_BOAT               */ {0, 0, 0},
    /* 17 TURN_FERRY_TRAIN              */ {0, 0, 0},
    /* 18 unassigned in proto           */ {0, 0, 0},
    /* 19 TURN_DESTINATION              */ {HUD_DESTINATION_LEFT, HUD_DESTINATION_RIGHT, HUD_DESTINATION},
};

// Roundabout exit icon — 12 directional roundabout glyphs per
// side, indexed by exit angle (rounded to nearest 30°). Offsets
// 37 (right-hand traffic) and 49 (left-hand traffic) come from
// the reference implementation; the HUD ECU has roundabout
// glyphs at IDs 37..48 and 49..60.
//
// `degrees` is the proto turn_angle, a SIGNED int32 with several
// values the raw reference formula `(degrees+15)/30` mishandled:
//   * Google Maps sends NEGATIVE angles for exits left of straight
//     (e.g. -90 for a 90°-left exit). Truncating integer division of
//     a negative numerator ran off the bottom of the 12-glyph block
//     into unrelated (non-roundabout) glyph IDs — the reason left-hand
//     exits rendered wrong.
//   * The producer forwards -1 when the phone omits turn_angle.
//   * An exit near 360° (or >360) overflowed past glyph 48 into the
//     other traffic side's block.
// Normalise into [0,360) and wrap the bucket mod 12 so the result is
// always a valid 0..11 index, i.e. icon 37..48 / 49..60. A negative
// angle maps to the same glyph as its positive co-terminal equivalent
// (-90 ≡ 270), which is geometrically correct.
inline uint8_t roundabout_icon(int32_t degrees, int32_t side_index_lr)
{
    int32_t norm = degrees % 360;
    if (norm < 0) norm += 360;
    uint8_t nearest = static_cast<uint8_t>(((norm + 15) / 30) % 12);
    uint8_t offset  = (side_index_lr == 0) ? 49 : 37;
    return static_cast<uint8_t>(nearest + offset);
}

// Approximate a roundabout exit bearing from the 1-based exit number,
// for phones that don't send a usable turn_angle. Verified on-device:
// some nav apps always send turn_angle=0 on roundabouts and put the
// guidance in turn_number instead — a logged Israeli (right-hand-traffic)
// route showed every roundabout as turn_event=13, turn_angle=0,
// turn_number=3. Without this, all roundabouts collapsed to bucket 0 (the
// "U-turn around the island" glyph) and looked identical.
//
// The first three exits map to the intuitive bearings (the exit angle is
// measured the same way the proto turn_angle is — ~180° = straight
// through, 0/360° = back/U-turn):
//   exit 1 ->  90°  (first exit — a right-ish turn in RH traffic)
//   exit 2 -> 180°  (straight through — the most common action)
//   exit 3 -> 270°  (a left-ish turn)
// The 4th exit and beyond map to 360° == bucket 0, the roundabout U-turn
// glyph ("go all the way around the island"), instead of continuing with
// exit*90. A plain exit*90 would, for exit 5+, wrap (450°->90°) and alias
// a late exit onto the 1st-exit glyph. We can't know the real angle of a
// 5th/6th exit without the total exit count — which this protocol does not
// carry (see NAVTurnMessage: only turn_number, no total) — so every late
// exit shows the single "around the roundabout" glyph; the distance
// countdown tells the driver when to leave. Returning a constant also
// bounds the result so a garbage exit number can't overflow int32.
// roundabout_icon() does the left/right-hand mirror via its offset, so
// this stays handedness-agnostic. It's a coarse hint anyway — the HUD has
// only 30° glyph resolution — used solely when no real angle is available.
inline int32_t roundabout_exit_angle(int32_t exit_number)
{
    if (exit_number <= 1) return 90;   // 1st exit (right-ish); floor for <1
    if (exit_number == 2) return 180;  // straight through
    if (exit_number == 3) return 270;  // left-ish
    return 360;                        // 4th and beyond: roundabout U-turn (bucket 0)
}

// === Distance-unit enum translation ===========================
//
// hu.proto DISPLAY_DISTANCE_UNIT:
//   1=METERS, 2=KILOMETERS10, 3=KILOMETERS, 4=MILES10,
//   5=MILES,  6=FEET
//
// Mazda HUD (HudDistanceUnit in reference hud.h):
//   1=METERS, 2=MILES, 3=KILOMETERS, 4=YARDS, 5=FEET
//
// The Mazda HUD doesn't distinguish the "rounded > 10" sub-units
// — it always shows one decimal and lets the magnitude speak for
// itself. So KILOMETERS10 and KILOMETERS both map to Mazda
// KILOMETERS, etc.
inline uint8_t map_distance_unit(uint32_t android_unit)
{
    switch (android_unit) {
    case 1: return 1;  // METERS       -> METERS
    case 2: return 3;  // KILOMETERS10 -> KILOMETERS
    case 3: return 3;  // KILOMETERS   -> KILOMETERS
    case 4: return 2;  // MILES10      -> MILES
    case 5: return 2;  // MILES        -> MILES
    case 6: return 5;  // FEET         -> FEET
    default: return 0; // Unknown — HUD will render nothing.
    }
}

// === Turn-icon resolution =====================================
//
// Resolve the HUD glyph from the proto turn fields. Pure function of
// its inputs (the three nav fields it reads), so it is independent of
// either transport's snapshot struct.
//   turn_event - proto TURN_EVENT (0..19 sparse)
//   turn_side  - proto TURN_SIDE  (1=L, 2=R, 3=U)
//   turn_angle - degrees (only used for roundabout exit angle)
//   turn_number- maneuver / roundabout exit number (used for the
//                roundabout glyph when no real angle is sent)
//
// A return of 0 means "no glyph for this maneuver", but 0 is more than a
// blank icon downstream — it doubles as "no active maneuver", and that
// takes the DISTANCE/ETA down with it:
//   * vbs/ECU: the maneuver code and the distance share ONE 12-byte HUD
//     frame (VbsNaviHudDisplay); a 0 maneuver makes the HUD ECU discard
//     the whole frame, so the distance disappears together with the icon.
//   * svcjcinavi merge: a 0 maneuver on an AAP-sentinel frame is the
//     explicit "AAP guidance stopped" signal (svcjcinavi/merge.cpp),
//     which relinquishes the HUD back to OEM nav and drops the AAP
//     distance and street too.
// So any maneuver we actually want on the HUD MUST resolve to a non-zero
// glyph. This is why the roundabout events route through roundabout_icon()
// (which never returns 0, only 37..48 / 49..60) instead of their blank
// kTurnIcons rows — a 0 here was exactly why roundabouts showed no
// distance either.
inline uint32_t compute_turn_icon(uint32_t turn_event, uint32_t turn_side,
                                  int32_t turn_angle, int32_t turn_number)
{
    // All three roundabout events resolve to a directional roundabout
    // glyph — the kTurnIcons rows for 11/12/13 are intentionally blank.
    // WHICH event the phone sends is app-specific: Google Maps favours the
    // combined ENTER_AND_EXIT (13), while Waze (and Maps for some
    // manoeuvres) splits it into a separate ROUNDABOUT_ENTER (11) /
    // ROUNDABOUT_EXIT (12). They must all be routed here, otherwise the
    // table's {0,0,0} rows draw a blank HUD — which is why roundabouts
    // showed nothing at all under Waze.
    if (turn_event == 11 /*TURN_ROUNDABOUT_ENTER*/ ||
        turn_event == 12 /*TURN_ROUNDABOUT_EXIT*/ ||
        turn_event == 13 /*TURN_ROUNDABOUT_ENTER_AND_EXIT*/) {
        // side_index: 0=left-hand traffic, 1=right-hand. Convert
        // proto TURN_SIDE (1=L, 2=R, 3=U) to that binary —
        // UNSPECIFIED falls back to right-hand, matching the
        // reference's `side - 1`.
        int32_t side_lr = (turn_side == 1) ? 0 : 1;

        // Prefer a real exit bearing when the phone actually sends one.
        // Treat 0 AND the -1 "absent" sentinel as "no angle": verified
        // on-device, some apps always send turn_angle=0 on roundabouts
        // (conveying the exit via turn_number instead), and the producer
        // forwards -1 when the field is omitted — both must fall through
        // to the exit-number heuristic rather than resolve to the
        // misleading bucket-0 U-turn glyph. Google Maps' genuine signed
        // bearings (e.g. -90 for a left exit) are non-zero/non-(-1) and
        // are still used directly.
        if (turn_angle != 0 && turn_angle != -1) {
            return roundabout_icon(turn_angle, side_lr);
        }
        // No usable angle: derive the glyph from the exit number when we
        // have one (1-based), else fall back to the neutral
        // straight-through glyph (~180°) rather than the U-turn one.
        if (turn_number >= 1) {
            return roundabout_icon(roundabout_exit_angle(turn_number), side_lr);
        }
        return roundabout_icon(180, side_lr);
    }
    if (turn_event < 20) {
        int32_t side_idx = static_cast<int32_t>(turn_side) - 1;
        if (side_idx < 0 || side_idx > 2) side_idx = 2;
        return kTurnIcons[turn_event][side_idx];
    }
    return 0;
}

#endif // LIBPATCH_BLMJCIAAPA_HUD_NAV_H
