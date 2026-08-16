#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/ui/Notification.hpp>
#include "RpcSettings.hpp"
#include "authPopup.hpp"

std::string RpcSettingsPopup::getFirebaseUrl() {
    std::string baseUrl = Mod::get()->getSettingValue<std::string>("firebase-url");
    if (baseUrl.empty()) baseUrl = "https://discordgdlinker-default-rtdb.firebaseio.com";
    if (baseUrl.back() == '/') baseUrl.pop_back();
    return baseUrl;
}

void RpcSettingsPopup::openSettingsPopup(int accountID) {
    std::string uid = Mod::get()->getSavedValue<std::string>("uid");
    auto promptReauth = [accountID]() {
        AuthPopup::showReauthPopup(accountID, [accountID]() {
            RpcSettingsPopup::openSettingsPopup(accountID);
        });
    };
    if (uid.empty()) {
        promptReauth();
        return;
    }
    if (auto popup = RpcSettingsPopup::create(accountID)) {
        popup->show();
    }
}

bool RpcSettingsPopup::init(int accountID) {
    if (!Popup::init(340.f, 260.f)) return false;

    m_accountID = accountID;
    m_uid = Mod::get()->getSavedValue<std::string>("uid");
    
    this->setTitle("RPC Settings");
    
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
                        if (s.contains("dir_listing")) m_dirListing = s["dir_listing"].asBool().unwrapOr(true);
                    }
                    this->setupUI();
                    return;
                }
            }
            FLAlertLayer::create("Error", "Failed to load your settings!", "OK")->show();
            this->onClose(nullptr);
        }
    );
}

void RpcSettingsPopup::setupUI() {
    m_discordIdLabel->setString(fmt::format("Geometry Dash ID: {} | Discord ID: {}", m_accountID, m_discordID).c_str());

    if (!m_pageNode) {
        m_pageNode = CCNode::create();
        m_pageNode->setContentSize(m_size);
        m_mainLayer->addChild(m_pageNode);
    }

    loadPage(m_currentPage);
}

void RpcSettingsPopup::loadPage(int page) {
    m_currentPage = page;
    m_pageNode->removeAllChildren();

    auto menu = CCMenu::create();
    menu->setPosition({m_size.width / 2.f, m_size.height / 2.f});
    m_pageNode->addChild(menu);

    if (m_currentPage == 1) {
        setupPage1(menu);
    } else if (m_currentPage == 2) {
        setupPage2(menu);
    } else if (m_currentPage == 3) {
        setupPage3(menu);
    }

    auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
    auto leftBtn = CCMenuItemSpriteExtra::create(leftSpr, this, menu_selector(RpcSettingsPopup::onSwitchPage));
    leftBtn->setPosition({-190.f, 0.f}); 
    leftBtn->setScale(0.8f);
    leftBtn->setTag(-1);
    menu->addChild(leftBtn);
    
    auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
    rightSpr->setFlipX(true);
    auto rightBtn = CCMenuItemSpriteExtra::create(rightSpr, this, menu_selector(RpcSettingsPopup::onSwitchPage));
    rightBtn->setPosition({190.f, 0.f}); 
    rightBtn->setScale(0.8f);
    rightBtn->setTag(1);
    menu->addChild(rightBtn);
}

