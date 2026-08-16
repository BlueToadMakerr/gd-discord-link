#include <Geode/Geode.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <argon/argon.hpp>
#include <iomanip>
#include <sstream>
#include "authPopup.hpp"
#include "RpcDisplay.hpp"
#include "RpcSettings.hpp"
#include "directory.hpp"

using namespace geode::prelude;

static std::string getFirebaseUrl() {
    std::string baseUrl = Mod::get()->getSettingValue<std::string>("firebase-url");
    if (baseUrl.empty()) baseUrl = "https://discordgdlinker-default-rtdb.firebaseio.com";
    if (baseUrl.back() == '/') baseUrl.pop_back();
    return baseUrl;
}

static std::string urlEncode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (char c : value) {
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << std::uppercase << '%' << std::setw(2) << int((unsigned char)c) << std::nouppercase;
        }
    }
    return escaped.str();
}

static std::string sanitizeAuth(std::string key) {
    for (char& c : key) {
        if (c == '.' || c == '#' || c == '$' || c == '[' || c == ']') {
            c = '_';
        }
    }
    return key;
}

$on_mod(Loaded) {
    ButtonSettingPressedEventV3(Mod::get(), "show-settings").listen([](auto buttonKey) {
        if (buttonKey == "toggle") {
            auto accountManager = GJAccountManager::get();
            int localAccountID = accountManager->m_accountID;
            if (localAccountID == 0) {
                FLAlertLayer::create(
                    "Error", 
                    "You must be logged into <cg>Geometry Dash</c> to use this button, you silly lil goober :3", 
                    "OK"
                )->show();
                return;
            }
            RpcSettingsPopup::openSettingsPopup(localAccountID);
        }
    }).leak();
}

class $modify(RPCMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;
        if (auto rightMenu = this->getChildByID("right-side-menu")) {
            auto dirBtnSpr = CCSprite::createWithSpriteFrameName("accountBtn_friends_001.png");
            auto dirBtn = CCMenuItemSpriteExtra::create(
                dirBtnSpr,
                this,
                menu_selector(RPCMenuLayer::onDirectoryButtonClicked)
            );
            
            dirBtn->setID("discord-directory-button"_spr);
            rightMenu->addChild(dirBtn);
            rightMenu->updateLayout();
        }
        return true;
    }

    void onDirectoryButtonClicked(CCObject* sender) {
        if (auto popup = DirectoryPopup::create()) {
            popup->show();
        }
    }
};

class $modify(RPCProfilePage, ProfilePage) {
    struct Fields {
        bool m_isLinked = false;
        bool m_hasRpc = false;
        int m_accountID = 0;
        Ref<AuthPopup> m_authPopup;
        
        async::TaskHolder<web::WebResponse> m_webTask;
    };

    bool init(int accountID, bool isMyProfile) {
        if (!ProfilePage::init(accountID, isMyProfile)) return false;
        m_fields->m_accountID = accountID;
        if (m_usernameLabel) {
            checkFirebaseStatus(accountID, isMyProfile);
        }
        return true;
    }

