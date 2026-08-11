#include "authPopup.hpp"

bool AuthPopup::init(std::function<void()> cancelCallback) {
    if (!Popup::init(260.f, 180.f)) return false;

    m_cancelCallback = cancelCallback;
    this->setTitle("Linking Discord", "goldFont.fnt");

    // Spinner
    m_spinner = CCSprite::create("loadingCircle.png");
    m_spinner->::AuthPopup::setPosition(m_size.width / 2, m_size.height / 2 + 10);
    m_spinner->setScale(0.8f);
    m_spinner->runAction(CCRepeatForever::create(CCRotateBy::create(1.0f, 360.0f)));
    m_mainLayer->addChild(m_spinner);

    // Text
    m_statusLabel = CCLabelBMFont::create("Loading...", "chatFont.fnt");
    m_statusLabel->setPosition(m_size.width / 2, m_size.height / 2 - 35);
    m_statusLabel->setScale(0.7f);
    m_mainLayer->addChild(m_statusLabel);

    // Cancel Button (I'm so good at making comments :3)
    auto cancelBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Cancel"),
        this,
        menu_selector(AuthPopup::onCancel)
    );
    cancelBtn->setPosition(m_size.width / 2, 25);
    m_buttonMenu->addChild(cancelBtn);

    return true;
}

AuthPopup* AuthPopup::create(std::function<void()> cancelCallback) {
    auto ret = new AuthPopup();
    if (ret && ret->init(cancelCallback)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

void AuthPopup::setStatus(const std::string& status) {
    if (m_statusLabel) m_statusLabel->setString(status.c_str());
}

void AuthPopup::markSuccess() {
    m_cancelCallback = nullptr;
}

void AuthPopup::onCancel(CCObject* sender) {
    this->onClose(sender);
}

void AuthPopup::closePopup() {
    this->onClose(nullptr);
}

void AuthPopup::onClose(CCObject* sender) {
    if (m_cancelCallback) {
        m_cancelCallback();
        m_cancelCallback = nullptr;
    }
    Popup::onClose(sender);
}