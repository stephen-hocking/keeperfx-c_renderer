/******************************************************************************/
// Bullfrog Engine Emulation Library - for use to remake classic games like
// Syndicate Wars, Magic Carpet or Dungeon Keeper.
/******************************************************************************/
/** @file bflib_sound.c
 *     Sound and music related routines.
 * @par Purpose:
 *     Sound and music routines to use in games.
 * @par Comment:
 *     None.
 * @author   KeeperFX Team
 * @date     16 Nov 2008 - 30 Dec 2008
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include "bflib_sound.h"

#include <string.h>
#include <stdio.h>

#include "bflib_basics.h"
#include "bflib_memory.h"
#include "bflib_math.h"
#include "bflib_heapmgr.h"
#include "bflib_sndlib.h"
#include "bflib_fileio.h"
#include "globals.h"

#define INVALID_SOUND_EMITTER (&emitter[0])

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************/
DLLIMPORT void _DK_stop_sample_using_heap(unsigned long a1, short a2, unsigned char pan);
DLLIMPORT long _DK_start_emitter_playing(struct SoundEmitter *emit, long a2, long pan, long volume, long pitch, long a6, long a7, long a8, long a9);
DLLIMPORT void _DK_close_sound_heap(void);
DLLIMPORT void _DK_play_non_3d_sample(long sidx);
DLLIMPORT struct SampleInfo *_DK_play_sample_using_heap(unsigned long a1, short a2, unsigned long pan, unsigned long volume, unsigned long pitch, char a6, unsigned char a7, unsigned char a8);
DLLIMPORT long _DK_S3DAddSampleToEmitterPri(long, long, long, long, long, long, char, long, long);
DLLIMPORT long _DK_S3DCreateSoundEmitterPri(long, long, long, long, long, long, long, long, long, long);
DLLIMPORT long _DK_S3DSetSoundReceiverPosition(int pos_x, int pos_y, int pos_z);
DLLIMPORT long _DK_S3DSetSoundReceiverOrientation(int ori_a, int ori_b, int ori_c);
DLLIMPORT long _DK_S3DDestroySoundEmitter(long eidx);
DLLIMPORT long _DK_S3DEmitterHasFinishedPlaying(long eidx);
DLLIMPORT long _DK_S3DMoveSoundEmitterTo(long eidx, long x, long y, long z);
DLLIMPORT long _DK_get_best_sound_heap_size(long mem_size);
DLLIMPORT long _DK_S3DInit(void);
DLLIMPORT long _DK_S3DSetNumberOfSounds(long nMaxSounds);
DLLIMPORT long _DK_S3DSetMaximumSoundDistance(long nDistance);

// Global variables
long NoSoundEmitters = SOUND_EMITTERS_MAX;
/******************************************************************************/
// Internal routines
long allocate_free_sound_emitter(void);
void delete_sound_emitter(long idx);
long start_emitter_playing(struct SoundEmitter *emit, long a2, long pan, long volume, long pitch, long a6, long a7, long a8, long a9);
void init_sample_list(void);
void delete_all_sound_emitters(void);
long get_emitter_id(struct SoundEmitter *emit);
long get_sample_id(struct S3DSample *sample);
void kick_out_sample(short smpl_id);
TbBool emitter_is_playing(struct SoundEmitter *emit);
/******************************************************************************/
// Functions

long get_best_sound_heap_size(long mem_size)
{
    //return _DK_get_best_sound_heap_size(mem_size);
    if (mem_size < 8)
    {
      ERRORLOG("Unhandled PhysicalMemory");
      return 0;
    }
    if (mem_size <= 8)
      return 0x100000; // 1MB
    if (mem_size <= 16)
      return 0x200000; // 2MB
    if (mem_size <= 24)
      return 0x500000; // 5MB
    if (mem_size <= 32)
      return 0x800000; // 8MB
    return 0xC00000; // 12MB
}