void RpcSettingsPopup::setupPage1(CCMenu* menu) {
    auto title = CCLabelBMFont::create("RPC Settings", "bigFont.fnt");
    title->setPosition({0.f, 65.f});
    title->setScale(0.3f);
    menu->addChild(title);

    auto createToggle = [&](const char* name, const char* infoTitle, const char* infoDesc, bool state, SEL_MenuHandler callback, CCPoint pos) {
        auto toggler = CCMenuItemToggler::createWithStandardSprites(this, callback, 0.7f);
        toggler->setPosition(pos);
        toggler->toggle(state);
        menu->addChild(toggler);
        
        auto label = CCLabelBMFont::create(name, "bigFont.fnt");
        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({pos.x + 15.f, pos.y});
        label->setScale(0.5f);
        menu->addChild(label);

        if (infoTitle && infoDesc) {
            auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
            infoSpr->setScale(0.5f);

            auto infoBtn = CCMenuItemExt::createSpriteExtra(infoSpr, [infoTitle, infoDesc](auto) {
                FLAlertLayer::create(infoTitle, infoDesc, "OK")->show();
            });
            infoBtn->setPosition({pos.x - 15.f, pos.y + 15.f});
            menu->addChild(infoBtn);
        }
    };

    createToggle("Playing", nullptr, nullptr, m_showPlaying, menu_selector(RpcSettingsPopup::onTogglePlaying), {-140.f, 25.f});
    createToggle("Streaming", nullptr, nullptr, m_showStreaming, menu_selector(RpcSettingsPopup::onToggleStreaming), {-140.f, -5.f});
    createToggle("Listening", nullptr, nullptr, m_showListening, menu_selector(RpcSettingsPopup::onToggleListening), {-140.f, -35.f});

    createToggle("Watching", nullptr, nullptr, m_showWatching, menu_selector(RpcSettingsPopup::onToggleWatching), {20.f, 25.f});
    createToggle("Competing", nullptr, nullptr, m_showCompeting, menu_selector(RpcSettingsPopup::onToggleCompeting), {20.f, -5.f});
}

void RpcSettingsPopup::setupPage2(CCMenu* menu) {
    auto title = CCLabelBMFont::create("Privacy Settings", "bigFont.fnt");
    title->setPosition({0.f, 65.f});
    title->setScale(0.3f);
    menu->addChild(title);

    auto createToggle = [&](const char* name, const char* infoTitle, const char* infoDesc, bool state, SEL_MenuHandler callback, CCPoint pos) {
        auto toggler = CCMenuItemToggler::createWithStandardSprites(this, callback, 0.7f);
        toggler->setPosition(pos);
        toggler->toggle(state);
        menu->addChild(toggler);
        
        auto label = CCLabelBMFont::create(name, "bigFont.fnt");
        label->setAnchorPoint({0.f, 0.5f});
        label->setPosition({pos.x + 15.f, pos.y});
        label->setScale(0.5f);
        menu->addChild(label);

        if (infoTitle && infoDesc) {
            auto infoSpr = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png");
            infoSpr->setScale(0.5f);

            auto infoBtn = CCMenuItemExt::createSpriteExtra(infoSpr, [infoTitle, infoDesc](auto) {
                FLAlertLayer::create(infoTitle, infoDesc, "OK")->show();
            });
            infoBtn->setPosition({pos.x - 15.f, pos.y + 15.f});
            menu->addChild(infoBtn);
        }
    };

    createToggle("Directory Listing", "Directory Listing", "Allow your profile and activity to appear in the public directory (Online Players) list.", m_dirListing, menu_selector(RpcSettingsPopup::onToggleDirListing), {-140.f, 25.f});
}

void RpcSettingsPopup::setupPage3(CCMenu* menu) {
    auto title = CCLabelBMFont::create("Account Management", "bigFont.fnt");
    title->setPosition({0.f, 65.f});
    title->setScale(0.3f);
    menu->addChild(title);

    auto unlinkSpr = ButtonSprite::create("Unlink Discord Account", "goldFont.fnt", "GJ_button_06.png", 0.7f);
    auto unlinkBtn = CCMenuItemSpriteExtra::create(unlinkSpr, this, menu_selector(RpcSettingsPopup::onUnlink));
    unlinkBtn->setPosition({0.f, 25.f});
    menu->addChild(unlinkBtn);

    auto logoutSpr = ButtonSprite::create("Log Out from This Device", "goldFont.fnt", "GJ_button_01.png", 0.7f);
    auto logoutBtn = CCMenuItemSpriteExtra::create(logoutSpr, this, menu_selector(RpcSettingsPopup::onLogOut));
    logoutBtn->setPosition({0.f, -25.f});
    menu->addChild(logoutBtn);

    auto logoutAllSpr = ButtonSprite::create("Log Out from All Devices", "goldFont.fnt", "GJ_button_06.png", 0.7f);
    auto logoutAllBtn = CCMenuItemSpriteExtra::create(logoutAllSpr, this, menu_selector(RpcSettingsPopup::onLogOutAll));
    logoutAllBtn->setPosition({0.f, -75.f});
    menu->addChild(logoutAllBtn);
}

