/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2011      Zynga Inc.

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

#ifndef __CCLAYER_H__
#define __CCLAYER_H__

#include "../base_nodes/CCNode.h"
#include "../include/CCProtocols.h"
#include "../touch_dispatcher/CCTouchDelegateProtocol.h"
#include "../platform/CCAccelerometerDelegate.h"
#include "../keypad_dispatcher/CCKeypadDelegate.h"

#include "../robtop/keyboard_dispatcher/CCKeyboardDelegate.h"
#include "../robtop/mouse_dispatcher/CCMouseDelegate.h"

#include "../cocoa/CCArray.h"
#ifdef EMSCRIPTEN
#include "../base_nodes/CCGLBufferedNode.h"
#endif // EMSCRIPTEN

NS_CC_BEGIN

typedef enum {
	kCCTouchesAllAtOnce,
	kCCTouchesOneByOne,
} ccTouchesMode;

/**
 * @addtogroup layer
 * @{
 */

class CCTouchScriptHandlerEntry;

//
// CCLayer
//
/** @brief CCLayer is a subclass of CCNode that implements the TouchEventsDelegate protocol.

All features from CCNode are valid, plus the following new features:
- It can receive iPhone Touches
- It can receive Accelerometer input
 * @note Robtop Addition: added CCKeyboardDelegate and CCMouseDelegate
*/
class CC_DLL CCLayer : public CCNode, public CCTouchDelegate, public CCAccelerometerDelegate, public CCKeypadDelegate, public CCKeyboardDelegate, public CCMouseDelegate
{
    GEODE_FRIEND_MODIFY
public:
    /**
     *  @js ctor
     */
    CCLayer();
    GEODE_CUSTOM_CONSTRUCTOR_COCOS(CCLayer, CCNode)
    /**
     *  @js NA
     *  @lua NA
     */
    virtual ~CCLayer();
    virtual bool init();
    
    /** create one layer */
    static CCLayer *create(void);
    /**
     *  @js NA
     *  @lua NA
     */
    virtual void onEnter();
    /**
     *  @js NA
     *  @lua NA
     */
    virtual void onExit();
    /**
     *  @js NA
     *  @lua NA
     */
    virtual void onEnterTransitionDidFinish();
    
    // default implements are used to call script callback if exist
    virtual bool ccTouchBegan(CCTouch *pTouch, CCEvent *pEvent);
    virtual void ccTouchMoved(CCTouch *pTouch, CCEvent *pEvent);
    virtual void ccTouchEnded(CCTouch *pTouch, CCEvent *pEvent);
    virtual void ccTouchCancelled(CCTouch *pTouch, CCEvent *pEvent);

    // default implements are used to call script callback if exist
    virtual void ccTouchesBegan(CCSet *pTouches, CCEvent *pEvent);
    virtual void ccTouchesMoved(CCSet *pTouches, CCEvent *pEvent);
    virtual void ccTouchesEnded(CCSet *pTouches, CCEvent *pEvent);
    virtual void ccTouchesCancelled(CCSet *pTouches, CCEvent *pEvent);
    /**
     * @js NA
     * @lua NA
     */
    virtual void didAccelerate(CCAcceleration* pAccelerationValue);
    void registerScriptAccelerateHandler(int nHandler);
    void unregisterScriptAccelerateHandler(void);

    /** If isTouchEnabled, this method is called onEnter. Override it to change the
    way CCLayer receives touch events.
    ( Default: CCTouchDispatcher::sharedDispatcher()->addStandardDelegate(this,0); )
    Example:
    void CCLayer::registerWithTouchDispatcher()
    {
    CCTouchDispatcher::sharedDispatcher()->addTargetedDelegate(this,INT_MIN+1,true);
    }
    @since v0.8.0
    */
    virtual void registerWithTouchDispatcher(void);
    
    /** Register script touch events handler */
    virtual void registerScriptTouchHandler(int nHandler, bool bIsMultiTouches = false, int nPriority = INT_MIN, bool bSwallowsTouches = false);
    /** Unregister script touch events handler */
    virtual void unregisterScriptTouchHandler(void);