long dummy_line_of_sight_function(long a1, long a2, long a3, long a4, long a5, long a6)
{
    return 1;
}

long S3DInit(void)
{
    //return _DK_S3DInit();
    // Clear emitters memory
    delete_all_sound_emitters();
    // Reset sound receiver data
    LbMemorySet(&Receiver, 0, sizeof(struct SoundReceiver));
    S3DSetSoundReceiverPosition(0, 0, 0);
    S3DSetSoundReceiverOrientation(0, 0, 0);
    S3DSetSoundReceiverSensitivity(64);
    S3DSetLineOfSightFunction(dummy_line_of_sight_function);
    S3DSetDeadzoneRadius(0);
    S3DSetNumberOfSounds(SOUNDS_MAX_COUNT);
    init_sample_list();
    return 1;
}

long S3DSetNumberOfSounds(long nMaxSounds)
{
    if (nMaxSounds > SOUNDS_MAX_COUNT)
        nMaxSounds = SOUNDS_MAX_COUNT;
    if (nMaxSounds < 1)
        nMaxSounds = 1;
    MaxNoSounds = nMaxSounds;
    return true;
}

static struct SoundEmitter *S3DGetSoundEmitter(long eidx)
{
    if ((eidx < 0) || (eidx >= SOUND_EMITTERS_MAX))
    {
        WARNLOG("Tried to get outranged emitter %ld",eidx);
        return INVALID_SOUND_EMITTER;
    }
    return &emitter[eidx];
}

TbBool S3DSoundEmitterInvalid(struct SoundEmitter *emit)
{
    if (emit == NULL)
        return true;
    if (emit == INVALID_SOUND_EMITTER)
        return true;
    return false;
}

TbBool S3DEmitterIsPlayingSample(long eidx, long smpl_idx, long a2)
{
    struct SoundEmitter *emit;
    struct S3DSample *sample;
    long i;
    emit = S3DGetSoundEmitter(eidx);
    if (S3DSoundEmitterInvalid(emit))
        return false;
    for (i=0; i < MaxNoSounds; i++)
    {
        sample = &SampleList[i];
        if ((sample->field_1F != 0) && (sample->emit_ptr == emit)) {
            if ((sample->field_8 == smpl_idx) && (sample->field_A == a2))
                return true;
        }
    }
    return false;
}

long S3DSetMaximumSoundDistance(long nDistance)
{
    //return _DK_S3DSetMaximumSoundDistance(nDistance);
    if (nDistance > 65536)
        nDistance = 65536;
    if (nDistance < 1)
        nDistance = 1;
    MaxSoundDistance = nDistance;
    return 1;
}

long S3DSetSoundReceiverPosition(int pos_x, int pos_y, int pos_z)
{
    //return _DK_S3DSetSoundReceiverPosition(pos_x, pos_y, pos_z);
    Receiver.pos.val_x = pos_x;
    Receiver.pos.val_y = pos_y;
    Receiver.pos.val_z = pos_z;
    return 1;
}

long S3DSetSoundReceiverOrientation(int ori_a, int ori_b, int ori_c)
{
    //return _DK_S3DSetSoundReceiverOrientation(ori_a, ori_b, ori_c);
    Receiver.orient_a = ori_a & 0x7FF;
    Receiver.orient_b = ori_b & 0x7FF;
    Receiver.orient_c = ori_c & 0x7FF;
    return 1;
}

void S3DSetSoundReceiverFlags(unsigned long nflags)
{
    Receiver.flags = nflags;
}

void S3DSetSoundReceiverSensitivity(unsigned short nsensivity)
{
    Receiver.sensivity = nsensivity;
}

