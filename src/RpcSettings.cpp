#include "RpcSettings.hpp"
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/ui/Notification.hpp>

// TODO: Add a button to config in settings!

std::string RpcSettingsPopup::getFirebaseUrl() {
    std::string baseUrl = Mod::get()->getSettingValue<std::string>("firebase-url");
    if (baseUrl.empty()) baseUrl = "https://discordgdlinker-default-rtdb.firebaseio.com";
    if (baseUrl.back() == '/') baseUrl.pop_back();
    return baseUrl;
}

bool RpcSettingsPopup::init(int accountID) {
    if (!Popup::init(340.f, 260.f)) return false;

    m_accountID = accountID;
    m_uid = Mod::get()->getSavedValue<std::string>("uid");
    
    this->setTitle("RPC Settings");
    
    if (m_uid.empty()) {
        FLAlertLayer::create("Error", "No local UID found! Try re-authenticating.", "OK")->show();
        this->onClose(nullptr);
        return true;
    }

    m_discordIdLabel = CCLabelBMFont::create("Fetching info...", "chatFont.fnt");
    m_discordIdLabel->setPosition({m_size.width / 2.f, m_size.height - 45.f});
    m_discordIdLabel->setScale(0.6f);
    m_mainLayer->addChild(m_discordIdLabel);

    this->loadData();
    return true;
}

void RpcSettingsPopup::loadData() {
    std::string url = fmt::format("{}/user_data/{}.json", getFirebaseUrl(), m_uid);

    geode::async::spawn(
        web::WebRequest().get(url),
        [this](web::WebResponse response) {
            if (response.ok()) {
                auto jsonRes = response.json();
                if (jsonRes.isOk()) {
                    auto data = jsonRes.unwrap();
                    if (data.contains("discord_id") && data["discord_id"].isString()) {
                        m_discordID = data["discord_id"].asString().unwrapOr("Unknown");
                    }
                    if (data.contains("settings") && data["settings"].isObject()) {
                        auto s = data["settings"];
                        if (s.contains("show_playing")) m_showPlaying = s["show_playing"].asBool().unwrapOr(true);
                        if (s.contains("show_streaming")) m_showStreaming = s["show_streaming"].asBool().unwrapOr(true);
                        if (s.contains("show_listening")) m_showListening = s["show_listening"].asBool().unwrapOr(true);
                        if (s.contains("show_watching")) m_showWatching = s["show_watching"].asBool().unwrapOr(true);
                        if (s.contains("show_competing")) m_showCompeting = s["show_competing"].asBool().unwrapOr(true);
                    }
                }
            }
            this->setupUI();
        }
    );
}

void RpcSettingsPopup::setupUI() {
    m_discordIdLabel->setString(fmt::format("Geometry Dash ID: {}\nDiscord ID: {}", m_accountID, m_discordID).c_str());

    auto menu = CCMenu::create();
    menu->setPosition({m_size.width / 2.f, m_size.height / 2.f});
    m_mainLayer->addChild(menu);

    auto createToggle = [&](const char* name, bool state, SEL_MenuHandler callback, CCPoint pos) {
        auto toggler = CCMenuItemToggler::createWithStandardSprites(this, callback, 0.7f);
        toggler->setPosition(pos);
        toggler->toggle(state);
        menu->addChild(toggler);
        auto label = CCLabelBMFont::create(name, "bigFont.fnt");
        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({pos.x + 15.f, pos.y});
        label->setScale(0.5f);
        menu->addChild(label);
    };

    createToggle("Playing", m_showPlaying, menu_selector(RpcSettingsPopup::onTogglePlaying), {-140.f, 25.f});
    createToggle("Streaming", m_showStreaming, menu_selector(RpcSettingsPopup::onToggleStreaming), {-140.f, -5.f});
    createToggle("Listening", m_showListening, menu_selector(RpcSettingsPopup::onToggleListening), {-140.f, -35.f});

    createToggle("Watching", m_showWatching, menu_selector(RpcSettingsPopup::onToggleWatching), {20.f, 25.f});
    createToggle("Competing", m_showCompeting, menu_selector(RpcSettingsPopup::onToggleCompeting), {20.f, -5.f});

    auto unlinkSpr = ButtonSprite::create("Unlink", "goldFont.fnt", "GJ_button_06.png", 0.7f);
    auto unlinkBtn = CCMenuItemSpriteExtra::create(unlinkSpr, this, menu_selector(RpcSettingsPopup::onUnlink));
    unlinkBtn->setPosition({0.f, -85.f});
    menu->addChild(unlinkBtn);
}

void RpcSettingsPopup::onTogglePlaying(CCObject*) { m_showPlaying = !m_showPlaying; updateSettings(); }
void RpcSettingsPopup::onToggleStreaming(CCObject*) { m_showStreaming = !m_showStreaming; updateSettings(); }
void RpcSettingsPopup::onToggleListening(CCObject*) { m_showListening = !m_showListening; updateSettings(); }
void RpcSettingsPopup::onToggleWatching(CCObject*) { m_showWatching = !m_showWatching; updateSettings(); }
void RpcSettingsPopup::onToggleCompeting(CCObject*) { m_showCompeting = !m_showCompeting; updateSettings(); }