    /** whether or not it will receive Touch events.
    You can enable / disable touch events with this property.
    Only the touches of this node will be affected. This "method" is not propagated to it's children.
    @since v0.8.1
    */
    virtual bool isTouchEnabled();
    virtual void setTouchEnabled(bool value);
    
    virtual void setTouchMode(ccTouchesMode mode);
    virtual int getTouchMode();
    
    /** priority of the touch events. Default is 0 */
    virtual void setTouchPriority(int priority);
    virtual int getTouchPriority();

    /** whether or not it will receive Accelerometer events
    You can enable / disable accelerometer events with this property.
    @since v0.8.1
    */
    virtual bool isAccelerometerEnabled();
    virtual void setAccelerometerEnabled(bool value);
    virtual void setAccelerometerInterval(double interval);

    /** whether or not it will receive keypad events
    You can enable / disable accelerometer events with this property.
    it's new in cocos2d-x
    */
    virtual bool isKeypadEnabled();
    virtual void setKeypadEnabled(bool value);

    // @note RobTop Addition
    virtual bool isKeyboardEnabled();
    // @note RobTop Addition
    virtual void setKeyboardEnabled(bool value);

    // @note RobTop Addition
    virtual bool isMouseEnabled();
    // @note RobTop Addition
    virtual void setMouseEnabled(bool value);

    /** Register keypad events handler */
    void registerScriptKeypadHandler(int nHandler);
    /** Unregister keypad events handler */
    void unregisterScriptKeypadHandler(void);

    virtual void keyBackClicked(void);
    virtual void keyMenuClicked(void);
    
    // @note RobTop Addition
    void keyDown(enumKeyCodes);

    // 2.2 additions
    virtual void setPreviousPriority(int);
    virtual int getPreviousPriority();
    
    inline CCTouchScriptHandlerEntry* getScriptTouchHandlerEntry() { return m_pScriptTouchHandlerEntry; };
    inline CCScriptHandlerEntry* getScriptKeypadHandlerEntry() { return m_pScriptKeypadHandlerEntry; };
    inline CCScriptHandlerEntry* getScriptAccelerateHandlerEntry() { return m_pScriptAccelerateHandlerEntry; };
public:   
    bool m_bTouchEnabled;
    bool m_bAccelerometerEnabled;
    bool m_bKeypadEnabled;
    // @note RobTop Addition
    bool m_bKeyboardEnabled;
    // @note RobTop Addition
    bool m_bMouseEnabled;
    
public:
    // Script touch events handler
    CCTouchScriptHandlerEntry* m_pScriptTouchHandlerEntry;
    CCScriptHandlerEntry* m_pScriptKeypadHandlerEntry;
    CCScriptHandlerEntry* m_pScriptAccelerateHandlerEntry;
    
    int m_nTouchPriority;
    ccTouchesMode m_eTouchMode;

    // 2.2 additions
    int m_uPreviousPriority; // no idea
    
private:
    int  excuteScriptTouchHandler(int nEventType, CCTouch *pTouch);
    int  excuteScriptTouchHandler(int nEventType, CCSet *pTouches);
};

#if (CC_TARGET_PLATFORM == CC_PLATFORM_IOS) || (CC_TARGET_PLATFORM == CC_PLATFORM_MAC)
#pragma mark -
#pragma mark CCLayerRGBA
#endif

/** CCLayerRGBA is a subclass of CCLayer that implements the CCRGBAProtocol protocol using a solid color as the background.
 
 All features from CCLayer are valid, plus the following new features that propagate into children that conform to the CCRGBAProtocol:
 - opacity
 - RGB colors
 @since 2.1
 */
class CC_DLL CCLayerRGBA : public CCLayer, public CCRGBAProtocol
{
    GEODE_FRIEND_MODIFY
public:
    CREATE_FUNC(CCLayerRGBA);
    /**
     *  @js ctor
     */
    CCLayerRGBA();
    GEODE_CUSTOM_CONSTRUCTOR_COCOS(CCLayerRGBA, CCLayer)
    /**
     *  @js NA
     *  @lua NA
     */
    virtual ~CCLayerRGBA();
    
    virtual bool init();
    
