/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2010      Stuart Carnie

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/
#ifndef __SUPPORT_CCPROFILING_H__
#define __SUPPORT_CCPROFILING_H__

#include "../include/ccConfig.h"
#include "../cocoa/CCObject.h"
#include "../platform/platform.h"
#include "../cocoa/CCDictionary.h"
#include <string>

NS_CC_BEGIN

/**
 * @addtogroup global
 * @{
 */

class CCProfilingTimer;

/** CCProfiler
 cocos2d builtin profiler.

 To use it, enable set the CC_ENABLE_PROFILERS=1 in the ccConfig.h file
 *@js NA
 *@lua NA
 */

class CC_DLL CCProfiler : public CCObject
{
    GEODE_FRIEND_MODIFY
public:
    GEODE_CUSTOM_CONSTRUCTOR_COCOS(CCProfiler, CCObject)
    CCProfiler() {}

    ~CCProfiler(void);
    /** display the timers */
    void displayTimers(void);
    bool init(void);

public:
    static CCProfiler* sharedProfiler(void);
    /** Creates and adds a new timer */
    CCProfilingTimer* createAndAddTimerWithName(const char* timerName);
    /** releases a timer */
    void releaseTimer(const char* timerName);
    /** releases all timers */
    void releaseAllTimers();

public:
    CCDictionary* m_pActiveTimers;
};
/**
 *@js NA
 *@lua NA
 */
class CCProfilingTimer : public CCObject
{
public:
    GEODE_CUSTOM_CONSTRUCTOR_COCOS(CCProfilingTimer, CCObject)
    CCProfilingTimer();
    ~CCProfilingTimer();
    bool initWithName(const char* timerName);
    const char* description(void);
    struct cc_timeval* getStartTime(void) { return &m_sStartTime; };
    /** resets the timer properties */
    void reset();

public:
    gd::string m_NameStr;
    int               numberOfCalls;
    int               m_dAverageTime1;
    int               m_dAverageTime2;
    long long         totalTime;
    int               minTime;
    int               maxTime;
    struct cc_timeval m_sStartTime;
};

extern CC_DLL void CCProfilingBeginTimingBlock(const char *timerName);
extern CC_DLL void CCProfilingEndTimingBlock(const char *timerName);
extern CC_DLL void CCProfilingResetTimingBlock(const char *timerName);

/*
 * cocos2d profiling categories
 * used to enable / disable profilers with granularity
 */

extern bool kCCProfilerCategorySprite;
extern bool kCCProfilerCategoryBatchSprite;
extern bool kCCProfilerCategoryParticles;

// end of global group
/// @}

NS_CC_END

#endif // __SUPPORT_CCPROFILING_H__