long S3DDestroySoundEmitter(long eidx)
{
    struct SoundEmitter *emit;
    struct S3DSample *sample;
    long i;
    //return _DK_S3DDestroySoundEmitter(eidx);
    emit = S3DGetSoundEmitter(eidx);
    if (S3DSoundEmitterInvalid(emit))
        return false;
    for (i = 0; i < MaxNoSounds; i++)
    {
        sample = &SampleList[i];
        if ((sample->field_1F != 0) && (sample->emit_ptr == emit))
        {
              stop_sample_using_heap(get_emitter_id(emit), sample->field_8, sample->field_A);
              sample->field_1F = 0;
        }
    }
    delete_sound_emitter(eidx);
    return true;
}

TbBool S3DEmitterIsAllocated(long eidx)
{
    struct SoundEmitter *emit;
    emit = S3DGetSoundEmitter(eidx);
    if (S3DSoundEmitterInvalid(emit))
        return false;
    return ((emit->flags & 0x01) != 0);
}

TbBool S3DEmitterHasFinishedPlaying(long eidx)
{
    struct SoundEmitter *emit;
    emit = S3DGetSoundEmitter(eidx);
    if (S3DSoundEmitterInvalid(emit))
        return true;
    return ((emit->flags & 0x02) == 0);
}

TbBool S3DMoveSoundEmitterTo(long eidx, long x, long y, long z)
{
    struct SoundEmitter *emit;
    //return _DK_S3DMoveSoundEmitterTo(eidx, x, y, z);
    if (!S3DEmitterIsAllocated(eidx))
        return false;
    emit = S3DGetSoundEmitter(eidx);
    emit->pos.val_x = x;
    emit->pos.val_y = y;
    emit->pos.val_z = z;
    emit->flags |= 0x04;
    return true;
}

TbBool S3DAddSampleToEmitterPri(long eidx, long a2, long a3, long a4, long a5, long a6, char a7, long a8, long a9)
{
    struct SoundEmitter *emit;
    //return _DK_S3DAddSampleToEmitterPri(emidx, a2, a3, a4, a5, a6, a7, a8, a9);
    emit = S3DGetSoundEmitter(eidx);
    return start_emitter_playing(emit, a2, a3, a4, a5, a6, a7, a8, a9) != 0;
}

long S3DCreateSoundEmitterPri(long x, long y, long z, long a4, long a5, long a6, long a7, long a8, long a9, long a10)
{
    struct SoundEmitter *emit;
    long eidx;
    //return _DK_S3DCreateSoundEmitterPri(x, y, z, a4, a5, a6, a7, a8, a9, a10);
    eidx = allocate_free_sound_emitter();
    emit = S3DGetSoundEmitter(eidx);
    if (S3DSoundEmitterInvalid(emit))
        return 0;
    emit->pos.val_x = x;
    emit->pos.val_y = y;
    emit->pos.val_z = z;
    emit->field_1 = a9;
    emit->field_14 = 100;
    emit->field_15 = 100;
    if (start_emitter_playing(emit, a4, a5, a6, a7, a8, 3, a9, a10))
        return eidx;
    delete_sound_emitter(eidx);
    return 0;
}

TbBool S3DDestroySoundEmitterAndSamples(long eidx)
{
    struct SoundEmitter *emit;
    emit = S3DGetSoundEmitter(eidx);
    if (S3DSoundEmitterInvalid(emit))
        return false;
    stop_emitter_samples(emit);
    delete_sound_emitter(eidx);
    return true;
}

long S3DEmitterIsPlayingAnySample(long eidx)
{
    struct SoundEmitter *emit;
    struct S3DSample *sample;
    long i;
    if (MaxNoSounds <= 0)
        return false;
    emit = S3DGetSoundEmitter(eidx);
    for (i=0; i < MaxNoSounds; i++)
    {
        sample = &SampleList[i];
        if ((sample->field_1F != 0) && (sample->emit_ptr == emit))
            return true;
    }
    return false;
}

void S3DSetLineOfSightFunction(S3D_LineOfSight_Func callback)
{
    LineOfSightFunction = callback;
}

