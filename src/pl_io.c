/*
   pl_io.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] context
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_io.h"
#include "pl_ds.h"
#include "pl_string.h"
#include <math.h>   // floorf
#include <string.h> // memset
#include <float.h>  // FLT_MAX

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

typedef enum
{
    PL_INPUT_EVENT_TYPE_NONE = 0,
    PL_INPUT_EVENT_TYPE_MOUSE_POS,
    PL_INPUT_EVENT_TYPE_MOUSE_WHEEL,
    PL_INPUT_EVENT_TYPE_MOUSE_BUTTON,
    PL_INPUT_EVENT_TYPE_KEY,
    PL_INPUT_EVENT_TYPE_TEXT,
    
    PL_INPUT_EVENT_TYPE_COUNT
} _plInputEventType;

typedef enum
{
    PL_INPUT_EVENT_SOURCE_NONE = 0,
    PL_INPUT_EVENT_SOURCE_MOUSE,
    PL_INPUT_EVENT_SOURCE_KEYBOARD,
    
    PL_INPUT_EVENT_SOURCE_COUNT
} _plInputEventSource;

//-----------------------------------------------------------------------------
// [SECTION] global context
//-----------------------------------------------------------------------------

static plIOContext* gptIOContext = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api 
//-----------------------------------------------------------------------------

#define PL_IO_VEC2_LENGTH_SQR(vec) (((vec).x * (vec).x) + ((vec).y * (vec).y))
#define PL_IO_VEC2_SUBTRACT(v1, v2) (plVec2){ (v1).x - (v2).x, (v1).y - (v2).y}
#define PL_IO_VEC2(v1, v2) (plVec2){(v1), (v2)}
#define PL_IO_MAX(x, y) (x) > (y) ? (x) : (y)

static void          pl__update_events(void);
static void          pl__update_mouse_inputs(void);
static void          pl__update_keyboard_inputs(void);
static int           pl__calc_typematic_repeat_amount(float fT0, float fT1, float fRepeatDelay, float fRepeatRate);
static plInputEvent* pl__get_last_event(plInputEventType tType, int iButtonOrKey);
static plInputEvent* pl__get_event(void);

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

void
pl_set_io_context(plIOContext* ptCtx)
{
    gptIOContext = ptCtx;
}

plIOContext*
pl_get_io_context(void)
{
    if(gptIOContext == NULL)
    {
        static plIOContext tIOContext;
        memset(&tIOContext, 0, sizeof(plIOContext));
        gptIOContext = &tIOContext;
        gptIOContext->fMouseDoubleClickTime    = 0.3f;
        gptIOContext->fMouseDoubleClickMaxDist = 6.0f;
        gptIOContext->fMouseDragThreshold      = 6.0f;
        gptIOContext->fKeyRepeatDelay          = 0.275f;
        gptIOContext->fKeyRepeatRate           = 0.050f;
        
        gptIOContext->_sbtInputEvents = gptIOContext->_atInputEvents;
        gptIOContext->_uInputEventCapacity = 64;
        gptIOContext->afMainFramebufferScale[0] = 1.0f;
        gptIOContext->afMainFramebufferScale[1] = 1.0f;

        gptIOContext->tCurrentCursor = PL_MOUSE_CURSOR_ARROW;
        gptIOContext->tNextCursor = gptIOContext->tCurrentCursor;
        gptIOContext->afMainViewportSize[0] = 500.0f;
        gptIOContext->afMainViewportSize[1] = 500.0f;
        gptIOContext->bViewportSizeChanged = true;
    }
    return gptIOContext;
}

void
pl_new_io_frame(void)
{
    plIOContext* ptIO = gptIOContext;

    ptIO->dTime += (double)ptIO->fDeltaTime;
    ptIO->ulFrameCount++;
    ptIO->bViewportSizeChanged = false;
    ptIO->bWantTextInput = false;
    ptIO->bWantCaptureMouse = false;
    ptIO->bWantCaptureKeyboard = false;

    // calculate frame rate
    ptIO->_fFrameRateSecPerFrameAccum += ptIO->fDeltaTime - ptIO->_afFrameRateSecPerFrame[ptIO->_iFrameRateSecPerFrameIdx];
    ptIO->_afFrameRateSecPerFrame[ptIO->_iFrameRateSecPerFrameIdx] = ptIO->fDeltaTime;
    ptIO->_iFrameRateSecPerFrameIdx = (ptIO->_iFrameRateSecPerFrameIdx + 1) % 120;
    ptIO->_iFrameRateSecPerFrameCount = PL_IO_MAX(ptIO->_iFrameRateSecPerFrameCount, 120);
    ptIO->fFrameRate = FLT_MAX;
    if(ptIO->_fFrameRateSecPerFrameAccum > 0) ptIO->fFrameRate = ((float) ptIO->_iFrameRateSecPerFrameCount) / ptIO->_fFrameRateSecPerFrameAccum;

    pl__update_events();
    pl__update_keyboard_inputs();
    pl__update_mouse_inputs();
}

void
pl_end_io_frame(void)
{
    gptIOContext->_fMouseWheel = 0.0f;
    gptIOContext->_fMouseWheelH = 0.0f;
    pl_sb_reset(gptIOContext->_sbInputQueueCharacters);
}

plKeyData*
pl_get_key_data(plKey tKey)
{
    if(tKey & PL_KEY_MOD_MASK_)
    {
        if     (tKey == PL_KEY_MOD_CTRL)  tKey = PL_KEY_RESERVED_MOD_CTRL;
        else if(tKey == PL_KEY_MOD_SHIFT) tKey = PL_KEY_RESERVED_MOD_SHIFT;
        else if(tKey == PL_KEY_MOD_ALT)   tKey = PL_KEY_RESERVED_MOD_ALT;
        else if(tKey == PL_KEY_MOD_SUPER) tKey = PL_RESERVED_KEY_MOD_SUPER;
        else if(tKey == PL_KEY_MOD_SHORTCUT) tKey = (gptIOContext->bConfigMacOSXBehaviors ? PL_RESERVED_KEY_MOD_SUPER : PL_KEY_RESERVED_MOD_CTRL);
    }
    PL_ASSERT(tKey > PL_KEY_NONE && tKey < PL_KEY_COUNT && "Key not valid");
    return &gptIOContext->_tKeyData[tKey];
}

void
pl_add_key_event(plKey tKey, bool bDown)
{
    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_KEY, (int)tKey);
    if(ptLastEvent && ptLastEvent->bKeyDown == bDown)
        return;

    plInputEvent* ptEvent = pl__get_event();
    ptEvent->tType    = PL_INPUT_EVENT_TYPE_KEY;
    ptEvent->tSource  = PL_INPUT_EVENT_SOURCE_KEYBOARD;
    ptEvent->tKey     = tKey;
    ptEvent->bKeyDown = bDown;
}

void
pl_add_text_event(uint32_t uChar)
{
    plInputEvent* ptEvent = pl__get_event();
    ptEvent->tType    = PL_INPUT_EVENT_TYPE_TEXT;
    ptEvent->tSource  = PL_INPUT_EVENT_SOURCE_KEYBOARD;
    ptEvent->uChar    = uChar;
}

void
pl_add_text_event_utf16(uint16_t uChar)
{
    if (uChar == 0 && gptIOContext->_tInputQueueSurrogate == 0)
        return;

    if ((uChar & 0xFC00) == 0xD800) // High surrogate, must save
    {
        if (gptIOContext->_tInputQueueSurrogate != 0)
            pl_add_text_event(0xFFFD);
        gptIOContext->_tInputQueueSurrogate = uChar;
        return;
    }

    plWChar cp = uChar;
    if (gptIOContext->_tInputQueueSurrogate != 0)
    {
        if ((uChar & 0xFC00) != 0xDC00) // Invalid low surrogate
        {
            pl_add_text_event(0xFFFD);
        }
        else
        {
            cp = 0xFFFD; // Codepoint will not fit in ImWchar
        }

        gptIOContext->_tInputQueueSurrogate = 0;
    }
    pl_add_text_event((uint32_t)cp);
}

void
pl_add_text_events_utf8(const char* pcText)
{
    while(*pcText != 0)
    {
        uint32_t uChar = 0;
        pcText += pl_text_char_from_utf8(&uChar, pcText, NULL);
        pl_add_text_event(uChar);
    }
}

void
pl_add_mouse_pos_event(float fX, float fY)
{

    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_MOUSE_POS, -1);
    if(ptLastEvent && ptLastEvent->fPosX == fX && ptLastEvent->fPosY == fY)
        return;

    plInputEvent* ptEvent = pl__get_event();
    ptEvent->tType   = PL_INPUT_EVENT_TYPE_MOUSE_POS;
    ptEvent->tSource = PL_INPUT_EVENT_SOURCE_MOUSE;
    ptEvent->fPosX   = fX;
    ptEvent->fPosY   = fY;
}

void
pl_add_mouse_button_event(int iButton, bool bDown)
{

    // check for duplicate
    const plInputEvent* ptLastEvent = pl__get_last_event(PL_INPUT_EVENT_TYPE_MOUSE_BUTTON, iButton);
    if(ptLastEvent && ptLastEvent->bMouseDown == bDown)
        return;

    plInputEvent* ptEvent = pl__get_event();
    ptEvent->tType      = PL_INPUT_EVENT_TYPE_MOUSE_BUTTON;
    ptEvent->tSource    = PL_INPUT_EVENT_SOURCE_MOUSE;
    ptEvent->iButton    = iButton;
    ptEvent->bMouseDown = bDown;
}

void
pl_add_mouse_wheel_event(float fX, float fY)
{
    plInputEvent* ptEvent = pl__get_event();
    ptEvent->tType   = PL_INPUT_EVENT_TYPE_MOUSE_WHEEL;
    ptEvent->tSource = PL_INPUT_EVENT_SOURCE_MOUSE;
    ptEvent->fWheelX = fX;
    ptEvent->fWheelY = fY;
}

void
pl_clear_input_characters(void)
{
    pl_sb_reset(gptIOContext->_sbInputQueueCharacters);
}

bool
pl_is_key_down(plKey tKey)
{
    const plKeyData* ptData = pl_get_key_data(tKey);
    return ptData->bDown;
}

bool
pl_is_key_pressed(plKey tKey, bool bRepeat)
{
    const plKeyData* ptData = pl_get_key_data(tKey);
    if (!ptData->bDown) // In theory this should already be encoded as (DownDuration < 0.0f), but testing this facilitates eating mechanism (until we finish work on input ownership)
        return false;
    const float fT = ptData->fDownDuration;
    if (fT < 0.0f)
        return false;

    bool bPressed = (fT == 0.0f);
    if (!bPressed && bRepeat)
    {
        const float fRepeatDelay = gptIOContext->fKeyRepeatDelay;
        const float fRepeatRate = gptIOContext->fKeyRepeatRate;
        bPressed = (fT > fRepeatDelay) && pl_get_key_pressed_amount(tKey, fRepeatDelay, fRepeatRate) > 0;
    }

    if (!bPressed)
        return false;
    return true;
}

bool
pl_is_key_released(plKey tKey)
{
    const plKeyData* ptData = pl_get_key_data(tKey);
    if (ptData->fDownDurationPrev < 0.0f || ptData->bDown)
        return false;
    return true;
}

int
pl_get_key_pressed_amount(plKey tKey, float fRepeatDelay, float fRate)
{
    plIOContext* ptIO = gptIOContext;
    const plKeyData* ptData = pl_get_key_data(tKey);
    if (!ptData->bDown) // In theory this should already be encoded as (DownDuration < 0.0f), but testing this facilitates eating mechanism (until we finish work on input ownership)
        return 0;
    const float fT = ptData->fDownDuration;
    return pl__calc_typematic_repeat_amount(fT - ptIO->fDeltaTime, fT, fRepeatDelay, fRate);
}

bool
pl_is_mouse_down(plMouseButton tButton)
{
    return gptIOContext->_abMouseDown[tButton];
}

bool
pl_is_mouse_clicked(plMouseButton tButton, bool bRepeat)
{
    plIOContext* ptIO = gptIOContext;
    if(!ptIO->_abMouseDown[tButton])
        return false;
    const float fT = ptIO->_afMouseDownDuration[tButton];
    if(fT == 0.0f)
        return true;
    if(bRepeat && fT > ptIO->fKeyRepeatDelay)
        return pl__calc_typematic_repeat_amount(fT - ptIO->fDeltaTime, fT, ptIO->fKeyRepeatDelay, ptIO->fKeyRepeatRate) > 0;
    return false;
}

bool
pl_is_mouse_released(plMouseButton tButton)
{
    return gptIOContext->_abMouseReleased[tButton];
}

bool
pl_is_mouse_double_clicked(plMouseButton tButton)
{
    return gptIOContext->_auMouseClickedCount[tButton] == 2;
}

bool
pl_is_mouse_dragging(plMouseButton tButton, float fThreshold)
{
    plIOContext* ptIO = gptIOContext;
    if(!ptIO->_abMouseDown[tButton])
        return false;
    if(fThreshold < 0.0f)
        fThreshold = ptIO->fMouseDragThreshold;
    return ptIO->_afMouseDragMaxDistSqr[tButton] >= fThreshold * fThreshold;
}

bool
pl_is_mouse_hovering_rect(plVec2 minVec, plVec2 maxVec)
{
    const plVec2 tMousePos = gptIOContext->_tMousePos;
    return ( tMousePos.x >= minVec.x && tMousePos.y >= minVec.y && tMousePos.x <= maxVec.x && tMousePos.y <= maxVec.y);
}

void
pl_reset_mouse_drag_delta(plMouseButton tButton)
{
    gptIOContext->_atMouseClickedPos[tButton] = gptIOContext->_tMousePos;
}

plVec2
pl_get_mouse_drag_delta(plMouseButton tButton, float fThreshold)
{
    plIOContext* ptIO = gptIOContext;
    if(fThreshold < 0.0f)
        fThreshold = ptIO->fMouseDragThreshold;
    if(ptIO->_abMouseDown[tButton] || ptIO->_abMouseReleased[tButton])
    {
        if(ptIO->_afMouseDragMaxDistSqr[tButton] >= fThreshold * fThreshold)
        {
            if(pl_is_mouse_pos_valid(ptIO->_tMousePos) && pl_is_mouse_pos_valid(ptIO->_atMouseClickedPos[tButton]))
                return PL_IO_VEC2_SUBTRACT(ptIO->_tLastValidMousePos, ptIO->_atMouseClickedPos[tButton]);
        }
    }
    
    return PL_IO_VEC2(0.0f, 0.0f);
}

plVec2
pl_get_mouse_pos(void)
{
    return gptIOContext->_tMousePos;
}

float
pl_get_mouse_wheel(void)
{
    return gptIOContext->_fMouseWheel;
}

bool
pl_is_mouse_pos_valid(plVec2 tPos)
{
    return tPos.x >= -FLT_MAX && tPos.y >= -FLT_MAX;
}

void
pl_set_mouse_cursor(plCursor tCursor)
{
    gptIOContext->tNextCursor = tCursor;
    gptIOContext->bCursorChanged = true;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__update_events(void)
{
    plIOContext* ptIO = gptIOContext;

    for(uint32_t i = 0; i < ptIO->_uInputEventSize; i++)
    {
        plInputEvent* ptEvent = &ptIO->_sbtInputEvents[i];

        switch(ptEvent->tType)
        {
            case PL_INPUT_EVENT_TYPE_MOUSE_POS:
            {

                if(ptEvent->fPosX != -FLT_MAX && ptEvent->fPosY != -FLT_MAX)
                {
                    ptIO->_tMousePos.x = ptEvent->fPosX;
                    ptIO->_tMousePos.y = ptEvent->fPosY;
                }
                break;
            }

            case PL_INPUT_EVENT_TYPE_MOUSE_WHEEL:
            {
                ptIO->_fMouseWheelH += ptEvent->fWheelX;
                ptIO->_fMouseWheel += ptEvent->fWheelY;
                break;
            }

            case PL_INPUT_EVENT_TYPE_MOUSE_BUTTON:
            {
                PL_ASSERT(ptEvent->iButton >= 0 && ptEvent->iButton < PL_MOUSE_BUTTON_COUNT);
                ptIO->_abMouseDown[ptEvent->iButton] = ptEvent->bMouseDown;
                break;
            }

            case PL_INPUT_EVENT_TYPE_KEY:
            {
                plKey tKey = ptEvent->tKey;
                PL_ASSERT(tKey != PL_KEY_NONE);
                plKeyData* ptKeyData = pl_get_key_data(tKey);
                ptKeyData->bDown = ptEvent->bKeyDown;
                break;
            }

            case PL_INPUT_EVENT_TYPE_TEXT:
            {
                plWChar uChar = (plWChar)ptEvent->uChar;
                pl_sb_push(gptIOContext->_sbInputQueueCharacters, uChar);
                break;
            }

            default:
            {
                PL_ASSERT(false && "unknown input event type");
                break;
            }
        }
    }

    ptIO->_uInputEventSize = 0;
}

static void
pl__update_keyboard_inputs(void)
{
   plIOContext* ptIO = gptIOContext;

    ptIO->tKeyMods = 0;
    if (pl_is_key_down(PL_KEY_MOD_CTRL))     { ptIO->tKeyMods |= PL_KEY_MOD_CTRL; }
    if (pl_is_key_down(PL_KEY_MOD_SHIFT))    { ptIO->tKeyMods |= PL_KEY_MOD_SHIFT; }
    if (pl_is_key_down(PL_KEY_MOD_ALT))      { ptIO->tKeyMods |= PL_KEY_MOD_ALT; }
    if (pl_is_key_down(PL_KEY_MOD_SUPER))    { ptIO->tKeyMods |= PL_KEY_MOD_SUPER; }

    ptIO->bKeyCtrl  = (ptIO->tKeyMods & PL_KEY_MOD_CTRL) != 0;
    ptIO->bKeyShift = (ptIO->tKeyMods & PL_KEY_MOD_SHIFT) != 0;
    ptIO->bKeyAlt   = (ptIO->tKeyMods & PL_KEY_MOD_ALT) != 0;
    ptIO->bKeySuper = (ptIO->tKeyMods & PL_KEY_MOD_SUPER) != 0;

    // Update keys
    for (uint32_t i = 0; i < PL_KEY_COUNT; i++)
    {
        plKeyData* ptKeyData = &ptIO->_tKeyData[i];
        ptKeyData->fDownDurationPrev = ptKeyData->fDownDuration;
        ptKeyData->fDownDuration = ptKeyData->bDown ? (ptKeyData->fDownDuration < 0.0f ? 0.0f : ptKeyData->fDownDuration + ptIO->fDeltaTime) : -1.0f;
    }
}

static void
pl__update_mouse_inputs(void)
{
    plIOContext* ptIO = gptIOContext;

    if(pl_is_mouse_pos_valid(ptIO->_tMousePos))
    {
        ptIO->_tMousePos.x = floorf(ptIO->_tMousePos.x);
        ptIO->_tMousePos.y = floorf(ptIO->_tMousePos.y);
        ptIO->_tLastValidMousePos = ptIO->_tMousePos;
    }

    if(pl_is_mouse_pos_valid(ptIO->_tMousePos) && pl_is_mouse_pos_valid(ptIO->_tMousePosPrev))
        ptIO->_tMouseDelta = PL_IO_VEC2_SUBTRACT(ptIO->_tMousePos, ptIO->_tMousePosPrev);
    else
    {
        ptIO->_tMouseDelta.x = 0.0f;
        ptIO->_tMouseDelta.y = 0.0f;
    }

    ptIO->_tMousePosPrev = ptIO->_tMousePos;

    for(uint32_t i = 0; i < PL_MOUSE_BUTTON_COUNT; i++)
    {
        ptIO->_abMouseClicked[i] = ptIO->_abMouseDown[i] && ptIO->_afMouseDownDuration[i] < 0.0f;
        ptIO->_auMouseClickedCount[i] = 0;
        ptIO->_abMouseReleased[i] = !ptIO->_abMouseDown[i] && ptIO->_afMouseDownDuration[i] >= 0.0f;
        ptIO->_afMouseDownDurationPrev[i] = ptIO->_afMouseDownDuration[i];
        ptIO->_afMouseDownDuration[i] = ptIO->_abMouseDown[i] ? (ptIO->_afMouseDownDuration[i] < 0.0f ? 0.0f : ptIO->_afMouseDownDuration[i] + ptIO->fDeltaTime) : -1.0f;

        if(ptIO->_abMouseClicked[i])
        {

            bool bIsRepeatedClick = false;
            if((float)(ptIO->dTime - ptIO->_adMouseClickedTime[i]) < ptIO->fMouseDoubleClickTime)
            {
                plVec2 tDeltaFromClickPos = PL_IO_VEC2(0.0f, 0.0f);
                if(pl_is_mouse_pos_valid(ptIO->_tMousePos))
                    tDeltaFromClickPos = PL_IO_VEC2_SUBTRACT(ptIO->_tMousePos, ptIO->_atMouseClickedPos[i]);

                if(PL_IO_VEC2_LENGTH_SQR(tDeltaFromClickPos) < ptIO->fMouseDoubleClickMaxDist * ptIO->fMouseDoubleClickMaxDist)
                    bIsRepeatedClick = true;
            }

            if(bIsRepeatedClick)
                ptIO->_auMouseClickedLastCount[i]++;
            else
                ptIO->_auMouseClickedLastCount[i] = 1;

            ptIO->_adMouseClickedTime[i] = ptIO->dTime;
            ptIO->_atMouseClickedPos[i] = ptIO->_tMousePos;
            ptIO->_afMouseDragMaxDistSqr[i] = 0.0f;
            ptIO->_auMouseClickedCount[i] = ptIO->_auMouseClickedLastCount[i];
        }
        else if(ptIO->_abMouseDown[i])
        {
            const plVec2 tClickPos = PL_IO_VEC2_SUBTRACT(ptIO->_tLastValidMousePos, ptIO->_atMouseClickedPos[i]);
            float fDeltaSqrClickPos = PL_IO_VEC2_LENGTH_SQR(tClickPos);
            ptIO->_afMouseDragMaxDistSqr[i] = PL_IO_MAX(fDeltaSqrClickPos, ptIO->_afMouseDragMaxDistSqr[i]);
        }


    }
}

static int
pl__calc_typematic_repeat_amount(float fT0, float fT1, float fRepeatDelay, float fRepeatRate)
{
    if(fT1 == 0.0f)
        return 1;
    if(fT0 >= fT1)
        return 0;
    if(fRepeatRate <= 0.0f)
        return (fT0 < fRepeatDelay) && (fT1 >= fRepeatDelay);
    
    const int iCountT0 = (fT0 < fRepeatDelay) ? -1 : (int)((fT0 - fRepeatDelay) / fRepeatRate);
    const int iCountT1 = (fT1 < fRepeatDelay) ? -1 : (int)((fT1 - fRepeatDelay) / fRepeatRate);
    const int iCount = iCountT1 - iCountT0;
    return iCount;
}

static plInputEvent*
pl__get_last_event(plInputEventType tType, int iButtonOrKey)
{
    plIOContext* ptIO = gptIOContext;
    for(uint32_t i = 0; i < ptIO->_uInputEventSize; i++)
    {
        plInputEvent* ptEvent = &ptIO->_sbtInputEvents[ptIO->_uInputEventSize - i - 1];
        if(ptEvent->tType != tType)
            continue;
        if(tType == PL_INPUT_EVENT_TYPE_KEY && (int)ptEvent->tKey != iButtonOrKey)
            continue;
        if(tType == PL_INPUT_EVENT_TYPE_MOUSE_BUTTON && ptEvent->iButton != iButtonOrKey)
            continue;
        return ptEvent;
    }
    return NULL;
}

static plInputEvent*
pl__get_event(void)
{
    plInputEvent* ptEvent = NULL;

    // check if new overflow
    if(!gptIOContext->_bOverflowInUse && gptIOContext->_uInputEventSize == gptIOContext->_uInputEventCapacity)
    {
        gptIOContext->_sbtInputEvents = (plInputEvent*)PL_ALLOC(sizeof(plInputEvent) * 256);
        memset(gptIOContext->_sbtInputEvents, 0, sizeof(plInputEvent) * 256);
        gptIOContext->_uInputEventOverflowCapacity = 256;

        // copy stack samples
        memcpy(gptIOContext->_sbtInputEvents, gptIOContext->_atInputEvents, sizeof(plInputEvent) * gptIOContext->_uInputEventCapacity);
        gptIOContext->_bOverflowInUse = true;
    }
    // check if overflow reallocation is needed
    else if(gptIOContext->_bOverflowInUse && gptIOContext->_uInputEventSize == gptIOContext->_uInputEventOverflowCapacity)
    {
        plInputEvent* sbtOldInputEvents = gptIOContext->_sbtInputEvents;
        gptIOContext->_sbtInputEvents = (plInputEvent*)PL_ALLOC(sizeof(plInputEvent) * gptIOContext->_uInputEventOverflowCapacity * 2);
        memset(gptIOContext->_sbtInputEvents, 0, sizeof(plInputEvent) * gptIOContext->_uInputEventOverflowCapacity * 2);
        
        // copy old values
        memcpy(gptIOContext->_sbtInputEvents, sbtOldInputEvents, sizeof(plInputEvent) * gptIOContext->_uInputEventOverflowCapacity);
        gptIOContext->_uInputEventOverflowCapacity *= 2;

        PL_FREE(sbtOldInputEvents);
    }

    ptEvent = &gptIOContext->_sbtInputEvents[gptIOContext->_uInputEventSize];
    gptIOContext->_uInputEventSize++;

    return ptEvent;
}