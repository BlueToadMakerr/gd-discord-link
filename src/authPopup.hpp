#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

class AuthPopup : public geode::Popup {
protected:
    CCLabelBMFont* m_statusLabel;
    CCSprite* m_spinner;
    std::function<void()> m_cancelCallback;

    bool init(std::function<void()> cancelCallback);

public:
    static AuthPopup* create(std::function<void()> cancelCallback);
    void setStatus(const std::string& status);
    void markSuccess();
    void onCancel(CCObject* sender);
    void closePopup();
    void onClose(CCObject* sender) override;
};