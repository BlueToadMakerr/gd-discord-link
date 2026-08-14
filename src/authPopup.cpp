#include "authPopup.hpp"
#include <iomanip>
#include <sstream>

namespace {
    std::string getFirebaseUrl() {
        std::string baseUrl = Mod::get()->getSettingValue<std::string>("firebase-url");
        if (baseUrl.empty()) baseUrl = "https://discordgdlinker-default-rtdb.firebaseio.com";
        if (baseUrl.back() == '/') baseUrl.pop_back();
        return baseUrl;
    }

    std::string urlEncode(const std::string &value) {
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

    std::string sanitizeAuth(std::string key) {
        for (char& c : key) {
            if (c == '.' || c == '#' || c == '$' || c == '[' || c == ']') {
                c = '_';
            }
        }
        return key;
    }
}

bool AuthPopup::init(int accountID, std::function<void()> successCallback) {
    if (!Popup::init(260.f, 180.f)) return false;

    m_accountID = accountID;
    m_successCallback = successCallback;
    this->setTitle("Linking Discord", "goldFont.fnt");

    // Spinner
    m_spinner = CCSprite::create("loadingCircle.png");
    m_spinner->::AuthPopup::setPosition(m_size.width / 2, m_size.height / 2 + 10);
    m_spinner->setScale(0.8f);
    m_spinner->runAction(CCRepeatForever::create(CCRotateBy::create(1.0f, 360.0f)));
    m_mainLayer->addChild(m_spinner);

    // Text
    m_statusLabel = CCLabelBMFont::create("Authenticating...", "chatFont.fnt");
    m_statusLabel->setPosition(m_size.width / 2, m_size.height / 2 - 35);
    m_statusLabel->setScale(0.7f);
    m_mainLayer->addChild(m_statusLabel);

    // Cancel Button (I am so good at making comments :3)
    auto cancelBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Cancel"),
        this,
        menu_selector(AuthPopup::onCancel)
    );
    cancelBtn->setPosition(m_size.width / 2, 25);
    m_buttonMenu->addChild(cancelBtn);
    this->beginArgonAuth();
    return true;
}

AuthPopup* AuthPopup::create(int accountID, std::function<void()> successCallback) {
    auto ret = new AuthPopup();
    if (ret && ret->init(accountID, successCallback)) {
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
    m_successCallback = nullptr; 
}

void AuthPopup::onCancel(CCObject* sender) {
    this->onClose(sender);
}

void AuthPopup::closePopup() {
    this->onClose(nullptr);
}

void AuthPopup::onClose(CCObject* sender) {
    this->cancelAuth();
    Popup::onClose(sender);
}

void AuthPopup::cancelAuth() {
    m_argonTask.cancel();
    m_oauthUrlTask.cancel();
    m_pollTask.cancel();
    m_isPolling = false;
}

void AuthPopup::beginArgonAuth() {
    this->setStatus("Authenticating with Argon...");

    m_argonTask.spawn(
        argon::startAuth(),
        [this](Result<std::string> result) {
            if (result.isOk()) {
                std::string token = std::move(result).unwrap();
                std::string encodedToken = urlEncode(token);
                m_authtoken = sanitizeAuth(token);
                // This grabs the oaurth url from the database, mainly for self hosting reasons :3
                this->setStatus("Connecting to Auth Server...");
                std::string oauthDbUrl = fmt::format("{}/oauth_url.json", getFirebaseUrl());
                m_oauthUrlTask.spawn(
                    web::WebRequest().get(oauthDbUrl),
                    [this, encodedToken](web::WebResponse response) {
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
                            this->closePopup();
                            FLAlertLayer::create("Error", "Failed to fetch OAuth URL from database...\n\nPlease make sure the DB url is correct. If you aren't sure, reset it!", "OK")->show();
                            return;
                        }

                        std::string fullUrl = fmt::format(
                            "{}?account_id={}&authtoken={}",
                            initAuthUrl,
                            m_accountID,
                            encodedToken
                        );

                        web::openLinkInBrowser(fullUrl);

                        this->setStatus("Waiting for browser...");
                        m_isPolling = true;
                        this->pollFirebase();
                    }
                );

            } else {
                this->closePopup();
                FLAlertLayer::create(
                    "Verification Failed!",
                    fmt::format("<cy>{}</c>", result.unwrapErr()),
                    "OK"
                )->show();
            }
        }
    );
}

