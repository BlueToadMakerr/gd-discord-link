#include <Geode/Geode.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/loader/SettingV3.hpp>
#include <argon/argon.hpp>
#include <iomanip>
#include <sstream>
#include "authPopup.hpp"
#include "RpcDisplay.hpp"
#include "RpcSettings.hpp"

using namespace geode::prelude;

static std::string getFirebaseUrl() {
    std::string url = Mod::get()->getSettingValue<std::string>("firebase-url");
    if (url.empty()) {
        url = "https://discordgdlinker-default-rtdb.firebaseio.com";
    }
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    return url;
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
                    "You must be logged into a <cg>Geometry Dash</c> account to use this button, you silly lil goober :3", 
                    "OK"
                )->show();
                return;
            }
            RpcSettingsPopup::create(localAccountID)->show();
        }
    }).leak();
}

class $modify(RPCProfilePage, ProfilePage) {
    struct Fields {
        bool m_isLinked = false;
        bool m_hasRpc = false;
        int m_accountID = 0;
        bool m_isPolling = false;
        std::string m_authtoken;
        Ref<AuthPopup> m_authPopup;
        
        async::TaskHolder<Result<std::string>> m_argonTask;
        async::TaskHolder<web::WebResponse> m_oauthUrlTask;
        async::TaskHolder<web::WebResponse> m_webTask;
        async::TaskHolder<web::WebResponse> m_pollTask;
    };

    bool init(int accountID, bool isMyProfile) {
        if (!ProfilePage::init(accountID, isMyProfile)) return false;

        m_fields->m_accountID = accountID;

        if (m_usernameLabel) {
            checkFirebaseStatus(accountID, isMyProfile);
        }

        return true;
    }

    void loadCommentsFinished(cocos2d::CCArray* comments, char const* key) {
        ProfilePage::loadCommentsFinished(comments, key);
        if (m_usernameLabel) {
            checkFirebaseStatus(m_fields->m_accountID, m_fields->m_accountID == GJAccountManager::sharedState()->m_accountID);
        }
    }

