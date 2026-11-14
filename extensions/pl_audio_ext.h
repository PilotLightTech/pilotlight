/*
   pl_audio_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*
    Implementation:
        The provided implementation retrieves the following APIs. Core services
        are included; this lists direct dependencies, not their dependencies.

        * plDataRegistryI (v1.x)
        * plMemoryI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_AUDIO_EXT_H
#define PL_AUDIO_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plAudioI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plAudioI
{
    int _iUnused;

} plAudioI;

#ifdef __cplusplus
}
#endif

#endif // PL_AUDIO_EXT_H