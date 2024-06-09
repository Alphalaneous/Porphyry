#pragma once

#include <Geode/cocos/base_nodes/CCNode.h>
#include <Geode/cocos/cocoa/CCGeometry.h>
#include <Geode/cocos/sprite_nodes/CCSprite.h>
#include <Geode/loader/Loader.hpp>
#include <GUI/CCControlExtension/CCScale9Sprite.h>
#include "ui/mods/popups/DevPopup.hpp"

using namespace geode::prelude;

class ModDeveloperItem : public CCNode {
protected:
    CCScale9Sprite* m_bg;
    DevListPopup* m_popup;
public:
    static ModDeveloperItem* create(DevListPopup* popup, std::string developer, CCSize const& size);
private:
    bool init(DevListPopup* popup, std::string developer, CCSize const& size);
    void onMoreByThisDev(CCObject* sender);
};
