/*
   example_basic_audio_0.c
     - demonstrates basic audio playback using pl_audio_ext
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] apis
// [SECTION] structs
// [SECTION] helpers
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pl.h"
#include "pl_audio_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*    gptIO = NULL;
const plAudioI* gptAudio = NULL;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    float*           pfSamples;
    uint32_t         uSampleRate;
    uint32_t         uFrameCount;
    plAudioClipHandle tClip;
    plAudioVoiceHandle tVoice;
    bool             bAudioReady;
    bool             bPlayed;
    bool             bPlayFailed;
    float            fElapsedTime;
    uint32_t         uFrameCounter;
    double           dStartTimeSec;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

static void
pl__generate_sine_wave(plAppData* ptAppData)
{
    const float fTau = 6.28318530718f;
    const float fAmplitude = 0.18f;
    const float fNoteDuration = 0.20f;
    const float afNotes[] = {
        523.25f, // C5
        659.25f, // E5
        783.99f, // G5
        1046.50f, // C6
        783.99f, // G5
        659.25f, // E5
        523.25f, // C5
        392.00f  // G4
    };
    const uint32_t uNoteCount = (uint32_t)(sizeof(afNotes) / sizeof(afNotes[0]));

    ptAppData->uSampleRate = 48000;
    ptAppData->uFrameCount = ptAppData->uSampleRate * 2u;
    ptAppData->pfSamples = (float*)malloc(sizeof(float) * ptAppData->uFrameCount);
    if(ptAppData->pfSamples == NULL)
        return;

    uint32_t uFramesPerNote = (uint32_t)(fNoteDuration * (float)ptAppData->uSampleRate);
    if(uFramesPerNote == 0)
        uFramesPerNote = 1;

    for(uint32_t i = 0; i < ptAppData->uFrameCount; i++)
    {
        uint32_t uNoteIndex = (i / uFramesPerNote) % uNoteCount;
        float fFrequency = afNotes[uNoteIndex];
        float fTime = (float)i / (float)ptAppData->uSampleRate;

        // Apply a light per-note envelope to reduce clicks.
        float fNoteLocalT = (float)(i % uFramesPerNote) / (float)uFramesPerNote;
        float fEnvelope = 1.0f;
        if(fNoteLocalT < 0.05f)
            fEnvelope = fNoteLocalT / 0.05f;
        else if(fNoteLocalT > 0.90f)
            fEnvelope = (1.0f - fNoteLocalT) / 0.10f;
        if(fEnvelope < 0.0f)
            fEnvelope = 0.0f;

        ptAppData->pfSamples[i] = sinf(fTau * fFrequency * fTime) * fAmplitude * fEnvelope;
    }
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, void* pAppData)
{
    gptIO = pl_get_api_latest(ptApiRegistry, plIOI);
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    if(ptExtensionRegistry)
    {
        ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
        ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    }
    gptAudio = pl_get_api_latest(ptApiRegistry, plAudioI);

    plAppData* ptAppData = (plAppData*)pAppData;
    if(ptAppData == NULL)
    {
        ptAppData = (plAppData*)malloc(sizeof(plAppData));
        if(ptAppData == NULL)
            return NULL;
        memset(ptAppData, 0, sizeof(plAppData));
    }

    if(gptAudio == NULL)
    {
        printf("audio api not found\n");
        return ptAppData;
    }

    ptAppData->dStartTimeSec = (double)clock() / (double)CLOCKS_PER_SEC;

    plAudioInit tInit = {
        .uMaxClips = 16,
        .uMaxVoices = 16,
        .fMasterVolume = 1.0f
    };

    ptAppData->bAudioReady = gptAudio->initialize(&tInit);
    if(!ptAppData->bAudioReady)
    {
        printf("audio initialize failed\n");
        return ptAppData;
    }

    if(ptAppData->pfSamples == NULL)
        pl__generate_sine_wave(ptAppData);

    if(ptAppData->pfSamples)
    {
        plAudioBufferDesc tDesc = {
            .pData = ptAppData->pfSamples,
            .szDataSize = sizeof(float) * ptAppData->uFrameCount,
            .uSampleRate = ptAppData->uSampleRate,
            .uChannelCount = 1,
            .tSampleFormat = PL_AUDIO_SAMPLE_FORMAT_F32
        };
        ptAppData->tClip = gptAudio->create_clip_from_memory(&tDesc);
        if(ptAppData->tClip.uGeneration == 0)
            printf("audio clip creation failed\n");
    }

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(void* pAppData)
{
    plAppData* ptAppData = (plAppData*)pAppData;
    if(ptAppData == NULL)
        return;

    if(gptAudio)
    {
        if(ptAppData->tVoice.uGeneration)
            gptAudio->stop(ptAppData->tVoice);
        if(ptAppData->tClip.uGeneration)
            gptAudio->destroy_clip(ptAppData->tClip);
        gptAudio->cleanup();
    }

    free(ptAppData->pfSamples);
    free(ptAppData);

    printf("audio example shutting down\n");
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, void* pAppData)
{
    (void)ptWindow;
    (void)pAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(void* pAppData)
{
    plAppData* ptAppData = (plAppData*)pAppData;
    if(ptAppData == NULL)
        return;

    gptIO->new_frame();
    ptAppData->uFrameCounter++;

    if(gptAudio)
        gptAudio->update();

    if(ptAppData->bAudioReady && !ptAppData->bPlayed && ptAppData->tClip.uGeneration)
    {
        ptAppData->tVoice = gptAudio->play(ptAppData->tClip, 1.0f, PL_AUDIO_PLAY_FLAG_NONE);
        ptAppData->bPlayed = true;

        if(ptAppData->tVoice.uGeneration == 0)
        {
            ptAppData->bPlayFailed = true;
            printf("audio play failed\n");
        }
        else
            printf("playing chiptune test tone (2s)\n");
    }

    plIO* ptIO = gptIO->get_io();
    ptAppData->fElapsedTime += ptIO->fDeltaTime;
    const double dNowSec = (double)clock() / (double)CLOCKS_PER_SEC;
    const double dWallElapsedSec = dNowSec - ptAppData->dStartTimeSec;

    if(ptAppData->bPlayFailed && (ptAppData->fElapsedTime > 1.0f || dWallElapsedSec > 1.0))
    {
        ptIO->bRunning = false;
    }
    else if(ptAppData->bPlayed && ptAppData->tVoice.uGeneration && !gptAudio->is_playing(ptAppData->tVoice) &&
            (ptAppData->fElapsedTime > 0.25f || dWallElapsedSec > 0.25))
    {
        ptIO->bRunning = false;
    }
    else if(ptAppData->fElapsedTime > 5.0f || dWallElapsedSec > 3.0)
    {
        ptIO->bRunning = false;
    }
}
