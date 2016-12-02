//
//  KSCrashState.c
//
//  Created by Karl Stenerud on 2012-02-05.
//
//  Copyright (c) 2012 Karl Stenerud. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall remain in place
// in this source code.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//


#include "KSCrashState.h"

#include "KSFileUtils.h"
#include "KSJSONCodec.h"

//#define KSLogger_LocalLevel TRACE
#include "KSLogger.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>


// ============================================================================
#pragma mark - Constants -
// ============================================================================

#define kFormatVersion 1

#define kKeyFormatVersion "version"
#define kKeyCrashedLastLaunch "crashedLastLaunch"
#define kKeyActiveDurationSinceLastCrash "activeDurationSinceLastCrash"
#define kKeyBackgroundDurationSinceLastCrash "backgroundDurationSinceLastCrash"
#define kKeyLaunchesSinceLastCrash "launchesSinceLastCrash"
#define kKeySessionsSinceLastCrash "sessionsSinceLastCrash"
#define kKeySessionsSinceLaunch "sessionsSinceLaunch"


// ============================================================================
#pragma mark - Globals -
// ============================================================================

/** Location where stat file is stored. */
static const char* g_stateFilePath;

/** Current state. */
static KSCrash_State* g_state;


// ============================================================================
#pragma mark - JSON Encoding -
// ============================================================================

static int onBooleanElement(const char* const name, const bool value, void* const userData)
{
    KSCrash_State* state = userData;

    if(strcmp(name, kKeyCrashedLastLaunch) == 0)
    {
        state->crashedLastLaunch = value;
    }

    return KSJSON_OK;
}

static int onFloatingPointElement(const char* const name, const double value, void* const userData)
{
    KSCrash_State* state = userData;

    if(strcmp(name, kKeyActiveDurationSinceLastCrash) == 0)
    {
        state->activeDurationSinceLastCrash = value;
    }
    if(strcmp(name, kKeyBackgroundDurationSinceLastCrash) == 0)
    {
        state->backgroundDurationSinceLastCrash = value;
    }

    return KSJSON_OK;
}

static int onIntegerElement(const char* const name, const int64_t value, void* const userData)
{
    KSCrash_State* state = userData;

    if(strcmp(name, kKeyFormatVersion) == 0)
    {
        if(value != kFormatVersion)
        {
            KSLOG_ERROR("Expected version 1 but got %lld", value);
            return KSJSON_ERROR_INVALID_DATA;
        }
    }
    else if(strcmp(name, kKeyLaunchesSinceLastCrash) == 0)
    {
        state->launchesSinceLastCrash = (int)value;
    }
    else if(strcmp(name, kKeySessionsSinceLastCrash) == 0)
    {
        state->sessionsSinceLastCrash = (int)value;
    }

    // FP value might have been written as a whole number.
    return onFloatingPointElement(name, value, userData);
}

static int onNullElement(__unused const char* const name, __unused void* const userData)
{
    return KSJSON_OK;
}

static int onStringElement(__unused const char* const name,
                           __unused const char* const value,
                           __unused void* const userData)
{
    return KSJSON_OK;
}

static int onBeginObject(__unused const char* const name, __unused void* const userData)
{
    return KSJSON_OK;
}

static int onBeginArray(__unused const char* const name, __unused void* const userData)
{
    return KSJSON_OK;
}

static int onEndContainer(__unused void* const userData)
{
    return KSJSON_OK;
}

static int onEndData(__unused void* const userData)
{
    return KSJSON_OK;
}


/** Callback for adding JSON data.
 */
static int addJSONData(const char* const data, const int length, void* const userData)
{
    const int fd = *((int*)userData);
    const bool success = ksfu_writeBytesToFD(fd, data, length);
    return success ? KSJSON_OK : KSJSON_ERROR_CANNOT_ADD_DATA;
}


// ============================================================================
#pragma mark - Utility -
// ============================================================================

static double getCurentTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + (double)tv.tv_usec / 1000000.0;
}

static double timeSince(double timeInSeconds)
{
    return getCurentTime() - timeInSeconds;
}

/** Load the persistent state portion of a crash context.
 *
 * @param context The context to load into.
 *
 * @param path The path to the file to read.
 *
 * @return true if the operation was successful.
 */
bool loadState(KSCrash_State* const context,
                              const char* const path)
{
    // Stop if the file doesn't exist.
    // This is expected on the first run of the app.
    const int fd = open(path, O_RDONLY);
    if(fd < 0)
    {
        return false;
    }
    close(fd);

    char* data;
    int length;
    if(!ksfu_readEntireFile(path, &data, &length))
    {
        KSLOG_ERROR("%s: Could not load file", path);
        return false;
    }

    KSJSONDecodeCallbacks callbacks;
    callbacks.onBeginArray = onBeginArray;
    callbacks.onBeginObject = onBeginObject;
    callbacks.onBooleanElement = onBooleanElement;
    callbacks.onEndContainer = onEndContainer;
    callbacks.onEndData = onEndData;
    callbacks.onFloatingPointElement = onFloatingPointElement;
    callbacks.onIntegerElement = onIntegerElement;
    callbacks.onNullElement = onNullElement;
    callbacks.onStringElement = onStringElement;

    int errorOffset = 0;

    char stringBuffer[1000];
    const int result = ksjson_decode(data,
                                     (int)length,
                                     stringBuffer,
                                     sizeof(stringBuffer),
                                     &callbacks,
                                     context,
                                     &errorOffset);
    free(data);
    if(result != KSJSON_OK)
    {
        KSLOG_ERROR("%s, offset %d: %s",
                    path, errorOffset, ksjson_stringForError(result));
        return false;
    }
    return true;
}

