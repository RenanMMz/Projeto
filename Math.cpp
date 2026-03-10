#include "Math.h"

SweepResult SweptAABB(float bx, float by, float bw, float bh, float vx, float vy, float ox, float oy, float ow, float oh) {
    float xInvEntry, yInvEntry, xInvExit, yInvExit;

    if (vx > 0.0f) { xInvEntry = (ox - (bx + bw)); xInvExit = ((ox + ow) - bx); }
    else { xInvEntry = ((ox + ow) - bx); xInvExit = (ox - (bx + bw)); }

    if (vy > 0.0f) { yInvEntry = (oy - (by + bh)); yInvExit = ((oy + oh) - by); }
    else { yInvEntry = ((oy + oh) - by); yInvExit = (oy - (by + bh)); }
    
    SweepResult result;
    result.t = 1.0f;
    result.nx = result.ny = 0.0f;

    float xEntry, yEntry, xExit, yExit;
    if (vx == 0.0f) { 
        if (bx + bw <= ox || bx >= ox + ow) return result;
        xEntry = -INFINITY; xExit = INFINITY; }
    else { xEntry = xInvEntry / vx; xExit = xInvExit / vx; }

    if (vy == 0.0f) {
        if (by + bh <= oy || by >= oy + oh) return result;
        yEntry = -INFINITY; yExit = INFINITY; }
    else { yEntry = yInvEntry / vy; yExit = yInvExit / vy; }

    float entryTime = max(xEntry, yEntry);
    float exitTime = min(xExit, yExit);


    if (entryTime > exitTime || entryTime < 0.0f || entryTime > 1.0f)
        return result;

    result.t = entryTime;
    if (xEntry > yEntry) { result.nx = (vx < 0.0f) ? 1.0f : -1.0f; result.ny = 0.0f; }
    else { result.nx = 0.0f; result.ny = (vy < 0.0f) ? 1.0f : -1.0f; }

    return result;
}

bool CircleRectCollision(float cx, float cy, float radius, float rx, float ry, float rw, float rh) {
    float closestX = max(rx, min(cx, rx + rw));
    float closestY = max(ry, min(cy, ry + rh));
    float dx = cx - closestX;
    float dy = cy - closestY;
    return (dx * dx + dy * dy) < (radius * radius);
}