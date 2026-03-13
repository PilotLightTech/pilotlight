/*
   pl_audio_ext.c
*/

/*
Index of this file:

// [SECTION] includes
// [SECTION] internal structs
// [SECTION] internal api
// [SECTION] internal helpers
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h>
#include "pl.h"
#include "pl_audio_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

#include "pl_ds.h"

#ifdef _MSC_VER
    #pragma warning(push, 0)
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "../dependencies/miniaudio/miniaudio.h"
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plAudioClipSlot
{
    bool      bValid;
    bool      bUseMaAllocator;
    uint32_t  uGeneration;
    void*     pData;
    size_t    szDataSize;
    ma_uint64 uFrameCount;
    ma_format tFormat;
    ma_uint32 uChannels;
    ma_uint32 uSampleRate;
} plAudioClipSlot;

typedef struct _plAudioVoiceSlot
{
    bool               bValid;
    uint32_t           uGeneration;
    uint32_t           uClipIndex;
    ma_audio_buffer_ref tBufferRef;
    ma_sound            tSound;
} plAudioVoiceSlot;

typedef struct _plAudioContext
{
    bool           bInitialized;
    float          fMasterVolume;
    ma_engine      tEngine;
    bool           bEngineValid;

    uint32_t       uMaxClips;
    uint32_t       uMaxVoices;
    plAudioClipSlot*  atClips;
    plAudioVoiceSlot* atVoices;
} plAudioContext;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static bool               pl__audio_initialize      (const plAudioInit*);
static void               pl__audio_cleanup         (void);
static bool               pl__audio_is_initialized  (void);
static void               pl__audio_update          (void);
static plAudioClipHandle  pl__audio_create_clip_from_file  (const char* pcPath);
static plAudioClipHandle  pl__audio_create_clip_from_memory(const plAudioBufferDesc* ptDesc);
static void               pl__audio_destroy_clip    (plAudioClipHandle tHandle);
static plAudioVoiceHandle pl__audio_play            (plAudioClipHandle tClip, float fVolume, plAudioPlayFlags tFlags);
static void               pl__audio_stop            (plAudioVoiceHandle tHandle);
static void               pl__audio_stop_all        (void);
static bool               pl__audio_is_playing      (plAudioVoiceHandle tHandle);
static void               pl__audio_set_master_volume(float fVolume);
static float              pl__audio_get_master_volume(void);
static void               pl__audio_set_voice_volume(plAudioVoiceHandle tHandle, float fVolume);
static float              pl__audio_get_voice_volume(plAudioVoiceHandle tHandle);

//-----------------------------------------------------------------------------
// [SECTION] internal helpers
//-----------------------------------------------------------------------------

static plAudioContext* gptAudioCtx = NULL;

static plAudioClipHandle
pl__audio_invalid_clip_handle(void)
{
    return (plAudioClipHandle){0};
}

static plAudioVoiceHandle
pl__audio_invalid_voice_handle(void)
{
    return (plAudioVoiceHandle){0};
}

static plAudioClipHandle
pl__audio_make_clip_handle(uint32_t uIndex, uint32_t uGeneration)
{
    plAudioClipHandle tHandle = {0};
    tHandle.uIndex = uIndex;
    tHandle.uGeneration = uGeneration;
    return tHandle;
}

static plAudioVoiceHandle
pl__audio_make_voice_handle(uint32_t uIndex, uint32_t uGeneration)
{
    plAudioVoiceHandle tHandle = {0};
    tHandle.uIndex = uIndex;
    tHandle.uGeneration = uGeneration;
    return tHandle;
}

static ma_format
pl__audio_to_ma_format(plAudioSampleFormat tFormat)
{
    switch(tFormat)
    {
        case PL_AUDIO_SAMPLE_FORMAT_S16: return ma_format_s16;
        case PL_AUDIO_SAMPLE_FORMAT_F32: return ma_format_f32;
        default: return ma_format_unknown;
    }
}

static bool
pl__audio_validate_clip_handle(plAudioClipHandle tHandle)
{
    if(gptAudioCtx == NULL || gptAudioCtx->atClips == NULL)
        return false;
    if(tHandle.uGeneration == 0)
        return false;
    if(tHandle.uIndex >= gptAudioCtx->uMaxClips)
        return false;

    const plAudioClipSlot* ptSlot = &gptAudioCtx->atClips[tHandle.uIndex];
    return ptSlot->bValid && ptSlot->uGeneration == tHandle.uGeneration;
}

static bool
pl__audio_validate_voice_handle(plAudioVoiceHandle tHandle)
{
    if(gptAudioCtx == NULL || gptAudioCtx->atVoices == NULL)
        return false;
    if(tHandle.uGeneration == 0)
        return false;
    if(tHandle.uIndex >= gptAudioCtx->uMaxVoices)
        return false;

    const plAudioVoiceSlot* ptSlot = &gptAudioCtx->atVoices[tHandle.uIndex];
    return ptSlot->bValid && ptSlot->uGeneration == tHandle.uGeneration;
}

static void
pl__audio_release_voice_slot(plAudioVoiceSlot* ptSlot)
{
    if(ptSlot == NULL || !ptSlot->bValid)
        return;

    ma_sound_uninit(&ptSlot->tSound);
    ma_audio_buffer_ref_uninit(&ptSlot->tBufferRef);

    ptSlot->bValid = false;
    ptSlot->uClipIndex = 0;
}

static void
pl__audio_collect_finished_voices(void)
{
    if(gptAudioCtx == NULL || !gptAudioCtx->bInitialized || gptAudioCtx->atVoices == NULL)
        return;

    for(uint32_t i = 0; i < gptAudioCtx->uMaxVoices; i++)
    {
        plAudioVoiceSlot* ptSlot = &gptAudioCtx->atVoices[i];
        if(!ptSlot->bValid)
            continue;

        if(!ma_sound_is_playing(&ptSlot->tSound) && ma_sound_at_end(&ptSlot->tSound))
            pl__audio_release_voice_slot(ptSlot);
    }
}

static uint32_t
pl__audio_find_free_clip_slot(void)
{
    if(gptAudioCtx == NULL || gptAudioCtx->atClips == NULL)
        return UINT32_MAX;

    for(uint32_t i = 0; i < gptAudioCtx->uMaxClips; i++)
    {
        if(!gptAudioCtx->atClips[i].bValid)
            return i;
    }
    return UINT32_MAX;
}

static uint32_t
pl__audio_find_free_voice_slot(void)
{
    if(gptAudioCtx == NULL || gptAudioCtx->atVoices == NULL)
        return UINT32_MAX;

    pl__audio_collect_finished_voices();

    for(uint32_t i = 0; i < gptAudioCtx->uMaxVoices; i++)
    {
        if(!gptAudioCtx->atVoices[i].bValid)
            return i;
    }
    return UINT32_MAX;
}

static void
pl__audio_free_clip_slot(plAudioClipSlot* ptSlot)
{
    if(ptSlot == NULL || !ptSlot->bValid)
        return;

    if(ptSlot->pData)
    {
        if(ptSlot->bUseMaAllocator)
            ma_free(ptSlot->pData, NULL);
        else
            PL_FREE(ptSlot->pData);
    }

    ptSlot->pData = NULL;
    ptSlot->szDataSize = 0;
    ptSlot->uFrameCount = 0;
    ptSlot->bUseMaAllocator = false;
    ptSlot->bValid = false;
}

static bool
pl__audio_ensure_storage(uint32_t uMaxClips, uint32_t uMaxVoices)
{
    if(gptAudioCtx == NULL)
        return false;

    if(uMaxClips == 0)
        uMaxClips = 256;
    if(uMaxVoices == 0)
        uMaxVoices = 64;

    if(gptAudioCtx->atClips == NULL)
    {
        gptAudioCtx->atClips = (plAudioClipSlot*)PL_ALLOC(sizeof(plAudioClipSlot) * uMaxClips);
        if(gptAudioCtx->atClips == NULL)
            return false;
        memset(gptAudioCtx->atClips, 0, sizeof(plAudioClipSlot) * uMaxClips);
        gptAudioCtx->uMaxClips = uMaxClips;
    }

    if(gptAudioCtx->atVoices == NULL)
    {
        gptAudioCtx->atVoices = (plAudioVoiceSlot*)PL_ALLOC(sizeof(plAudioVoiceSlot) * uMaxVoices);
        if(gptAudioCtx->atVoices == NULL)
            return false;
        memset(gptAudioCtx->atVoices, 0, sizeof(plAudioVoiceSlot) * uMaxVoices);
        gptAudioCtx->uMaxVoices = uMaxVoices;
    }

    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static bool
pl__audio_initialize(const plAudioInit* ptInit)
{
    if(gptAudioCtx == NULL)
        return false;
    if(gptAudioCtx->bInitialized)
        return true;

    uint32_t uMaxClips = ptInit ? ptInit->uMaxClips : 0u;
    uint32_t uMaxVoices = ptInit ? ptInit->uMaxVoices : 0u;
    if(!pl__audio_ensure_storage(uMaxClips, uMaxVoices))
        return false;

    ma_engine_config tEngineConfig = ma_engine_config_init();
    if(ma_engine_init(&tEngineConfig, &gptAudioCtx->tEngine) != MA_SUCCESS)
        return false;

    gptAudioCtx->bEngineValid = true;
    gptAudioCtx->bInitialized = true;
    gptAudioCtx->fMasterVolume = (ptInit) ? ptInit->fMasterVolume : 1.0f;
    if(gptAudioCtx->fMasterVolume < 0.0f)
        gptAudioCtx->fMasterVolume = 0.0f;
    ma_engine_set_volume(&gptAudioCtx->tEngine, gptAudioCtx->fMasterVolume);

    return true;
}

static void
pl__audio_cleanup(void)
{
    if(gptAudioCtx == NULL)
        return;

    if(gptAudioCtx->atVoices)
    {
        for(uint32_t i = 0; i < gptAudioCtx->uMaxVoices; i++)
            pl__audio_release_voice_slot(&gptAudioCtx->atVoices[i]);
    }

    if(gptAudioCtx->bEngineValid)
    {
        ma_engine_uninit(&gptAudioCtx->tEngine);
        gptAudioCtx->bEngineValid = false;
    }

    if(gptAudioCtx->atClips)
    {
        for(uint32_t i = 0; i < gptAudioCtx->uMaxClips; i++)
            pl__audio_free_clip_slot(&gptAudioCtx->atClips[i]);
    }

    if(gptAudioCtx->atVoices)
    {
        PL_FREE(gptAudioCtx->atVoices);
        gptAudioCtx->atVoices = NULL;
    }

    if(gptAudioCtx->atClips)
    {
        PL_FREE(gptAudioCtx->atClips);
        gptAudioCtx->atClips = NULL;
    }

    gptAudioCtx->uMaxClips = 0;
    gptAudioCtx->uMaxVoices = 0;
    gptAudioCtx->bInitialized = false;
}

static bool
pl__audio_is_initialized(void)
{
    return gptAudioCtx && gptAudioCtx->bInitialized && gptAudioCtx->bEngineValid;
}

static void
pl__audio_update(void)
{
    pl__audio_collect_finished_voices();
}

static plAudioClipHandle
pl__audio_create_clip_from_file(const char* pcPath)
{
    if(!pl__audio_is_initialized() || pcPath == NULL)
        return pl__audio_invalid_clip_handle();

    uint32_t uSlotIndex = pl__audio_find_free_clip_slot();
    if(uSlotIndex == UINT32_MAX)
        return pl__audio_invalid_clip_handle();

    ma_decoder_config tDecodeConfig = ma_decoder_config_init(ma_format_f32, 2, 0);
    ma_uint64 uFrameCount = 0;
    void* pFrames = NULL;
    if(ma_decode_file(pcPath, &tDecodeConfig, &uFrameCount, &pFrames) != MA_SUCCESS)
        return pl__audio_invalid_clip_handle();

    if(pFrames == NULL || uFrameCount == 0)
    {
        if(pFrames)
            ma_free(pFrames, NULL);
        return pl__audio_invalid_clip_handle();
    }

    plAudioClipSlot* ptSlot = &gptAudioCtx->atClips[uSlotIndex];
    ptSlot->uGeneration++;
    if(ptSlot->uGeneration == 0)
        ptSlot->uGeneration = 1;

    ptSlot->bValid = true;
    ptSlot->bUseMaAllocator = true;
    ptSlot->pData = pFrames;
    ptSlot->uFrameCount = uFrameCount;
    ptSlot->tFormat = tDecodeConfig.format;
    ptSlot->uChannels = tDecodeConfig.channels;
    ptSlot->uSampleRate = tDecodeConfig.sampleRate;
    ptSlot->szDataSize = (size_t)(uFrameCount * ma_get_bytes_per_frame(tDecodeConfig.format, tDecodeConfig.channels));

    return pl__audio_make_clip_handle(uSlotIndex, ptSlot->uGeneration);
}

static plAudioClipHandle
pl__audio_create_clip_from_memory(const plAudioBufferDesc* ptDesc)
{
    if(!pl__audio_is_initialized() || ptDesc == NULL || ptDesc->pData == NULL)
        return pl__audio_invalid_clip_handle();
    if(ptDesc->uChannelCount == 0 || ptDesc->uSampleRate == 0 || ptDesc->szDataSize == 0)
        return pl__audio_invalid_clip_handle();

    ma_format tFormat = pl__audio_to_ma_format(ptDesc->tSampleFormat);
    if(tFormat == ma_format_unknown)
        return pl__audio_invalid_clip_handle();

    const size_t szBytesPerFrame = (size_t)ma_get_bytes_per_frame(tFormat, ptDesc->uChannelCount);
    if(szBytesPerFrame == 0 || (ptDesc->szDataSize % szBytesPerFrame) != 0)
        return pl__audio_invalid_clip_handle();

    uint32_t uSlotIndex = pl__audio_find_free_clip_slot();
    if(uSlotIndex == UINT32_MAX)
        return pl__audio_invalid_clip_handle();

    void* pDataCopy = PL_ALLOC(ptDesc->szDataSize);
    if(pDataCopy == NULL)
        return pl__audio_invalid_clip_handle();

    memcpy(pDataCopy, ptDesc->pData, ptDesc->szDataSize);

    plAudioClipSlot* ptSlot = &gptAudioCtx->atClips[uSlotIndex];
    ptSlot->uGeneration++;
    if(ptSlot->uGeneration == 0)
        ptSlot->uGeneration = 1;

    ptSlot->bValid = true;
    ptSlot->bUseMaAllocator = false;
    ptSlot->pData = pDataCopy;
    ptSlot->szDataSize = ptDesc->szDataSize;
    ptSlot->uFrameCount = (ma_uint64)(ptDesc->szDataSize / szBytesPerFrame);
    ptSlot->tFormat = tFormat;
    ptSlot->uChannels = ptDesc->uChannelCount;
    ptSlot->uSampleRate = ptDesc->uSampleRate;

    return pl__audio_make_clip_handle(uSlotIndex, ptSlot->uGeneration);
}

static void
pl__audio_destroy_clip(plAudioClipHandle tHandle)
{
    if(!pl__audio_validate_clip_handle(tHandle))
        return;

    for(uint32_t i = 0; i < gptAudioCtx->uMaxVoices; i++)
    {
        plAudioVoiceSlot* ptVoice = &gptAudioCtx->atVoices[i];
        if(ptVoice->bValid && ptVoice->uClipIndex == tHandle.uIndex)
            pl__audio_release_voice_slot(ptVoice);
    }

    pl__audio_free_clip_slot(&gptAudioCtx->atClips[tHandle.uIndex]);
}

static plAudioVoiceHandle
pl__audio_play(plAudioClipHandle tClip, float fVolume, plAudioPlayFlags tFlags)
{
    if(!pl__audio_is_initialized() || !pl__audio_validate_clip_handle(tClip))
        return pl__audio_invalid_voice_handle();

    uint32_t uVoiceIndex = pl__audio_find_free_voice_slot();
    if(uVoiceIndex == UINT32_MAX)
        return pl__audio_invalid_voice_handle();

    plAudioClipSlot* ptClip = &gptAudioCtx->atClips[tClip.uIndex];
    plAudioVoiceSlot* ptVoice = &gptAudioCtx->atVoices[uVoiceIndex];

    if(ma_audio_buffer_ref_init(ptClip->tFormat, ptClip->uChannels, ptClip->pData, ptClip->uFrameCount, &ptVoice->tBufferRef) != MA_SUCCESS)
        return pl__audio_invalid_voice_handle();

    if(ma_sound_init_from_data_source(&gptAudioCtx->tEngine, (ma_data_source*)&ptVoice->tBufferRef, MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, &ptVoice->tSound) != MA_SUCCESS)
    {
        ma_audio_buffer_ref_uninit(&ptVoice->tBufferRef);
        return pl__audio_invalid_voice_handle();
    }

    ptVoice->uGeneration++;
    if(ptVoice->uGeneration == 0)
        ptVoice->uGeneration = 1;

    ptVoice->bValid = true;
    ptVoice->uClipIndex = tClip.uIndex;

    if(fVolume < 0.0f)
        fVolume = 0.0f;
    ma_sound_set_volume(&ptVoice->tSound, fVolume);
    ma_sound_set_looping(&ptVoice->tSound, (tFlags & PL_AUDIO_PLAY_FLAG_LOOP) ? MA_TRUE : MA_FALSE);

    if(ma_sound_start(&ptVoice->tSound) != MA_SUCCESS)
    {
        pl__audio_release_voice_slot(ptVoice);
        return pl__audio_invalid_voice_handle();
    }

    return pl__audio_make_voice_handle(uVoiceIndex, ptVoice->uGeneration);
}

static void
pl__audio_stop(plAudioVoiceHandle tHandle)
{
    if(!pl__audio_validate_voice_handle(tHandle))
        return;

    plAudioVoiceSlot* ptVoice = &gptAudioCtx->atVoices[tHandle.uIndex];
    ma_sound_stop(&ptVoice->tSound);
    ma_sound_seek_to_pcm_frame(&ptVoice->tSound, 0);
    pl__audio_release_voice_slot(ptVoice);
}

static void
pl__audio_stop_all(void)
{
    if(gptAudioCtx == NULL || gptAudioCtx->atVoices == NULL)
        return;

    for(uint32_t i = 0; i < gptAudioCtx->uMaxVoices; i++)
    {
        plAudioVoiceSlot* ptVoice = &gptAudioCtx->atVoices[i];
        if(!ptVoice->bValid)
            continue;
        ma_sound_stop(&ptVoice->tSound);
        pl__audio_release_voice_slot(ptVoice);
    }
}

static bool
pl__audio_is_playing(plAudioVoiceHandle tHandle)
{
    if(!pl__audio_validate_voice_handle(tHandle))
        return false;
    return ma_sound_is_playing(&gptAudioCtx->atVoices[tHandle.uIndex].tSound) != 0;
}

static void
pl__audio_set_master_volume(float fVolume)
{
    if(gptAudioCtx == NULL)
        return;

    if(fVolume < 0.0f)
        fVolume = 0.0f;

    gptAudioCtx->fMasterVolume = fVolume;
    if(gptAudioCtx->bEngineValid)
        ma_engine_set_volume(&gptAudioCtx->tEngine, fVolume);
}

static float
pl__audio_get_master_volume(void)
{
    if(gptAudioCtx == NULL)
        return 0.0f;
    return gptAudioCtx->fMasterVolume;
}

static void
pl__audio_set_voice_volume(plAudioVoiceHandle tHandle, float fVolume)
{
    if(!pl__audio_validate_voice_handle(tHandle))
        return;

    if(fVolume < 0.0f)
        fVolume = 0.0f;
    ma_sound_set_volume(&gptAudioCtx->atVoices[tHandle.uIndex].tSound, fVolume);
}

static float
pl__audio_get_voice_volume(plAudioVoiceHandle tHandle)
{
    if(!pl__audio_validate_voice_handle(tHandle))
        return 0.0f;
    return ma_sound_get_volume(&gptAudioCtx->atVoices[tHandle.uIndex].tSound);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_audio_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plAudioI tApi = {
        .initialize              = pl__audio_initialize,
        .cleanup                 = pl__audio_cleanup,
        .is_initialized          = pl__audio_is_initialized,
        .update                  = pl__audio_update,
        .create_clip_from_file   = pl__audio_create_clip_from_file,
        .create_clip_from_memory = pl__audio_create_clip_from_memory,
        .destroy_clip            = pl__audio_destroy_clip,
        .play                    = pl__audio_play,
        .stop                    = pl__audio_stop,
        .stop_all                = pl__audio_stop_all,
        .is_playing              = pl__audio_is_playing,
        .set_master_volume       = pl__audio_set_master_volume,
        .get_master_volume       = pl__audio_get_master_volume,
        .set_voice_volume        = pl__audio_set_voice_volume,
        .get_voice_volume        = pl__audio_get_voice_volume
    };
    pl_set_api(ptApiRegistry, plAudioI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptAudioCtx = ptDataRegistry->get_data("plAudioContext");
    }
    else
    {
        gptAudioCtx = (plAudioContext*)PL_ALLOC(sizeof(plAudioContext));
        if(gptAudioCtx)
            memset(gptAudioCtx, 0, sizeof(plAudioContext));
        if(gptAudioCtx)
            gptAudioCtx->fMasterVolume = 1.0f;
        ptDataRegistry->set_data("plAudioContext", gptAudioCtx);
    }
}

PL_EXPORT void
pl_unload_audio_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        // Backend teardown across hot reload is safer than keeping audio threads running.
        pl__audio_cleanup();
        return;
    }

    pl__audio_cleanup();

    const plAudioI* ptApi = pl_get_api_latest(ptApiRegistry, plAudioI);
    ptApiRegistry->remove_api(ptApi);

    if(ptDataRegistry)
        ptDataRegistry->set_data("plAudioContext", NULL);

    if(gptAudioCtx)
    {
        PL_FREE(gptAudioCtx);
        gptAudioCtx = NULL;
    }
}