    void checkFirebaseStatus(int accountID, bool isMyProfile) {
        std::string url = fmt::format("{}/users/{}.json", getFirebaseUrl(), accountID);

        m_fields->m_webTask.spawn(
            web::WebRequest().get(url),
            [this, accountID, isMyProfile](web::WebResponse response) {
                if (!response.ok()) return;
                bool registered = false;
                bool hasRpc = false;

                auto jsonRes = response.json();
                if (jsonRes.isOk()) {
                    auto data = jsonRes.unwrap();
                    if (data.isObject()) {
                        if (data.contains("is_active") && data["is_active"].asBool().unwrapOr(false)) {
                            registered = true;
                        }
                        if (data.contains("rpc") && data["rpc"].isArray()) {
                            auto rpcArray = data["rpc"].asArray().unwrap();
                            if (!rpcArray.empty()) {
                                hasRpc = true;
                            }
                        }
                    }
                }

                if (!registered && isMyProfile) {
                    Mod::get()->setSavedValue<std::string>("uid", "");
                }

                if (registered && isMyProfile) {
                    std::string localUid = Mod::get()->getSavedValue<std::string>("uid");
                    if (!localUid.empty()) {
                        std::string privateDataUrl = fmt::format("{}/user_data/{}.json", getFirebaseUrl(), localUid);
                        async::spawn(
                            web::WebRequest().get(privateDataUrl),
                            [this](web::WebResponse privResponse) {
                                std::string body = privResponse.string().unwrapOr("");
                                
                                if (!privResponse.ok() || body == "null" || body.empty()) {
                                    log::warn("Local UID test read failed or UID doesn't exist. Clearing local UID.");
                                    Mod::get()->setSavedValue<std::string>("uid", "");
                                    return;
                                }

                                auto jsonRes = matjson::parse(body);
                                if (jsonRes.isOk()) {
                                    auto json = jsonRes.unwrap();
                                    if (json.contains("account_id")) {
                                        std::string storedAccountId = json["account_id"].asString().unwrapOr("");
                                        std::string currentAccountId = std::to_string(GJAccountManager::sharedState()->m_accountID);
                                        if (storedAccountId != currentAccountId) {
                                            log::warn("Account ID mismatch for local UID! Clearing local UID.");
                                            Mod::get()->setSavedValue<std::string>("uid", "");
                                            return;
                                        }
                                    } else {
                                        Mod::get()->setSavedValue<std::string>("uid", "");
                                    }
                                } else {
                                    log::warn("Failed to parse user data JSON. Clearing local UID.");
                                    Mod::get()->setSavedValue<std::string>("uid", "");
                                }
                            }
                        );
                    }
                }

                m_fields->m_isLinked = registered;
                m_fields->m_hasRpc = hasRpc;
                this->updateStatusIndicator(isMyProfile);
            }
        );
    }

    void updateStatusIndicator(bool isMyProfile) {
        if (!m_usernameLabel) return;
        auto usernameMenu = m_usernameLabel->getParent();
        if (!usernameMenu) return;
        if (auto existingBtn = usernameMenu->getChildByID("discord-status-button"_spr)) {
            existingBtn->removeFromParent();
        }
        if (!isMyProfile && !m_fields->m_isLinked) {
            usernameMenu->updateLayout();
            return;
        }

        CircleBaseColor baseColor;
        if (!m_fields->m_hasRpc) {
            baseColor = CircleBaseColor::Gray;
        } else {
            baseColor = CircleBaseColor::Green;
        }

        if (isMyProfile && !m_fields->m_isLinked) {
            baseColor = CircleBaseColor::Pink;
        }

        auto circle = CircleButtonSprite::create(
            CCNode::create(),
            baseColor,
            CircleBaseSize::MediumAlt
        );
        circle->setScale(0.35f);
        auto statusBtn = CCMenuItemSpriteExtra::create(
            circle,
            this,
            menu_selector(RPCProfilePage::onStatusClicked)
        );

        statusBtn->setID("discord-status-button"_spr);
        usernameMenu->addChild(statusBtn);
        usernameMenu->updateLayout();
    }

    void onStatusClicked(CCObject* sender) {
        int myAccountID = GJAccountManager::sharedState()->m_accountID;
        std::string localUid = Mod::get()->getSavedValue<std::string>("uid");
        bool isMyProfile = (m_fields->m_accountID == myAccountID);

        if (m_fields->m_isLinked && !localUid.empty()) {
            rpc_display::showRpcDetailsPopup(m_fields->m_accountID, m_fields->m_isLinked, m_fields->m_hasRpc, isMyProfile);
        } else if (isMyProfile) {
            auto onSuccess = [this]() {
                this->m_fields->m_isLinked = true;
                this->updateStatusIndicator(true);
            };

            if (!m_fields->m_isLinked) {
                AuthPopup::showSetupDisclaimer(myAccountID, onSuccess);
                return;
            }
            rpc_display::showRpcDetailsPopup(m_fields->m_accountID, m_fields->m_isLinked, m_fields->m_hasRpc, isMyProfile);
        } else {
            rpc_display::showRpcDetailsPopup(m_fields->m_accountID, m_fields->m_isLinked, m_fields->m_hasRpc, isMyProfile);
        }
    }
};