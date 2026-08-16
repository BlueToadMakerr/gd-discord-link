#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

class RpcSettingsPopup : public geode::Popup {
protected:
    int m_accountID;
    std::string m_uid;
    std::string m_discordID = "Loading...";
    
    // Page 1
    bool m_showPlaying = true;
    bool m_showStreaming = true;
    bool m_showListening = true;
    bool m_showWatching = true;
    bool m_showCompeting = true;

    // Page 2
    bool m_dirListing = true;

    int m_currentPage = 1;
    CCNode* m_pageNode = nullptr;
    CCLabelBMFont* m_discordIdLabel = nullptr;

    bool init(int accountID);
    void loadData();
    void setupUI();
    void loadPage(int page);
    void setupPage1(CCMenu* menu);
    void setupPage2(CCMenu* menu);
    void setupPage3(CCMenu* menu);

    void onSwitchPage(CCObject* sender);
    void onTogglePlaying(CCObject* sender);
    void onToggleStreaming(CCObject* sender);
    void onToggleListening(CCObject* sender);
    void onToggleWatching(CCObject* sender);
    void onToggleCompeting(CCObject* sender);
    void onToggleDirListing(CCObject* sender);

    bool m_isUnlinking = false;
    geode::Notification* m_unlinkNotif = nullptr;
    int m_pollAttempts = 0;
    std::string m_activeRequestNode = "";

    void pollUnlinkStatus();
    void pollUnlinkStatusCallback();
    void onUnlinkRequestFinished();
    void onUnlink(CCObject* sender);
    
    // New Account Actions
    void onLogOut(CCObject* sender);
    void onLogOutAll(CCObject* sender);

    void updateSettings();
    std::string getFirebaseUrl();

public:
    static RpcSettingsPopup* create(int accountID);
    static void openSettingsPopup(int accountID);
};