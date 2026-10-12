#include "global.h"

/* Family Edition co-op test overlay.
 * Rows P1/P2: state, cockpit-view flag, draw flag, |x| |y| |trueZ| |zPath| shields.
 * Row C0/C1: controller hold buttons, stick x+100, stick y+100 for controllers 0 and 1.
 * Row BST: boostMeter, boostCooldown, U-turn brake/down timers (P1, then P2).
 * Row CAM: |P1 cam.eye.x| |P1 cam.eye.y| |P1 xPath| gPlayerNum gMainController. */
void Coop_DrawDebug(void) {
    s32 i;
    s32 y;
    Player* p;

    if ((gGameState != GSTATE_PLAY) || (gDrawMode != DRAW_PLAY) || (gPlayer == NULL)) {
        return;
    }
    RCP_SetupDL(&gMasterDisp, SETUPDL_83);
    gDPSetPrimColor(gMasterDisp++, 0, 0, 255, 255, 0, 255);
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
    for (i = 0; i < 2; i++) {
        y = 190 + i * 10;
        Graphics_DisplaySmallText(14, y, 1.0f, 1.0f, (i == 0) ? "C0" : "C1");
        Graphics_DisplaySmallNumber(40, y, gControllerHold[i].button);
        Graphics_DisplaySmallNumber(100, y, gControllerHold[i].stick_x + 100);
        Graphics_DisplaySmallNumber(140, y, gControllerHold[i].stick_y + 100);
        Graphics_DisplaySmallNumber(180, y, gControllerPlugged[i]);
    }
    Graphics_DisplaySmallText(14, 210, 1.0f, 1.0f, "CAM");
    Graphics_DisplaySmallNumber(40, 210, (s32) fabsf(gPlayer[0].cam.eye.x));
    Graphics_DisplaySmallNumber(80, 210, (s32) fabsf(gPlayer[0].cam.eye.y));
    Graphics_DisplaySmallNumber(120, 210, (s32) fabsf(gPlayer[0].xPath));
    Graphics_DisplaySmallNumber(170, 210, gPlayerNum);
    Graphics_DisplaySmallNumber(190, 210, gMainController);
    // BST: boost meter, boost cooldown, U-turn brake / down timers for P1 then P2
    Graphics_DisplaySmallText(14, 220, 1.0f, 1.0f, "BST");
    for (i = 0; i < gNumPlayers; i++) {
        Graphics_DisplaySmallNumber(40 + i * 130, 220, (s32) gPlayer[i].boostMeter);
        Graphics_DisplaySmallNumber(70 + i * 130, 220, gPlayer[i].boostCooldown);
        Graphics_DisplaySmallNumber(90 + i * 130, 220, gUturnBrakeTimers[i]);
        Graphics_DisplaySmallNumber(110 + i * 130, 220, gUturnDownTimers[i]);
    }
}