void S3DSetDeadzoneRadius(long dzradius)
{
    deadzone_radius = dzradius;
}

long S3DGetDeadzoneRadius(void)
{
    return deadzone_radius;
}

long get_emitter_id(struct SoundEmitter *emit)
{
    return (long)emit->index + 4000;
}

long get_sample_id(struct S3DSample *sample)
{
    return (long)sample->field_19 + 4000;
}

short sound_emitter_in_use(long eidx)
{
    return S3DEmitterIsAllocated(eidx);
}

long get_sound_distance(struct SoundCoord3d *pos1, struct SoundCoord3d *pos2)
{
    long dist_x,dist_y,dist_z;
    dist_x = (pos1->val_x - pos2->val_x);
    dist_y = (pos1->val_y - pos2->val_y);
    dist_z = (pos1->val_z - pos2->val_z);
    return LbSqrL( dist_y*dist_y + dist_x*dist_x + dist_z*dist_z );
}

long get_emitter_distance(struct SoundReceiver *recv, struct SoundEmitter *emit)
{
    long dist;
    dist = get_sound_distance(&recv->pos, &emit->pos);
    if (dist > MaxSoundDistance-1)
    {
        dist = MaxSoundDistance - 1;
    }
    if (dist < 0)
    {
        dist = 0;
    }
    return dist;
}

long get_emitter_sight(struct SoundReceiver *recv, struct SoundEmitter *emit)
{
    return LineOfSightFunction(recv->pos.val_x, recv->pos.val_y, recv->pos.val_z, emit->pos.val_x, emit->pos.val_y, emit->pos.val_z);
}

long get_angle_sign(long angle_a, long angle_b)
{
    long diff;
    diff = abs((angle_a & 0x7FF) - (angle_b & 0x7FF));
    if (diff > 1024)
        diff = (2048 - diff);
    return diff;
}

long get_angle_difference(long angle_a, long angle_b)
{
    long diff;
    diff = (angle_b & 0x7FF) - (angle_a & 0x7FF);
    if (diff == 0)
        return 0;
    if (abs(diff) > 1024)
    {
      if (diff >= 0)
          diff -= 2048;
      else
          diff += 2048;
    }
    if (diff == 0)
        return 0;
    return diff / abs(diff);
}

long get_emitter_pan(struct SoundReceiver *recv, struct SoundEmitter *emit)
{
    long diff_x,diff_y;
    long angle_a, angle_b;
    long adiff,asign;
    long radius,pan;
    long i;
    if ((recv->flags & 0x01) != 0) {
      return 64;
    }
    diff_x = emit->pos.val_x - recv->pos.val_x;
    diff_y = emit->pos.val_y - recv->pos.val_y;
    // Faster way of doing simple thing: radius = sqrt(dist_x*dist_y);
    radius = LbProportion(abs(diff_x), abs(diff_y));
    if (radius < deadzone_radius) {
      return 64;
    }
    angle_b = LbArcTan(diff_x, diff_y) & 0x7FF;
    angle_a = recv->orient_a;
    adiff = get_angle_difference(angle_a, angle_b);
    asign = get_angle_sign(angle_a, angle_b);
    i = (radius - deadzone_radius) * LbSinL(asign*adiff) >> 16;
    pan = (i << 6) / (MaxSoundDistance - deadzone_radius) + 64;
    if (pan > 127)
        pan = 127;
    if (pan < 0)
        pan = 0;
    return pan;
}

