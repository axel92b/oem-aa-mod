// SPDX-License-Identifier: AGPL-3.0-or-later
//
// maneuver_test — step through the HUD maneuver-glyph atlas on the OEM
// com.jci.vbs.navi service one glyph at a time under keyboard control,
// printing each code and labelling it on the HUD street strip so you can
// read off which number draws which arrow on the head-up display.
//
// === Raw libdbus-1 (NOT dbus-c++) ============================
//
// Unlike lane_test (which uses the dbus-c++ generated proxies from the
// local-only reference/dbus/generated_cmu.h), this tool talks to
// com.jci.vbs.navi with plain libdbus-1 method calls. That keeps it free
// of the un-committed generated header, so it builds from a clean
// checkout / in CI with only the sysroot's libdbus-1 — no local assets.
// The wire behaviour is identical: dbus-c++ ultimately issues the same
// libdbus-1 method calls with the same destination / path / interface.
//
// Two methods are sent per glyph, sharing one "sync" byte (cycled 1..7)
// so the HUD treats them as one generation:
//   com.jci.vbs.navi.SetHUDDisplayMsgReq( (uqyqyy) )   — maneuver frame
//   com.jci.vbs.navi.tmc.SetHUD_Display_Msg2( (sy) )   — street strip text
// both on object /com/jci/vbs/navi, destination "com.jci.vbs.navi".
//
// Interactive: starts at glyph 0 and waits. There is NO auto-switching —
// the glyph only changes when you press a key:
//   n  — show the NEXT glyph (index + 1) and print the index
//   p  — show the PREVIOUS glyph (index - 1)
//   q  — quit (Ctrl-C also works)
//
// Usage (run on the device, as root):
//   maneuver_test                 # start at 0, index range 0..255
//   maneuver_test <start>         # start at <start>
//   maneuver_test <start> <max>   # cap the index at <max>
//
// Env overrides:
//   HUD_DUNIT   distance unit shown   (default 1 = meters)

#include <dbus/dbus.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <termios.h>
#include <signal.h>

#define logd(fmt, ...) fprintf(stdout, "[maneuver_test] " fmt "\n", ##__VA_ARGS__)
#define loge(fmt, ...) fprintf(stderr, "[maneuver_test] " fmt "\n", ##__VA_ARGS__)

// com.jci.vbs.navi is hosted on the SERVICE bus.
#define SERVICE_BUS_ADDRESS "unix:path=/tmp/dbus_service_socket"
#define NAVI_DEST  "com.jci.vbs.navi"
#define NAVI_PATH  "/com/jci/vbs/navi"
#define NAVI_IFACE "com.jci.vbs.navi"
#define TMC_IFACE  "com.jci.vbs.navi.tmc"
#define CALL_TIMEOUT_MS 2000

// Mazda HUD distance-unit enum (HudDistanceUnit in the reference HUD
// headers): 1=METERS, 2=MILES, 3=KILOMETERS, 4=YARDS, 5=FEET.
enum HudDistanceUnit : unsigned char {
    METERS = 1, MILES = 2, KILOMETERS = 3, YARDS = 4, FEET = 5
};

// --- terminal raw-mode + clean shutdown -----------------------------
static struct termios        g_oldt;
static bool                  g_raw  = false;
static volatile sig_atomic_t g_quit = 0;

static void term_restore(void)
{
    if (g_raw) { tcsetattr(STDIN_FILENO, TCSANOW, &g_oldt); g_raw = false; }
}
static void on_sigint(int) { g_quit = 1; }

// Put stdin into single-keypress mode (no line buffering, no echo) so a
// bare 'n'/'p' is acted on immediately, without Enter. If stdin is not a
// tty this silently no-ops and keys then need a trailing Enter — the read
// loop works either way.
static void term_raw(void)
{
    struct termios newt;
    if (tcgetattr(STDIN_FILENO, &g_oldt) != 0) return;
    newt = g_oldt;
    newt.c_lflag &= ~(tcflag_t)(ICANON | ECHO);
    newt.c_cc[VMIN]  = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    g_raw = true;
}

static long env_long(const char *name, long dflt)
{
    const char *s = getenv(name);
    return (s && *s) ? strtol(s, NULL, 0) : dflt;
}

// Send a built method-call message and block for its reply. Takes
// ownership of msg (unrefs it). Returns true on a non-error reply.
static bool call_blocking(DBusConnection *conn, const char *iface,
                          const char *method, DBusMessage *msg)
{
    DBusError err;
    dbus_error_init(&err);
    DBusMessage *rep = dbus_connection_send_with_reply_and_block(
        conn, msg, CALL_TIMEOUT_MS, &err);
    dbus_message_unref(msg);
    if (rep == NULL) {
        loge("%s.%s failed: %s: %s", iface, method,
             err.name ? err.name : "?", err.message ? err.message : "?");
        dbus_error_free(&err);
        return false;
    }
    dbus_message_unref(rep);
    return true;
}