/** Save the persistent state portion of a crash context.
 *
 * @param state The context to save from.
 *
 * @param path The path to the file to create.
 *
 * @return true if the operation was successful.
 */
bool saveState(const KSCrash_State* const state,
                              const char* const path)
{
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if(fd < 0)
    {
        KSLOG_ERROR("Could not open file %s for writing: %s",
                    path,
                    strerror(errno));
        return false;
    }

    KSJSONEncodeContext JSONContext;
    ksjson_beginEncode(&JSONContext,
                       true,
                       addJSONData,
                       &fd);

    int result;
    if((result = ksjson_beginObject(&JSONContext, NULL)) != KSJSON_OK)
    {
        goto done;
    }
    if((result = ksjson_addIntegerElement(&JSONContext,
                                          kKeyFormatVersion,
                                          kFormatVersion)) != KSJSON_OK)
    {
        goto done;
    }
    // Record this launch crashed state into "crashed last launch" field.
    if((result = ksjson_addBooleanElement(&JSONContext,
                                          kKeyCrashedLastLaunch,
                                          state->crashedThisLaunch)) != KSJSON_OK)
    {
        goto done;
    }
    if((result = ksjson_addFloatingPointElement(&JSONContext,
                                                kKeyActiveDurationSinceLastCrash,
                                                state->activeDurationSinceLastCrash)) != KSJSON_OK)
    {
        goto done;
    }
    if((result = ksjson_addFloatingPointElement(&JSONContext,
                                                kKeyBackgroundDurationSinceLastCrash,
                                                state->backgroundDurationSinceLastCrash)) != KSJSON_OK)
    {
        goto done;
    }
    if((result = ksjson_addIntegerElement(&JSONContext,
                                          kKeyLaunchesSinceLastCrash,
                                          state->launchesSinceLastCrash)) != KSJSON_OK)
    {
        goto done;
    }
    if((result = ksjson_addIntegerElement(&JSONContext,
                                          kKeySessionsSinceLastCrash,
                                          state->sessionsSinceLastCrash)) != KSJSON_OK)
    {
        goto done;
    }
    result = ksjson_endEncode(&JSONContext);

done:
    close(fd);
    if(result != KSJSON_OK)
    {
        KSLOG_ERROR("%s: %s",
                    path, ksjson_stringForError(result));
        return false;
    }
    return true;
}


// ============================================================================
#pragma mark - API -
// ============================================================================

bool kscrashstate_init(const char* const stateFilePath, KSCrash_State* const state)
{
    g_stateFilePath = strdup(stateFilePath);
    g_state = state;

    loadState(g_state, g_stateFilePath);
    return kscrashstate_reset();
}

bool kscrashstate_reset()
{
    g_state->sessionsSinceLaunch = 1;
    g_state->activeDurationSinceLaunch = 0;
    g_state->backgroundDurationSinceLaunch = 0;
    if(g_state->crashedLastLaunch)
    {
        g_state->activeDurationSinceLastCrash = 0;
        g_state->backgroundDurationSinceLastCrash = 0;
        g_state->launchesSinceLastCrash = 0;
        g_state->sessionsSinceLastCrash = 0;
    }
    g_state->crashedThisLaunch = false;
    
    // Simulate first transition to foreground
    g_state->launchesSinceLastCrash++;
    g_state->sessionsSinceLastCrash++;
    g_state->applicationIsInForeground = true;
    
    return saveState(g_state, g_stateFilePath);
}

void kscrashstate_notifyAppActive(const bool isActive)
{
    KSCrash_State* const state = g_state;

    state->applicationIsActive = isActive;
    if(isActive)
    {
        state->appStateTransitionTime = getCurentTime();
    }
    else
    {
        double duration = timeSince(state->appStateTransitionTime);
        state->activeDurationSinceLaunch += duration;
        state->activeDurationSinceLastCrash += duration;
    }
}

void kscrashstate_notifyAppInForeground(const bool isInForeground)
{
    KSCrash_State* const state = g_state;
    const char* const stateFilePath = g_stateFilePath;

    state->applicationIsInForeground = isInForeground;
    if(isInForeground)
    {
        double duration = getCurentTime() - state->appStateTransitionTime;
        state->backgroundDurationSinceLaunch += duration;
        state->backgroundDurationSinceLastCrash += duration;
        state->sessionsSinceLastCrash++;
        state->sessionsSinceLaunch++;
    }
    else
    {
        state->appStateTransitionTime = getCurentTime();
        saveState(state, stateFilePath);
    }
}

void kscrashstate_notifyAppTerminate(void)
{
    KSCrash_State* const state = g_state;
    const char* const stateFilePath = g_stateFilePath;

    const double duration = timeSince(state->appStateTransitionTime);
    state->backgroundDurationSinceLastCrash += duration;
    saveState(state, stateFilePath);
}

void kscrashstate_notifyAppCrash(void)
{
    KSCrash_State* const state = g_state;
    const char* const stateFilePath = g_stateFilePath;

    const double duration = timeSince(state->appStateTransitionTime);
    if(state->applicationIsActive)
    {
        state->activeDurationSinceLaunch += duration;
        state->activeDurationSinceLastCrash += duration;
    }
    else if(!state->applicationIsInForeground)
    {
        state->backgroundDurationSinceLaunch += duration;
        state->backgroundDurationSinceLastCrash += duration;
    }
    state->crashedThisLaunch = true;
    saveState(state, stateFilePath);
}

const KSCrash_State* const kscrashstate_currentState(void)
{
    return g_state;
}