long get_emitter_pitch_from_doppler(struct SoundReceiver *recv, struct SoundEmitter *emit)
{
    long dist_x,dist_y,dist_z;
    long delta;
    long i;
    dist_x = abs(emit->pos.val_x - recv->pos.val_x);
    dist_y = abs(emit->pos.val_y - recv->pos.val_y);
    dist_z = abs(emit->pos.val_z - recv->pos.val_z);
    delta = dist_x + dist_y + dist_z - emit->field_10;
    if (delta > 256)
        delta = 256;
    if (delta < 0)
        delta = 0;
    if (delta <= 0)
      emit->field_15 = 100;
    else
      emit->field_15 = -20 * delta / 256 + 100;
    if (emit->field_14 != emit->field_15)
    {
        i = emit->field_15 - emit->field_14;
        emit->field_14 += (abs(i) >> 1);
    }
    emit->field_10 = dist_x + dist_y + dist_z;
    //texty += 16; // I have no idea what is this.. garbage.
    return emit->field_14;
}


long get_emitter_pan_volume_pitch(struct SoundReceiver *recv, struct SoundEmitter *emit, long *pan, long *volume, long *pitch)
{
    TbBool on_sight;
    long dist;
    long i,n;
    if ((emit->field_1 & 0x08) != 0)
    {
        *volume = 127;
        *pan = 64;
        *pitch = 100;
        return 1;
    }
    dist = get_emitter_distance(recv, emit);
    if ((emit->field_1 & 0x04) != 0) {
        on_sight = 1;
    } else {
        on_sight = get_emitter_sight(recv, emit);
    }
    i = (dist - deadzone_radius);
    if (i < 0) i = 0;
    n = Receiver.sensivity * (127 - 127 * i / (MaxSoundDistance - deadzone_radius)) >> 6;
    if (on_sight) {
        *volume = n;
    } else {
        *volume = n >> 1;
    }
    if (i >= 128) {
        *pan = get_emitter_pan(recv, emit);
    } else {
        *pan = 64;
    }
    if ((emit->flags & 0x04) != 0) {
        *pitch = get_emitter_pitch_from_doppler(recv, emit);
    } else {
        *pitch = 100;
    }
    return 1;
}

long set_emitter_pan_volume_pitch(struct SoundEmitter *emit, long pan, long volume, long pitch)
{
    //TODO!!!
}

TbBool process_sound_emitters(void)
{
    struct SoundEmitter *emit;
    long pan, volume, pitch;
    long i;
    for (i=1; i < NoSoundEmitters; i++)
    {
        emit = S3DGetSoundEmitter(i);
        if ( ((emit->flags & 0x01) != 0) && ((emit->flags & 0x02) != 0) )
        {
            if ( emitter_is_playing(emit) )
            {
                get_emitter_pan_volume_pitch(&Receiver, emit, &pan, &volume, &pitch);
                set_emitter_pan_volume_pitch(emit, pan, volume, pitch);
            } else
            {
                emit->flags ^= 0x02;
            }
        }
    }
    return true;
}

TbBool emitter_is_playing(struct SoundEmitter *emit)
{
    struct S3DSample *sample;
    long i;
    for (i=0; i < MaxNoSounds; i++)
    {
        sample = &SampleList[i];
        if ((sample->field_1F != 0) && (sample->emit_ptr == emit))
            return true;
    }
    return false;
}

/*long get_angle_difference(long a1, long a2)
{

}*/

long remove_active_samples_from_emitter(struct SoundEmitter *emit)
{
    struct S3DSample *sample;
    long i;
    for (i=0; i < MaxNoSounds; i++)
    {
        sample = &SampleList[i];
        if ( (sample->field_1F != 0) && (sample->emit_ptr == emit) )
        {
            if (sample->field_1D == -1)
            {
                stop_sample_using_heap(get_emitter_id(emit), sample->field_8, sample->field_A);
                sample->field_1F = 0;
            }
            sample->emit_ptr = NULL;
        }
    }
    return true;
}

void close_sound_heap(void)
{
    // TODO: use rewritten version when sound routines are rewritten
    _DK_close_sound_heap(); return;

    if (sound_file != -1)
    {
        LbFileClose(sound_file);
        sound_file = -1;
    }
    if (sound_file2 != -1)
    {
        LbFileClose(sound_file2);
        sound_file2 = -1;
    }
    using_two_banks = 0;
}

