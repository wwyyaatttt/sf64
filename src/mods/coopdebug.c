#include "global.h"

/* Family Edition co-op test overlay.
 * Rows P1/P2: state, cockpit-view flag, draw flag, |x| |y| |trueZ| |zPath| shields.
 * Row C0/C1: controller hold buttons, stick x+100, stick y+100 for controllers 0 and 1.
 * Row BST: boostMeter, boostCooldown, U-turn brake/down timers (P1, then P2).
 * Row CAM: |P1 cam.eye.x| |P1 cam.eye.y| |P1 xPath| gPlayerNum gMainController. */
extern s32 gCoopGfxPeak[6]; // sys_main.c (test builds): graphics pool peaks
extern s32 gCoopHoldDbg[3]; // fox_play.c (test builds): hold timer ms, longest gap ms, restarts
extern s32 gCoopDlBad[12];  // sys_main.c (test builds): display-list validator (count, op, w0, w1, frame, depth, index, call chain)
#if MODS_COOP == 1
extern s32 gCoopLog[8][4]; // fox_play.c (co-op): last events, oldest first from gCoopLogN
extern s32 gCoopLogN;
#else
static s32 gCoopLog[8][4];
static s32 gCoopLogN = 0;
#endif

static s32 sCoopNan[4]; // count, player, byte offset, game frame of the first NaN/inf seen in a Player struct

#define COOP_MASTER_DL_CAP 0x1380 // GfxPool.masterDL entries (4992); sys_main.c adds 3 more after this overlay
s32 gCoopGamePeak[2];     // master display-list commands used by the game before this overlay: peak, game frame
static s32 sCoopOvlCut;   // frames where the overlay stopped early to keep the master list from overflowing

/* true when <need> more master display-list commands (plus a margin for the frame's end) still fit. Each overlay
 * character costs 10 commands; an overflow runs into unkDL2 and the frame never finishes (emulator hang). */
static s32 Coop_DbgRoom(s32 need) {
    return (COOP_MASTER_DL_CAP - (s32) (gMasterDisp - gGfxPool->masterDL)) > (need + 32);
}

/* Scan both players' float fields (positions, camera, rotations, joints, ...) for NaN / infinity. */
static void Coop_NanCheck(void) {
    static const u16 ranges[][2] = { { 0x000, 0x198 }, { 0x248, 0x260 }, { 0x2BC, 0x2C4 }, { 0x2C8, 0x478 }, { 0x4D8, 0x4DC } };
    s32 i;
    s32 r;
    u32 off;

    for (i = 0; i < gNumPlayers; i++) {
        for (r = 0; r < ARRAY_COUNT(ranges); r++) {
            for (off = ranges[r][0]; off < ranges[r][1]; off += 4) {
                u32 bits = *(u32*) ((u8*) &gPlayer[i] + off);

                if ((bits & 0x7F800000) == 0x7F800000) { // exponent all ones: NaN or +-inf
                    if (sCoopNan[0]++ == 0) {
                        sCoopNan[1] = i;
                        sCoopNan[2] = off;
                        sCoopNan[3] = gGameFrameCount;
                    }
                    return;
                }
            }
        }
    }
}