    virtual GLubyte getOpacity();
    virtual GLubyte getDisplayedOpacity();
    virtual void setOpacity(GLubyte opacity);
    virtual void updateDisplayedOpacity(GLubyte parentOpacity);
    virtual bool isCascadeOpacityEnabled();
    virtual void setCascadeOpacityEnabled(bool cascadeOpacityEnabled);
    
    virtual const ccColor3B& getColor();
    virtual const ccColor3B& getDisplayedColor();
    virtual void setColor(const ccColor3B& color);
    virtual void updateDisplayedColor(const ccColor3B& parentColor);
    virtual bool isCascadeColorEnabled();
    virtual void setCascadeColorEnabled(bool cascadeColorEnabled);
    
    virtual void setOpacityModifyRGB(bool bValue) {CC_UNUSED_PARAM(bValue);}
    virtual bool isOpacityModifyRGB() { return false; }
protected:
	GLubyte		_displayedOpacity, _realOpacity;
	ccColor3B	_displayedColor, _realColor;
	bool		_cascadeOpacityEnabled, _cascadeColorEnabled;
};

//
// CCLayerColor
//
/** @brief CCLayerColor is a subclass of CCLayer that implements the CCRGBAProtocol protocol.

All features from CCLayer are valid, plus the following new features:
- opacity
- RGB colors
*/
class CC_DLL CCLayerColor : public CCLayerRGBA, public CCBlendProtocol
#ifdef EMSCRIPTEN
, public CCGLBufferedNode
#endif // EMSCRIPTEN
{
    GEODE_FRIEND_MODIFY
protected:


    ccVertex2F m_pSquareVertices[4];
    ccColor4F  m_pSquareColors[4];

public:
    /**
     *  @js ctor
     */
    CCLayerColor();
    GEODE_CUSTOM_CONSTRUCTOR_COCOS(CCLayerColor, CCLayerRGBA)
    /**
     *  @js NA
     *  @lua NA
     */
    virtual ~CCLayerColor();

    virtual void draw();
    virtual void setContentSize(const CCSize & var);
    
    static CCLayerColor* create();
    
    /** creates a CCLayer with color, width and height in Points */
    static CCLayerColor * create(const ccColor4B& color, GLfloat width, GLfloat height);
    /** creates a CCLayer with color. Width and height are the window size. */
    static CCLayerColor * create(const ccColor4B& color);

    virtual bool init();
    /** initializes a CCLayer with color, width and height in Points */
    virtual bool initWithColor(const ccColor4B& color, GLfloat width, GLfloat height);
    /** initializes a CCLayer with color. Width and height are the window size. */
    virtual bool initWithColor(const ccColor4B& color);

    /** change width in Points*/
    void changeWidth(GLfloat w);
    /** change height in Points*/
    void changeHeight(GLfloat h);
    /** change width and height in Points
    @since v0.8
    */
    void changeWidthAndHeight(GLfloat w ,GLfloat h);

    /** BlendFunction. Conforms to CCBlendProtocol protocol */
    CC_PROPERTY(ccBlendFunc, m_tBlendFunc, BlendFunc)
   
    virtual void setColor(const ccColor3B &color);
    virtual void setOpacity(GLubyte opacity);

	void addToVertices(cocos2d::CCPoint, cocos2d::CCPoint, cocos2d::CCPoint);
	void setVertices(cocos2d::CCPoint, cocos2d::CCPoint, cocos2d::CCPoint);


protected:
    virtual void updateColor();
};

//
// CCLayerGradient
//
/** @brief CCLayerGradient is a subclass of CCLayerColor that draws gradients across the background.

All features from CCLayerColor are valid, plus the following new features:
- direction
- final color
- interpolation mode

Color is interpolated between the startColor and endColor along the given
vector (starting at the origin, ending at the terminus).  If no vector is
supplied, it defaults to (0, -1) -- a fade from top to bottom.

If 'compressedInterpolation' is disabled, you will not see either the start or end color for
non-cardinal vectors; a smooth gradient implying both end points will be still
be drawn, however.

If ' compressedInterpolation' is enabled (default mode) you will see both the start and end colors of the gradient.

@since v0.99.5
*/
class CC_DLL CCLayerGradient : public CCLayerColor
{
    GEODE_FRIEND_MODIFY
public:
    GEODE_CUSTOM_CONSTRUCTOR_COCOS(CCLayerGradient, CCLayerColor)
    CCLayerGradient() {}