short find_slot(long a1, long a2, struct SoundEmitter *emit, long a4, long a5)
{
    struct S3DSample *sample;
    long spcval;
    short min_sample_id;
    long i;
    spcval = 2147483647;
    min_sample_id = SOUNDS_MAX_COUNT;
    if ((a4 == 2) || (a4 == 3))
    {
        for (i=0; i < MaxNoSounds; i++)
        {
            sample = &SampleList[i];
            if ( (sample->field_1F) && (sample->emit_ptr != NULL) )
            {
                if ( (sample->emit_ptr->index == emit->index)
                  && (sample->field_8 == a1) && (sample->field_A == a2) )
                    return i;
            }
        }
    }
    for (i=0; i < MaxNoSounds; i++)
    {
        sample = &SampleList[i];
        if (sample->field_1F == 0)
            return i;
        if (spcval > sample->field_0)
        {
            min_sample_id = i;
            spcval = sample->field_0;
        }
    }
    if (spcval >= a5)
    {
        return -1;
    }
    kick_out_sample(min_sample_id);
    return min_sample_id;
}

void play_non_3d_sample(long sample_idx)
{
    if (SoundDisabled)
        return;
    if (GetCurrentSoundMasterVolume() <= 0)
        return;
    if (Non3DEmitter != 0)
      if (!sound_emitter_in_use(Non3DEmitter))
      {
          ERRORLOG("Non 3d Emitter has been deleted!");
          Non3DEmitter = 0;
      }
    if (Non3DEmitter == 0)
    {
        Non3DEmitter = S3DCreateSoundEmitterPri(0, 0, 0, sample_idx, 0, 100, 256, 0, 8, 2147483646);
    } else
    {
        S3DAddSampleToEmitterPri(Non3DEmitter, sample_idx, 0, 100, 256, 0, 3, 8, 2147483646);
    }
}

void play_non_3d_sample_no_overlap(long smpl_idx)
{
    if (SoundDisabled)
        return;
    if (GetCurrentSoundMasterVolume() <= 0)
        return;
    if (Non3DEmitter != 0)
    {
        if (!sound_emitter_in_use(Non3DEmitter))
        {
            ERRORLOG("Non 3d Emitter has been deleted!");
            Non3DEmitter = 0;
        }
    }
    if (Non3DEmitter == 0)
    {
        Non3DEmitter = S3DCreateSoundEmitterPri(0, 0, 0, smpl_idx, 0, 100, 256, 0, 8, 0x7FFFFFFE);
    } else
    if (!S3DEmitterIsPlayingSample(Non3DEmitter, smpl_idx, 0))
    {
        S3DAddSampleToEmitterPri(Non3DEmitter, smpl_idx, 0, 100, 256, 0, 3, 8, 0x7FFFFFFE);
    }
}

/**
 * Initializes and returns sound emitter structure.
 * Returns its index; if no free emitter is found, returns 0.
 */
long allocate_free_sound_emitter(void)
{
    struct SoundEmitter *emit;
    long i;
    for (i=1; i < NoSoundEmitters; i++)
    {
        if (!S3DEmitterIsAllocated(i))
        {
            emit = S3DGetSoundEmitter(i);
            emit->flags = 0x01;
            emit->index = i;
            return i;
        }
    }
    return 0;
}

/**
 * Clears sound emitter structure and marks it as unused.
 */
void delete_sound_emitter(long idx)
{
    struct SoundEmitter *emit;
    if (S3DEmitterIsAllocated(idx))
    {
        emit = S3DGetSoundEmitter(idx);
        LbMemorySet(emit, 0, sizeof(struct SoundEmitter));
    }
}

/**
 * Drastic emitter clearing. Resets memory of all emitters, even unallocated ones.
 * Does not stop any playing samples - these should be cleared before this call.
 */