void Coop_DrawDebug(void) {
    s32 i;
    s32 y;
    Player* p;
    static char* sEvNames[] = { "", "PARK", "SPWN", "DOWN", "RGRP", "FOX" };

    static OSTime sFpsT0 = 0;
    static s32 sFpsFrames = 0;
    static s32 sFps = 0;
    OSTime now;

    if ((gGameState != GSTATE_PLAY) || (gDrawMode != DRAW_PLAY) || (gPlayer == NULL)) {
        return;
    }
    // game frames per second (game logic rate, not the 60 Hz video rate)
    now = osGetTime();
    sFpsFrames++;
    if ((sFpsT0 == 0) || (OS_CYCLES_TO_USEC(now - sFpsT0) > 3000000)) {
        sFpsT0 = now;
        sFpsFrames = 0;
    } else if (OS_CYCLES_TO_USEC(now - sFpsT0) >= 1000000) {
        sFps = sFpsFrames;
        sFpsFrames = 0;
        sFpsT0 = now;
    }
    Coop_NanCheck();
    // what the game itself used this frame (everything drawn before this overlay)
    i = gMasterDisp - gGfxPool->masterDL;
    if (i > gCoopGamePeak[0]) {
        gCoopGamePeak[0] = i;
        gCoopGamePeak[1] = gGameFrameCount;
    }
    if (!Coop_DbgRoom(250)) {
        goto cut;
    }
    RCP_SetupDL(&gMasterDisp, SETUPDL_83);
    gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 0, 255);
    // most important rows first: if the frame is busy, the rest of the overlay is dropped (see Coop_DbgRoom)
    // PK (top right, above the event log): peak master display-list commands the GAME used (before this overlay;
    // of 4992), the game frame it happened, peak matrices (of 1152), frames where this overlay was cut short so the
    // master list could not overflow (the overlay costs ~10 commands per character)
    Graphics_DisplaySmallText(190, 50, 1.0f, 1.0f, "PK");
    Graphics_DisplaySmallNumber(208, 50, gCoopGamePeak[0]);
    Graphics_DisplaySmallNumber(240, 50, gCoopGamePeak[1]);
    Graphics_DisplaySmallNumber(274, 50, gCoopGfxPeak[2]);
    Graphics_DisplaySmallNumber(302, 50, sCoopOvlCut);
    if (!Coop_DbgRoom(1000)) { // BAD + DL + NAN rows: up to ~90 characters
        goto cut;
    }
    // BAD (below the event log): display-list commands the validator rejected: count, opcode, depth, index, frame;
    // next row: the first bad command's words as hi/lo 16-bit halves (w0 hi, w0 lo, w1 hi, w1 lo)
    if (gCoopDlBad[0] != 0) {
        gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 80, 80, 255);
        Graphics_DisplaySmallText(190, 134, 1.0f, 1.0f, "BAD");
        Graphics_DisplaySmallNumber(214, 134, gCoopDlBad[0]);
        Graphics_DisplaySmallNumber(240, 134, gCoopDlBad[1]);
        Graphics_DisplaySmallNumber(262, 134, gCoopDlBad[5]);
        Graphics_DisplaySmallNumber(276, 134, gCoopDlBad[6]);
        Graphics_DisplaySmallNumber(300, 134, gCoopDlBad[4] % 10000);
        Graphics_DisplaySmallNumber(190, 143, (gCoopDlBad[2] >> 16) & 0xFFFF);
        Graphics_DisplaySmallNumber(224, 143, gCoopDlBad[2] & 0xFFFF);
        Graphics_DisplaySmallNumber(258, 143, (gCoopDlBad[3] >> 16) & 0xFFFF);
        Graphics_DisplaySmallNumber(292, 143, gCoopDlBad[3] & 0xFFFF);
        // DL: the G_DL calls that led to the bad list: target address at depth 0 (hi, lo) + its index in the master
        // list, then the same for depth 1; last: the full game frame
        Graphics_DisplaySmallText(190, 152, 1.0f, 1.0f, "DL");
        Graphics_DisplaySmallNumber(206, 152, (gCoopDlBad[7] >> 16) & 0xFFFF);
        Graphics_DisplaySmallNumber(236, 152, gCoopDlBad[7] & 0xFFFF);
        Graphics_DisplaySmallNumber(270, 152, gCoopDlBad[8]);
        Graphics_DisplaySmallNumber(206, 161, (gCoopDlBad[9] >> 16) & 0xFFFF);
        Graphics_DisplaySmallNumber(236, 161, gCoopDlBad[9] & 0xFFFF);
        Graphics_DisplaySmallNumber(270, 161, gCoopDlBad[10]);
        Graphics_DisplaySmallNumber(294, 161, gCoopDlBad[4]);
        gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 0, 255);
    }
    // NAN: NaN / infinity found in a Player struct: count, player, byte offset, game frame (overlay in red)
    if (sCoopNan[0] != 0) {
        gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 80, 80, 255);
        Graphics_DisplaySmallText(190, 170, 1.0f, 1.0f, "NAN");
        Graphics_DisplaySmallNumber(214, 170, sCoopNan[0]);
        Graphics_DisplaySmallNumber(246, 170, sCoopNan[1]);
        Graphics_DisplaySmallNumber(262, 170, sCoopNan[2]);
        Graphics_DisplaySmallNumber(294, 170, sCoopNan[3] % 10000);
        gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 0, 255);
    }
    // EVENT LOG (top right): the last 8 co-op events, newest at the bottom: name, frame, a, b
    for (i = 0; i < 8; i++) {
        s32* e = gCoopLog[(gCoopLogN + i) % 8];

        if ((e[0] > 0) && (e[0] < 6)) {
            if (!Coop_DbgRoom(220)) {
                goto cut;
            }
            y = 60 + i * 9;
            Graphics_DisplaySmallText(200, y, 1.0f, 1.0f, sEvNames[e[0]]);
            Graphics_DisplaySmallNumber(236, y, e[1]);
            Graphics_DisplaySmallNumber(272, y, (e[2] < 0) ? -e[2] : e[2]);
            Graphics_DisplaySmallNumber(296, y, (e[3] < 0) ? -e[3] : e[3]);
        }
    }
    if (!Coop_DbgRoom(600)) {
        goto cut;
    }
    for (i = 0; i < gNumPlayers; i++) {
        p = &gPlayer[i];
        y = 170 + i * 10;
        Graphics_DisplaySmallText(14, y, 1.0f, 1.0f, (i == 0) ? "P1" : "P2");
        Graphics_DisplaySmallNumber(30, y, p->state);
        Graphics_DisplaySmallNumber(48, y, p->alternateView);
        Graphics_DisplaySmallNumber(58, y, p->draw);
        Graphics_DisplaySmallNumber(72, y, (s32) fabsf(p->pos.x));
        Graphics_DisplaySmallNumber(112, y, (s32) fabsf(p->pos.y));
        Graphics_DisplaySmallNumber(152, y, (s32) fabsf(p->trueZpos));
        Graphics_DisplaySmallNumber(200, y, (s32) fabsf(p->zPath));
        Graphics_DisplaySmallNumber(256, y, p->shields);
    }
    if (!Coop_DbgRoom(350)) {
        goto cut;
    }
    for (i = 0; i < 2; i++) {
        y = 190 + i * 10;
        Graphics_DisplaySmallText(14, y, 1.0f, 1.0f, (i == 0) ? "C0" : "C1");
        Graphics_DisplaySmallNumber(40, y, gControllerHold[i].button);
        Graphics_DisplaySmallNumber(100, y, gControllerHold[i].stick_x + 100);
        Graphics_DisplaySmallNumber(140, y, gControllerHold[i].stick_y + 100);
        Graphics_DisplaySmallNumber(180, y, gControllerPlugged[i]);
    }
    if (!Coop_DbgRoom(350)) {
        goto cut;
    }
    Graphics_DisplaySmallText(14, 160, 1.0f, 1.0f, "FPS");
    Graphics_DisplaySmallNumber(40, 160, sFps);
    // emulated clock in tenths of a second (compare with screenshot wall-clock times to see emulator speed)
    Graphics_DisplaySmallText(70, 160, 1.0f, 1.0f, "T");
    Graphics_DisplaySmallNumber(80, 160, (s32) (OS_CYCLES_TO_USEC(now) / 100000));
    Graphics_DisplaySmallText(130, 160, 1.0f, 1.0f, "H");
    Graphics_DisplaySmallNumber(140, 160, gCoopHoldDbg[0]);
    Graphics_DisplaySmallNumber(180, 160, gCoopHoldDbg[1]);
    Graphics_DisplaySmallNumber(214, 160, gCoopHoldDbg[2]);
    if (!Coop_DbgRoom(350)) {
        goto cut;
    }
    Graphics_DisplaySmallText(14, 210, 1.0f, 1.0f, "CAM");
    Graphics_DisplaySmallNumber(40, 210, (s32) fabsf(gPlayer[0].cam.eye.x));
    Graphics_DisplaySmallNumber(80, 210, (s32) fabsf(gPlayer[0].cam.eye.y));
    Graphics_DisplaySmallNumber(120, 210, (s32) fabsf(gPlayer[0].xPath));
    Graphics_DisplaySmallNumber(170, 210, gPlayerNum);
    Graphics_DisplaySmallNumber(190, 210, gMainController);
    if (!Coop_DbgRoom(350)) {
        goto cut;
    }
    // BST: boost meter, boost cooldown, U-turn brake / down timers for P1 then P2
    Graphics_DisplaySmallText(14, 220, 1.0f, 1.0f, "BST");
    for (i = 0; i < gNumPlayers; i++) {
        Graphics_DisplaySmallNumber(40 + i * 130, 220, (s32) gPlayer[i].boostMeter);
        Graphics_DisplaySmallNumber(70 + i * 130, 220, gPlayer[i].boostCooldown);
        Graphics_DisplaySmallNumber(90 + i * 130, 220, gUturnBrakeTimers[i]);
        Graphics_DisplaySmallNumber(110 + i * 130, 220, gUturnDownTimers[i]);
    }
    if (!Coop_DbgRoom(200)) {
        goto cut;
    }
    // GFX: lights used (of 256), matrices used (of 1152), master display-list words used (of 4992), so far this frame
    Graphics_DisplaySmallText(190, 210, 1.0f, 1.0f, "GFX");
    Graphics_DisplaySmallNumber(216, 210, (s32) (gLight - gGfxPool->lights));
    Graphics_DisplaySmallNumber(244, 210, (s32) (gGfxMtx - gGfxPool->mtx));
    Graphics_DisplaySmallNumber(280, 210, (s32) (gMasterDisp - gGfxPool->masterDL));
    return;
cut:
    sCoopOvlCut++;
}