void RpcSettingsPopup::updateSettings() {
    std::string url = fmt::format("{}/user_data/{}/settings.json?x-http-method-override=PUT", getFirebaseUrl(), m_uid);
    
    std::string payload = fmt::format(
        R"({{"show_playing": {}, "show_streaming": {}, "show_listening": {}, "show_watching": {}, "show_competing": {}}})", 
        m_showPlaying ? "true" : "false", 
        m_showStreaming ? "true" : "false",
        m_showListening ? "true" : "false",
        m_showWatching ? "true" : "false",
        m_showCompeting ? "true" : "false"
    );

    geode::async::spawn(
        web::WebRequest().bodyString(payload).post(url),
        [](web::WebResponse response) {
            if (!response.ok()) {
                log::error("Failed to update settings in Firebase!");
            }
        }
    );
}

void RpcSettingsPopup::onUnlink(CCObject* sender) {
    if (m_isUnlinking) return; 
    geode::createQuickPopup(
        "Confirm Unlink",
        "Are you sure you want to <cr>unlink</c> your <cb>Discord</c> account from Geometry Dash?",
        "Cancel", "Unlink",
        [this](FLAlertLayer* alert, bool btn2) {
            if (btn2) {
                m_isUnlinking = true;
                m_pollAttempts = 0; 

                if (m_discordIdLabel) {
                    m_discordIdLabel->setString("Unlinking... Please wait.");
                }

                m_unlinkNotif = Notification::create("Unlinking...", NotificationIcon::Loading, 0.0f);
                m_unlinkNotif->show();
                std::string url = fmt::format("{}/user_data/{}/unlink_requested.json?x-http-method-override=PUT", getFirebaseUrl(), m_uid);
                geode::async::spawn(
                    web::WebRequest().bodyString("true").post(url),
                    [this](web::WebResponse response) { 
                        if (response.ok()) {
                            this->pollUnlinkStatus();
                        } else {
                            m_unlinkNotif->cancel();
                            m_unlinkNotif = nullptr;
                            m_isUnlinking = false;
                            FLAlertLayer::create("Error", "Failed to send unlink request to server.", "OK")->show();
                            if (m_discordIdLabel) m_discordIdLabel->setString(fmt::format("GD Account ID: {}\nDiscord ID: {}", m_accountID, m_discordID).c_str());
                        }
                    }
                );
            }
        }
    );
}

void RpcSettingsPopup::pollUnlinkStatus() {
    if (!m_isUnlinking) return;
    if (m_pollAttempts >= 15) {
        m_isUnlinking = false;
        m_unlinkNotif->cancel();
        m_unlinkNotif = nullptr;
        FLAlertLayer::create(
            "Timeout", 
            "The server took too long to respond! Try again?", 
            "OK"
        )->show();
        std::string cleanupUrl = fmt::format("{}/user_data/{}/unlink_requested.json?x-http-method-override=DELETE", getFirebaseUrl(), m_uid);
        geode::async::spawn(web::WebRequest().post(cleanupUrl), [](web::WebResponse) {});
        return;
    }
    
    m_pollAttempts++;
    std::string url = fmt::format("{}/user_data/{}.json", getFirebaseUrl(), m_uid);
    
    geode::async::spawn(
        web::WebRequest().get(url),
        [this](web::WebResponse response) {
            if (!m_isUnlinking) return;
            bool isDeleted = false;
            if (response.ok()) {
                auto str = response.string().unwrapOr("");
                if (str == "null" || str.empty()) {
                    isDeleted = true;
                }
            } else if (response.code() == 404) {
                isDeleted = true;
            }
            if (isDeleted) {
                this->onUnlinkRequestFinished();
            } else {
                auto delay = CCDelayTime::create(2.0f);
                auto call = CCCallFunc::create(this, callfunc_selector(RpcSettingsPopup::pollUnlinkStatusCallback));
                this->runAction(CCSequence::create(delay, call, nullptr));
            }
        }
    );
}

void RpcSettingsPopup::pollUnlinkStatusCallback() {
    this->pollUnlinkStatus();
}

void RpcSettingsPopup::onUnlinkRequestFinished() {
    m_isUnlinking = false;
    m_unlinkNotif->cancel();
    m_unlinkNotif = nullptr;
    Mod::get()->setSavedValue<std::string>("uid", "");
    FLAlertLayer::create("Unlinked", "Your account has been unlinked successfully!", "OK")->show();
    this->onClose(nullptr);
}

RpcSettingsPopup* RpcSettingsPopup::create(int accountID) {
    auto ret = new RpcSettingsPopup();
    if (ret && ret->init(accountID)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}