void delete_all_sound_emitters(void)
{
    struct SoundEmitter *emit;
    long i;
    for (i=0; i < SOUND_EMITTERS_MAX; i++)
    {
        emit = &emitter[i];
        LbMemorySet(emit, 0, sizeof(struct SoundEmitter));
    }
}

void init_sample_list(void)
{
    struct S3DSample *sample;
    long i;
    for (i=0; i < SOUNDS_MAX_COUNT; i++)
    {
        sample = &SampleList[i];
        LbMemorySet(sample, 0, sizeof(struct S3DSample));
    }
}

void increment_sample_times(void)
{
    struct S3DSample *sample;
    long i;
    for (i=0; i < MaxNoSounds; i++)
    {
        sample = &SampleList[i];
        sample->time_turn++;
    }
}

void kick_out_sample(short smpl_id)
{
    struct S3DSample *sample;
    sample = &SampleList[smpl_id];
    sample->field_1F = 0;
    stop_sample_using_heap(get_sample_id(sample), sample->field_8, sample->field_A);
}

long stop_emitter_samples(struct SoundEmitter *emit)
{
    struct S3DSample *sample;
    long num_stopped;
    long i;
    num_stopped = 0;
    for (i=0; i < MaxNoSounds; i++)
    {
        sample = &SampleList[i];
        if ((sample->field_1F != 0) && (sample->emit_ptr == emit))
        {
            stop_sample_using_heap(get_emitter_id(emit), sample->field_8, sample->field_A);
            sample->field_1F = 0;
            num_stopped++;
        }
    }
    return num_stopped;
}

struct HeapMgrHandle *find_handle_for_new_sample(long smpl_len, long smpl_idx, long file_pos, unsigned char bank_id)
{
    struct SampleTable *smp_table;
    struct HeapMgrHandle *hmhandle;
    long i;
    if ((!using_two_banks) && (bank_id > 0))
    {
        ERRORLOG("Trying to use two sound banks when only one has been set up");
        return NULL;
    }
    hmhandle = heapmgr_add_item(sndheap, smpl_len);
    if (hmhandle == NULL)
    {
        while (sndheap->field_10)
        {
            hmhandle = heapmgr_add_item(sndheap, smpl_len);
            if (hmhandle != NULL)
              break;
            i = heapmgr_free_oldest(sndheap);
            if (i < 0)
              break;
            if (i < samples_in_bank)
            {
              smp_table = &sample_table[i];
            } else
            {
              smp_table = &sample_table2[i-samples_in_bank];
            }
            smp_table->hmhandle = hmhandle;
        }
    }
    if (hmhandle == NULL)
        return NULL;
    if (bank_id > 0)
    {
        hmhandle->field_A = samples_in_bank + smpl_idx;
        LbFileSeek(sound_file2, file_pos, Lb_FILE_SEEK_BEGINNING);
        LbFileRead(sound_file2, hmhandle->field_0, smpl_len);
    } else
    {
        hmhandle->field_A = smpl_idx;
        LbFileSeek(sound_file, file_pos, Lb_FILE_SEEK_BEGINNING);
        LbFileRead(sound_file, hmhandle->field_0, smpl_len);
    }
    return hmhandle;
}

struct SampleInfo *play_sample_using_heap(unsigned long a1, short smpl_idx, unsigned long a3, unsigned long a4, unsigned long a5, char a6, unsigned char a7, unsigned char bank_id)
{
    struct SampleInfo *sample;
    struct SampleTable *smp_table;
    if ((!using_two_banks) && (bank_id > 0))
    {
      ERRORLOG("Trying to use two sound banks when only one has been set up");
      return NULL;
    }
    // TODO: use rewritten version when sound routines are rewritten
    return _DK_play_sample_using_heap(a1, smpl_idx, a3, a4, a5, a6, a7, bank_id);