void AuthPopup::pollFirebaseCallback() {
    this->pollFirebase();
}

void AuthPopup::pollFirebase() {
    if (!m_isPolling) return;
    std::string url = fmt::format("{}/auth_tokens/{}.json", getFirebaseUrl(), m_authtoken);
    
    m_pollTask.spawn(
        web::WebRequest().get(url),
        [this, url](web::WebResponse response) {
            if (!m_isPolling) return; 
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
                m_isPolling = false;
                Mod::get()->setSavedValue("uid", uid);
                
                std::string deleteUrl = url + "?x-http-method-override=DELETE";
                async::spawn(
                    web::WebRequest().post(deleteUrl),
                    [](web::WebResponse response) {}
                );
                
                if (m_successCallback) {
                    m_successCallback();
                }

                this->markSuccess(); 
                this->closePopup();

                FLAlertLayer::create(
                    "Success!",
                    "<cg>Successfully linked your Geometry Dash account to Discord!</c>\n\nYour presence data will now sync.",
                    "OK"
                )->show();
            } else {
                auto delay = CCDelayTime::create(2.0f);
                auto call = CCCallFunc::create(this, callfunc_selector(AuthPopup::pollFirebaseCallback));
                this->runAction(CCSequence::create(delay, call, nullptr));
            }
        }
    );
}

// Auth disclamers
void AuthPopup::showSetupDisclaimer(int accountID, std::function<void()> successCallback) {
    geode::createQuickPopup(
        "Link Discord RPC",
        "<cy>Setup Info:</c>\n\n"
        "Your <cg>Geometry Dash</c> account will be verified againsed <cl>Argon</c>.\n"
        "Onced verified, a browser window will open up to connect your <cb>Discord</c> account.\n",
        "Cancel", "Next",
        [accountID, successCallback](auto, bool btn2) {
            if (btn2) {
                AuthPopup::showSetupStep2(accountID, successCallback);
            }
        }
    );
}

void AuthPopup::showSetupStep2(int accountID, std::function<void()> successCallback) {
    geode::createQuickPopup(
        "Link Discord RPC",
        "Note about linking your <cb>Discord</c> account:\n\n"
        "Your <cb>Discord</c> UID will be stored alongside your <cg>Geometry Dash</c> Account ID (This is not visible to other players)\n"
        "When connecting to <cb>Discord</c>, you will be put in a server, this is needed to grab your RPC status!\n\n"
        "If you do not agree then click <cr>Cancel</c>!",
        "Cancel", "Next",
        [accountID, successCallback](auto, bool btn2) {
            if (btn2) {
                AuthPopup::showSetupStep3(accountID, successCallback);
            }
        }
    );
}

void AuthPopup::showSetupStep3(int accountID, std::function<void()> successCallback) {
    geode::createQuickPopup(
        "Link Discord RPC",
        "By continuing, you agree to the <cj>Privacy Policy</c>\n\n"
        "Click <cg>Start Auth</c> to open start authenticating! :3.",
        "Cancel", "Start Auth",
        [accountID, successCallback](auto, bool btn2) {
            if (btn2) {
                auto popup = AuthPopup::create(accountID, successCallback);
                if (popup) popup->show();
            }
        }
    );
}

void AuthPopup::showReauthPopup(int accountID, std::function<void()> successCallback) {
    geode::createQuickPopup(
        "Relink Discord RPC",
        "<cy>You need to relink your Discord account to GD!</c>\n\n"
        "We lost your UID that is required to manage your account on here...\n"
        "This usually happens whenever you use a diffrent device with this mod.\n",
        "Cancel", "Auth",
        [accountID, successCallback](auto, bool btn2) {
            if (btn2) {
                auto popup = AuthPopup::create(accountID, successCallback);
                if (popup) popup->show();
            }
        }
    );
}