    /** Creates a full-screen CCLayer with a gradient between start and end. */
    static CCLayerGradient* create(const ccColor4B& start, const ccColor4B& end);

    /** Creates a full-screen CCLayer with a gradient between start and end in the direction of v. */
    static CCLayerGradient* create(const ccColor4B& start, const ccColor4B& end, const CCPoint& v);

    virtual bool init();

    virtual void updateColor();
    /** Initializes the CCLayer with a gradient between start and end. 
     *  @js init
     */
    virtual bool initWithColor(const ccColor4B& start, const ccColor4B& end);

    /** Initializes the CCLayer with a gradient between start and end in the direction of v. 
     *  @js init
     */
    virtual bool initWithColor(const ccColor4B& start, const ccColor4B& end, const CCPoint& v);

    CC_PROPERTY_PASS_BY_REF(ccColor3B, m_startColor, StartColor)
    CC_PROPERTY_PASS_BY_REF(ccColor3B, m_endColor, EndColor)
    CC_PROPERTY(GLubyte, m_cStartOpacity, StartOpacity)
    CC_PROPERTY(GLubyte, m_cEndOpacity, EndOpacity)
    CC_PROPERTY_PASS_BY_REF(CCPoint, m_AlongVector, Vector)

	bool getShouldPremultiply() const;
	void setShouldPremultiply(bool);
	void setValues(cocos2d::_ccColor3B const&, unsigned char, cocos2d::_ccColor3B const&, unsigned char, cocos2d::CCPoint const&);


    /** Whether or not the interpolation will be compressed in order to display all the colors of the gradient both in canonical and non canonical vectors
    Default: YES
    */
protected:
    bool m_bCompressedInterpolation;
public:
    virtual void setCompressedInterpolation(bool bCompressedInterpolation);
    virtual bool isCompressedInterpolation();
    
    static CCLayerGradient* create();
};


/** @brief CCMultipleLayer is a CCLayer with the ability to multiplex it's children.
Features:
- It supports one or more children
- Only one children will be active a time
*/
class CC_DLL CCLayerMultiplex : public CCLayer
{
    GEODE_FRIEND_MODIFY
protected:


    unsigned int m_nEnabledLayer;
    CCArray*     m_pLayers;
public:
    GEODE_CUSTOM_CONSTRUCTOR_COCOS(CCLayerMultiplex, CCLayer)
    /**
     * @js ctor
     * @lua NA
     */
    CCLayerMultiplex();
    /**
     * @js NA
     * @lua NA
     */
    virtual ~CCLayerMultiplex();
    /**
     * @js NA
     */
    static CCLayerMultiplex* create();
    
    /** creates a CCMultiplexLayer with an array of layers.
     * @since v2.1
     * @js NA
     */
    static CCLayerMultiplex* createWithArray(CCArray* arrayOfLayers);

    /** creates a CCLayerMultiplex with one or more layers using a variable argument list. 
     * @lua NA
     */
    static CCLayerMultiplex * create(CCLayer* layer, ... );

    /**
     * lua script can not init with undetermined number of variables
     * so add these functions to be used with lua.
     */
    static CCLayerMultiplex * createWithLayer(CCLayer* layer);

    void addLayer(CCLayer* layer);

    /** initializes a MultiplexLayer with one or more layers using a variable argument list. 
     *  @js NA
     *  @lua NA
     */
    bool initWithLayers(CCLayer* layer, va_list params);
    /** switches to a certain layer indexed by n. 
    The current (old) layer will be removed from it's parent with 'cleanup:YES'.
    */

    /** initializes a CCMultiplexLayer with an array of layers
     *  @since v2.1
     *  @lua NA
     */
    bool initWithArray(CCArray* arrayOfLayers);

    void switchTo(unsigned int n);
    /** release the current layer and switches to another layer indexed by n.
    The current (old) layer will be removed from it's parent with 'cleanup:YES'.
    */
    void switchToAndReleaseMe(unsigned int n);
};


// end of layer group
/// @}

NS_CC_END

#endif // __CCLAYER_H__