    if (bank_id > 0)
    {
      if (sound_file2 == -1)
        return 0;
      if ((smpl_idx <= 0) || (smpl_idx >= samples_in_bank2))
      {
        ERRORLOG("Sample %d exceeds bank %d bounds",smpl_idx,2);
        return NULL;
      }
      smp_table = &sample_table2[smpl_idx];
    } else
    {
      if (sound_file == -1)
        return 0;
      if ((smpl_idx <= 0) || (smpl_idx >= samples_in_bank))
      {
        ERRORLOG("Sample %d exceeds bank %d bounds",smpl_idx,1);
        return NULL;
      }
      smp_table = &sample_table[smpl_idx];
    }
    if (smp_table->hmhandle == NULL)
      smp_table->hmhandle = find_handle_for_new_sample(smp_table->field_4, smpl_idx, smp_table->field_0, bank_id);
    if (smp_table->hmhandle == NULL)
    {
      ERRORLOG("Can't find handle to play sample %d",smpl_idx);
      return NULL;
    }
    heapmgr_make_newest(sndheap, smp_table->hmhandle);
    sample = PlaySampleFromAddress(a1, smpl_idx, a3, a4, a5, a6, a7, smp_table->hmhandle, smp_table->field_8);
    if (sample == NULL)
    {
      ERRORLOG("Can't start playing sample %d",smpl_idx);
      return NULL;
    }
    sample->field_17 |= 0x01;
    if (bank_id != 0)
      sample->field_17 |= 0x04;
    smp_table->hmhandle->field_8 |= 0x06;
    return sample;
}

void stop_sample_using_heap(unsigned long a1, short a2, unsigned char a3)
{
    //TODO rewrite
    _DK_stop_sample_using_heap(a1, a2, a3);
}

long speech_sample_playing(void)
{
    long sp_emiter;
    if (SoundDisabled)
        return false;
    if (GetCurrentSoundMasterVolume() <= 0)
        return false;
    sp_emiter = SpeechEmitter;
    if (sp_emiter != 0)
    {
        if (S3DEmitterIsAllocated(SpeechEmitter))
        {
          sp_emiter = SpeechEmitter;
        } else
        {
          ERRORLOG("Speech Emitter has been deleted");
          sp_emiter = 0;
        }
    }
    SpeechEmitter = sp_emiter;
    if (sp_emiter == 0)
      return false;
    return S3DEmitterIsPlayingAnySample(sp_emiter);
}

long play_speech_sample(long smpl_idx)
{
    long sp_emiter;
    if (SoundDisabled)
      return false;
    if (GetCurrentSoundMasterVolume() <= 0)
      return false;
    sp_emiter = SpeechEmitter;
    if (sp_emiter != 0)
    {
      if (S3DEmitterIsAllocated(SpeechEmitter))
      {
        sp_emiter = SpeechEmitter;
      } else
      {
        ERRORLOG("Speech Emitter has been deleted");
        sp_emiter = 0;
      }
    }
    SpeechEmitter = sp_emiter;
    if (sp_emiter != 0)
    {
      if (S3DEmitterHasFinishedPlaying(sp_emiter))
        if (S3DAddSampleToEmitterPri(SpeechEmitter, smpl_idx, 1, 100, 256, 0, 3, 8, 2147483647))
          return true;
      return false;
    }
    sp_emiter = S3DCreateSoundEmitterPri(0, 0, 0, smpl_idx, 1, 100, 256, 0, 8, 2147483647);
    SpeechEmitter = sp_emiter;
    if (sp_emiter == 0)
    {
      ERRORLOG("Cannot create speech emitter.");
      return false;
    }
    return true;
}

long start_emitter_playing(struct SoundEmitter *emit, long a2, long pan, long volume, long pitch, long a6, long a7, long a8, long a9)
{
    //TODO rewrite
    return _DK_start_emitter_playing(emit, a2, pan, volume, pitch, a6, a7, a8, a9);
}

/******************************************************************************/
#ifdef __cplusplus
}
#endif
