#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <argon/argon.hpp>

using namespace geode::prelude;

class AuthPopup : public geode::Popup {
protected:
    CCLabelBMFont* m_statusLabel;
    CCSprite* m_spinner;
    
    int m_accountID;
    std::string m_authtoken;
    bool m_isPolling = false;

    async::TaskHolder<Result<std::string>> m_argonTask;
    async::TaskHolder<web::WebResponse> m_oauthUrlTask;
    async::TaskHolder<web::WebResponse> m_pollTask;

    std::function<void()> m_successCallback;

    bool init(int accountID, std::function<void()> successCallback);

public:
    static AuthPopup* create(int accountID, std::function<void()> successCallback);
    
    void setStatus(const std::string& status);
    void markSuccess();
    
    void onCancel(CCObject* sender);
    void closePopup();
    void onClose(CCObject* sender) override;
    
    // Auth Methods
    void cancelAuth();
    void beginArgonAuth();
    void pollFirebase();
    void pollFirebaseCallback();

    // Disclaimer popups
    static void showSetupDisclaimer(int accountID, std::function<void()> successCallback);
    static void showSetupStep2(int accountID, std::function<void()> successCallback);
    static void showSetupStep3(int accountID, std::function<void()> successCallback);
    static void showReauthPopup(int accountID, std::function<void()> successCallback);
};