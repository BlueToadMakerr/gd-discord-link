#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;

class RpcSettingsPopup : public geode::Popup {
protected:
    int m_accountID;
    std::string m_uid;
    std::string m_discordID = "Loading...";
    
    bool m_showPlaying = true;
    bool m_showStreaming = true;
    bool m_showListening = true;
    bool m_showWatching = true;
    bool m_showCompeting = true;

    CCLabelBMFont* m_discordIdLabel = nullptr;

    bool init(int accountID);
    void loadData();
    void setupUI();
    
    void onTogglePlaying(CCObject* sender);
    void onToggleStreaming(CCObject* sender);
    void onToggleListening(CCObject* sender);
    void onToggleWatching(CCObject* sender);
    void onToggleCompeting(CCObject* sender);

    bool m_isUnlinking = false;
    geode::Notification* m_unlinkNotif = nullptr;
    int m_pollAttempts = 0;
    void pollUnlinkStatus();
    void pollUnlinkStatusCallback();
    void onUnlinkRequestFinished();
    
    void onUnlink(CCObject* sender);
    void updateSettings();
    std::string getFirebaseUrl();

public:
    static RpcSettingsPopup* create(int accountID);
};