void RpcSettingsPopup::onSwitchPage(CCObject* sender) {
    int maxPages = 3;
    auto btn = static_cast<CCNode*>(sender);
    int direction = btn->getTag();
    m_currentPage += direction;

    if (m_currentPage > maxPages) {
        m_currentPage = 1;
    } else if (m_currentPage < 1) {
        m_currentPage = maxPages;
    }

    loadPage(m_currentPage);
}

void RpcSettingsPopup::onTogglePlaying(CCObject*) { m_showPlaying = !m_showPlaying; updateSettings(); }
void RpcSettingsPopup::onToggleStreaming(CCObject*) { m_showStreaming = !m_showStreaming; updateSettings(); }
void RpcSettingsPopup::onToggleListening(CCObject*) { m_showListening = !m_showListening; updateSettings(); }
void RpcSettingsPopup::onToggleWatching(CCObject*) { m_showWatching = !m_showWatching; updateSettings(); }
void RpcSettingsPopup::onToggleCompeting(CCObject*) { m_showCompeting = !m_showCompeting; updateSettings(); }
void RpcSettingsPopup::onToggleDirListing(CCObject*) { m_dirListing = !m_dirListing; updateSettings(); }

void RpcSettingsPopup::updateSettings() {
    std::string url = fmt::format("{}/user_data/{}/settings.json?x-http-method-override=PUT", getFirebaseUrl(), m_uid);
    
    std::string payload = fmt::format(
        R"({{"show_playing": {}, "show_streaming": {}, "show_listening": {}, "show_watching": {}, "show_competing": {}, "dir_listing": {}}})", 
        m_showPlaying ? "true" : "false", 
        m_showStreaming ? "true" : "false",
        m_showListening ? "true" : "false",
        m_showWatching ? "true" : "false",
        m_showCompeting ? "true" : "false",
        m_dirListing ? "true" : "false"
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

void RpcSettingsPopup::onLogOut(CCObject* sender) {
    if (m_isUnlinking) return;
    Mod::get()->setSavedValue<std::string>("uid", "");
    FLAlertLayer::create("Logged Out", "You have been logged out.", "OK")->show();
    this->onClose(nullptr);
}

void RpcSettingsPopup::onLogOutAll(CCObject* sender) {
    if (m_isUnlinking) return; 
    geode::createQuickPopup(
        "Confirm Global Log Out",
        "Are you sure you want to log out from <cr>all devices</c>? This will reset your token used for authenticating you, requiring a reauth.",
        "Cancel", "Log Out",
        [this](FLAlertLayer* alert, bool btn2) {
            if (btn2) {
                m_isUnlinking = true;
                m_pollAttempts = 0; 
                m_activeRequestNode = "logout_all_requested";
                m_unlinkNotif = Notification::create("Logging out from all devices...", NotificationIcon::Loading, 0.0f);
                m_unlinkNotif->show();
                
                std::string url = fmt::format("{}/user_data/{}/{}.json?x-http-method-override=PUT", getFirebaseUrl(), m_uid, m_activeRequestNode);
                geode::async::spawn(
                    web::WebRequest().bodyString("true").post(url),
                    [this](web::WebResponse response) { 
                        if (response.ok()) {
                            this->pollUnlinkStatus();
                        } else {
                            m_unlinkNotif->cancel();
                            m_unlinkNotif = nullptr;
                            m_isUnlinking = false;
                            FLAlertLayer::create("Error", "Failed to send global log out request to server.", "OK")->show();
                        }
                    }
                );
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
                m_activeRequestNode = "unlink_requested";
                m_unlinkNotif = Notification::create("Unlinking...", NotificationIcon::Loading, 0.0f);
                m_unlinkNotif->show();
                std::string url = fmt::format("{}/user_data/{}/{}.json?x-http-method-override=PUT", getFirebaseUrl(), m_uid, m_activeRequestNode);
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
        
        std::string cleanupUrl = fmt::format("{}/user_data/{}/{}.json?x-http-method-override=DELETE", getFirebaseUrl(), m_uid, m_activeRequestNode);
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
    
    std::string successMsg = (m_activeRequestNode == "unlink_requested") ? 
        "Your account has been unlinked successfully!" : 
        "You have been successfully logged out of all devices!";
        
    FLAlertLayer::create("Success", successMsg.c_str(), "OK")->show();
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