// com.jci.vbs.navi.SetHUDDisplayMsgReq( (uqyqyy) ) — the maneuver frame.
//   u nextManeuverInfo, q distanceValue, y distanceUnit,
//   q displaySpeedLimit, y displaySpeedUnit, y text_ID3 (sync).
static bool send_maneuver(DBusConnection *conn, int code, unsigned char dunit,
                          unsigned char sync)
{
    DBusMessage *msg = dbus_message_new_method_call(
        NAVI_DEST, NAVI_PATH, NAVI_IFACE, "SetHUDDisplayMsgReq");
    if (!msg) { loge("OOM building SetHUDDisplayMsgReq"); return false; }

    DBusMessageIter it, st;
    dbus_message_iter_init_append(msg, &it);
    dbus_message_iter_open_container(&it, DBUS_TYPE_STRUCT, NULL, &st);
    dbus_uint32_t maneuver = (dbus_uint32_t)code;        // the icon under test
    dbus_uint16_t dist     = (dbus_uint16_t)(code * 10); // distance = code
    dbus_uint16_t speed    = 0;
    unsigned char zero     = 0;
    dbus_message_iter_append_basic(&st, DBUS_TYPE_UINT32, &maneuver);
    dbus_message_iter_append_basic(&st, DBUS_TYPE_UINT16, &dist);
    dbus_message_iter_append_basic(&st, DBUS_TYPE_BYTE,   &dunit);
    dbus_message_iter_append_basic(&st, DBUS_TYPE_UINT16, &speed);
    dbus_message_iter_append_basic(&st, DBUS_TYPE_BYTE,   &zero);   // speed unit
    dbus_message_iter_append_basic(&st, DBUS_TYPE_BYTE,   &sync);   // text_ID3
    dbus_message_iter_close_container(&it, &st);
    return call_blocking(conn, NAVI_IFACE, "SetHUDDisplayMsgReq", msg);
}

// com.jci.vbs.navi.tmc.SetHUD_Display_Msg2( (sy) ) — the street strip.
//   s guidancePointName, y SyncBit.
static bool send_street(DBusConnection *conn, const char *text, unsigned char sync)
{
    DBusMessage *msg = dbus_message_new_method_call(
        NAVI_DEST, NAVI_PATH, TMC_IFACE, "SetHUD_Display_Msg2");
    if (!msg) { loge("OOM building SetHUD_Display_Msg2"); return false; }

    DBusMessageIter it, st;
    dbus_message_iter_init_append(msg, &it);
    dbus_message_iter_open_container(&it, DBUS_TYPE_STRUCT, NULL, &st);
    dbus_message_iter_append_basic(&st, DBUS_TYPE_STRING, &text);
    dbus_message_iter_append_basic(&st, DBUS_TYPE_BYTE,   &sync);
    dbus_message_iter_close_container(&it, &st);
    return call_blocking(conn, TMC_IFACE, "SetHUD_Display_Msg2", msg);
}

// One (glyph -> HUD) generation: street strip label + maneuver frame.
static bool show_glyph(DBusConnection *conn, int code, unsigned char sync,
                       unsigned char dunit)
{
    char street[64];
    snprintf(street, sizeof(street), "GLYPH %d  0x%02X", code, code);
    const char *s = street;

    bool ok = send_street(conn, s, sync) &&
              send_maneuver(conn, code, dunit, sync);
    if (ok)
        logd("glyph=%3d (0x%02x) sync=%u street=\"%s\"",
             code, code, sync, street);
    return ok;
}

int main(int argc, char **argv)
{
    int start = 0, maxidx = 255;
    if (argc >= 2) start  = atoi(argv[1]);
    if (argc >= 3) maxidx = atoi(argv[2]);
    if (start < 0)  start  = 0;
    if (maxidx < 0) maxidx = 0;
    if (start > maxidx) start = maxidx;

    unsigned char dunit = (unsigned char)env_long("HUD_DUNIT", METERS);

    DBusError err;
    dbus_error_init(&err);
    DBusConnection *conn = dbus_connection_open_private(SERVICE_BUS_ADDRESS, &err);
    if (conn == NULL) {
        loge("open %s failed: %s: %s", SERVICE_BUS_ADDRESS,
             err.name ? err.name : "?", err.message ? err.message : "?");
        dbus_error_free(&err);
        return 1;
    }
    if (!dbus_bus_register(conn, &err)) {
        loge("bus register failed: %s: %s",
             err.name ? err.name : "?", err.message ? err.message : "?");
        dbus_error_free(&err);
        dbus_connection_close(conn);
        dbus_connection_unref(conn);
        return 1;
    }
    // A torn socket on a source switch should not kill this tool.
    dbus_connection_set_exit_on_disconnect(conn, 0 /* FALSE */);

    logd("connected to %s on the service bus", NAVI_DEST);
    logd("interactive glyph stepper (index range %d..%d)", start, maxidx);
    logd("keys:  n = next   p = prev   q = quit   (no auto-switching)");

    // Clean shutdown: SIGINT just flips g_quit so the blocking read()
    // returns EINTR and we fall through to restore the terminal.
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;            // no SA_RESTART: we want read() interrupted
    sigaction(SIGINT, &sa, NULL);
    term_raw();

    unsigned char sync = 0;    // cycles 1..7 so each frame is a new generation
    int idx = start;

    // Show the starting glyph (0 by default) and then wait for keys.
    sync = (unsigned char)((sync % 7) + 1);
    show_glyph(conn, idx, sync, dunit);

    while (!g_quit) {
        char ch;
        ssize_t k = read(STDIN_FILENO, &ch, 1);
        if (k <= 0) {
            if (k < 0 && errno == EINTR) continue;  // SIGINT -> re-test g_quit
            break;                                   // EOF / error
        }

        if (ch == 'n' || ch == 'N') {
            if (idx >= maxidx) { logd("already at max index %d", maxidx); continue; }
            ++idx;
        } else if (ch == 'p' || ch == 'P') {
            if (idx <= 0) { logd("already at min index 0"); continue; }
            --idx;
        } else if (ch == 'q' || ch == 'Q') {
            break;
        } else {
            continue;  // ignore any other key
        }

        sync = (unsigned char)((sync % 7) + 1);
        if (!show_glyph(conn, idx, sync, dunit)) break;
    }

    term_restore();
    logd("exiting at glyph %d", idx);
    dbus_connection_close(conn);
    dbus_connection_unref(conn);
    return 0;
}
