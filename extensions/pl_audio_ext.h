/*
   pl_audio_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] apis
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_AUDIO_EXT_H
#define PL_AUDIO_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plAudioI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// flags
typedef int plAudioPlayFlags; // -> enum _plAudioPlayFlags // Flag: audio playback flags (PL_AUDIO_PLAY_FLAG_XXXX)

// basic types
typedef struct _plAudioClipHandle  plAudioClipHandle;
typedef struct _plAudioVoiceHandle plAudioVoiceHandle;
typedef struct _plAudioBufferDesc  plAudioBufferDesc;
typedef struct _plAudioInit        plAudioInit;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plAudioI
{
    // setup/shutdown
    bool (*initialize)     (const plAudioInit*);
    void (*cleanup)        (void);
    bool (*is_initialized) (void);

    // optional per-frame update (may be a no-op depending on backend)
    void (*update)         (void);

    // clip lifetime (PCM buffer input)
    plAudioClipHandle (*create_clip_from_file)  (const char* pcPath);
    plAudioClipHandle (*create_clip_from_memory)(const plAudioBufferDesc*);
    void              (*destroy_clip)           (plAudioClipHandle);

    // playback
    plAudioVoiceHandle (*play)      (plAudioClipHandle, float fVolume, plAudioPlayFlags);
    void               (*stop)      (plAudioVoiceHandle);
    void               (*stop_all)  (void);
    bool               (*is_playing)(plAudioVoiceHandle);

    // volume
    void  (*set_master_volume)(float fVolume);
    float (*get_master_volume)(void);
    void  (*set_voice_volume) (plAudioVoiceHandle, float fVolume);
    float (*get_voice_volume) (plAudioVoiceHandle);
} plAudioI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plAudioPlayFlags
{
    PL_AUDIO_PLAY_FLAG_NONE = 0,
    PL_AUDIO_PLAY_FLAG_LOOP = 1 << 0
};

typedef enum _plAudioSampleFormat
{
    PL_AUDIO_SAMPLE_FORMAT_S16,
    PL_AUDIO_SAMPLE_FORMAT_F32
} plAudioSampleFormat;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAudioClipHandle
{
    uint32_t uIndex;
    uint32_t uGeneration;
} plAudioClipHandle;

typedef struct _plAudioVoiceHandle
{
    uint32_t uIndex;
    uint32_t uGeneration;
} plAudioVoiceHandle;

typedef struct _plAudioBufferDesc
{
    const void*         pData;
    size_t              szDataSize;
    uint32_t            uSampleRate;
    uint32_t            uChannelCount;
    plAudioSampleFormat tSampleFormat;
} plAudioBufferDesc;

typedef struct _plAudioInit
{
    uint32_t uMaxClips;  // default: implementation-defined
    uint32_t uMaxVoices; // default: implementation-defined
    float    fMasterVolume; // default: 1.0f
} plAudioInit;

#endif // PL_AUDIO_EXT_H