    void checkFirebaseStatus(int accountID, bool isMyProfile) {
        std::string url = fmt::format("{}/users/{}.json", getFirebaseUrl(), accountID);

        m_fields->m_webTask.spawn(
            web::WebRequest().get(url),
            [this, accountID, isMyProfile](web::WebResponse response) {
                bool registered = false;
                bool hasRpc = false;

                if (response.ok()) {
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

        if (!isMyProfile && !m_fields->m_isLinked) {
            if (auto existingMenu = m_usernameLabel->getParent()->getChildByID("discord-status-menu"_spr)) {
                existingMenu->removeFromParent();
            }
            return;
        }
        auto usernameNode = m_usernameLabel;
        if (auto existingMenu = usernameNode->getParent()->getChildByID("discord-status-menu"_spr)) {
            existingMenu->removeFromParent();
        }

        auto parentMenu = CCMenu::create();
        parentMenu->setID("discord-status-menu"_spr);
        float xOffset = (usernameNode->getScaledContentSize().width / 2.0f) + 16.0f;
        parentMenu->setPosition(usernameNode->getPosition() + ccp(xOffset, 0.0f));
        CircleBaseColor baseColor;

        if (!m_fields->m_hasRpc) {
            baseColor = CircleBaseColor::Gray;
        } else {
            baseColor = CircleBaseColor::Green;
        }

        if (isMyProfile) {
            if (!m_fields->m_isLinked) {
                baseColor = CircleBaseColor::Pink;
            }
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

        parentMenu->addChild(statusBtn);
        usernameNode->getParent()->addChild(parentMenu);
    }

    void onStatusClicked(CCObject* sender) {
        int myAccountID = GJAccountManager::sharedState()->m_accountID;
        std::string localUid = Mod::get()->getSavedValue<std::string>("uid");
        bool isMyProfile = (m_fields->m_accountID == myAccountID);

        if (m_fields->m_isLinked && !localUid.empty()) {
            rpc_display::showRpcDetailsPopup(m_fields->m_accountID, m_fields->m_isLinked, m_fields->m_hasRpc, isMyProfile);
        } else if (m_fields->m_accountID == myAccountID) {
            if (m_fields->m_isLinked) {
                showReauthPopup();
            } else {
                showSetupDisclaimer();
            }
        } else {
            rpc_display::showRpcDetailsPopup(m_fields->m_accountID, m_fields->m_isLinked, m_fields->m_hasRpc, isMyProfile);
        }
    }

    void showSetupStep3() {
        geode::createQuickPopup(
            "Link Discord RPC",
            "By continuing, you agree to the <cs>Terms of Service</c> and <cj>Privacy Policy</c>\n\n"
            "Click <cg>Start Auth</c> to open start authenticating! :3.",
            "Cancel", "Start Auth",
            [this](auto, bool btn2) {
                if (btn2) {
                    this->beginArgonAuth();
                }
            }
        );
    }

    void showSetupStep2() {
        geode::createQuickPopup(
            "Link Discord RPC",
            "Note about linking your <cb>Discord</c> account.\n\n"
            "Your <cb>Discord</c> UID will be stored alongside your <cg>Geometry Dash</c> Account ID (This is not visible to other players)\n"
            "When connecting to <cb>Discord</c>, you will be put in a server, this is needed to grab your RPC status!\n\n"
            "If you do not agree then click <cr>Cancel</c>!",
            "Cancel", "Next",
            [this](auto, bool btn2) {
                if (btn2) {
                    this->showSetupStep3();
                }
            }
        );
    }

    void showSetupDisclaimer() {
        geode::createQuickPopup(
            "Link Discord RPC",
            "<cy>Setup Info:</c>\n\n"
            "Your Geometry Dash account will be verified againsed <cl>Argon</c>.\n"
            "Onced verified, a browser window will open up to connect your <cb>Discord</c> account.\n",
            "Cancel", "Next",
            [this](auto, bool btn2) {
                if (btn2) {
                    this->showSetupStep2();
                }
            }
        );
    }

    void showReauthPopup() {
        geode::createQuickPopup(
            "Relink Discord RPC",
            "<cy>You need to relink your Discord account to GD!</c>\n\n"
            "We lost your UID that is required to manage your account on here...\n"
            "This usually happens whenever you use a diffrent device with this mod.\n",
            "Cancel", "Auth",
            [this](auto, bool btn2) {
                if (btn2) {
                    this->beginArgonAuth();
                }
            }
        );
    }

    void cancelAuth() {
        m_fields->m_argonTask.cancel();
        m_fields->m_oauthUrlTask.cancel();
        m_fields->m_pollTask.cancel();
        m_fields->m_isPolling = false;
        m_fields->m_authPopup = nullptr;
    }

    void beginArgonAuth() {
        auto popup = AuthPopup::create([this]() {
            this->cancelAuth();
        });
        m_fields->m_authPopup = popup;
        popup->show();
        
        popup->setStatus("Authenticating with Argon...");

        m_fields->m_argonTask.spawn(
            argon::startAuth(),
            [this](Result<std::string> result) {
                if (!m_fields->m_authPopup) return;

                if (result.isOk()) {
                    std::string token = std::move(result).unwrap();
                    std::string encodedToken = urlEncode(token);
                    m_fields->m_authtoken = sanitizeAuth(token);

                    m_fields->m_authPopup->setStatus("Connecting to Auth Server...");

                    std::string oauthDbUrl = fmt::format("{}/oauth_url.json", getFirebaseUrl());
                    
                    m_fields->m_oauthUrlTask.spawn(
                        web::WebRequest().get(oauthDbUrl),
                        [this, encodedToken](web::WebResponse response) {
                            if (!m_fields->m_authPopup) return;

                            std::string initAuthUrl = "";
                            if (response.ok()) {
                                auto jsonRes = response.json();
                                if (jsonRes.isOk()) {
                                    auto val = jsonRes.unwrap();
                                    if (val.isString()) {
                                        initAuthUrl = val.asString().unwrapOr("");
                                    }
                                }
                            }

                            if (initAuthUrl.empty()) {
                                m_fields->m_authPopup->closePopup();
                                m_fields->m_authPopup = nullptr;
                                FLAlertLayer::create("Error", "Failed to fetch OAuth URL from database...\n\nPlease make sure the DB url is correct. If you aren't sure, reset it!", "OK")->show();
                                return;
                            }

                            int myAccountID = GJAccountManager::sharedState()->m_accountID;
                            std::string fullUrl = fmt::format(
                                "{}?account_id={}&authtoken={}",
                                initAuthUrl,
                                myAccountID,
                                encodedToken
                            );

                            web::openLinkInBrowser(fullUrl);

                            m_fields->m_authPopup->setStatus("Waiting for browser...");
                            m_fields->m_isPolling = true;
                            this->pollFirebase();
                        }
                    );

                } else {
                    m_fields->m_authPopup->closePopup();
                    m_fields->m_authPopup = nullptr;

                    FLAlertLayer::create(
                        "Argon Verification Failed",
                        fmt::format("Error: {}", result.unwrapErr()),
                        "OK"
                    )->show();
                }
            }
        );
    }

    void pollFirebaseCallback() {
        this->pollFirebase();
    }

    void pollFirebase() {
        if (!m_fields->m_isPolling) return;
        std::string url = fmt::format("{}/auth_tokens/{}.json", getFirebaseUrl(), m_fields->m_authtoken);
        
        m_fields->m_pollTask.spawn(
            web::WebRequest().get(url),
            [this, url](web::WebResponse response) {
                if (!m_fields->m_isPolling) return; 

                bool success = false;
                std::string uid = "";

                if (response.ok()) {
                    auto jsonRes = response.json();
                    if (jsonRes.isOk()) {
                        auto data = jsonRes.unwrap();
                        if (data.isObject() && data.contains("uid")) {
                            uid = data["uid"].asString().unwrapOr("");
                            if (!uid.empty()) {
                                success = true;
                            }
                        }
                    }
                }

                if (success) {
                    m_fields->m_isPolling = false;
                    Mod::get()->setSavedValue("uid", uid);
                    std::string deleteUrl = url + "?x-http-method-override=DELETE";
                    async::spawn(
                        web::WebRequest().post(deleteUrl),
                        [](web::WebResponse response) {}
                    );
                    
                    if (m_fields->m_authPopup) {
                        m_fields->m_authPopup->markSuccess(); 
                        m_fields->m_authPopup->closePopup();
                        m_fields->m_authPopup = nullptr;
                    }

                    this->m_fields->m_isLinked = true;
                    this->updateStatusIndicator(true);

                    FLAlertLayer::create(
                        "Success!",
                        "<cg>Successfully linked your Geometry Dash account to Discord!</c>\n\nYour presence data will now sync.",
                        "OK"
                    )->show();
                } else {
                    auto delay = CCDelayTime::create(2.0f);
                    auto call = CCCallFunc::create(this, callfunc_selector(RPCProfilePage::pollFirebaseCallback));
                    this->runAction(CCSequence::create(delay, call, nullptr));
                }
            }
        );
    }
};