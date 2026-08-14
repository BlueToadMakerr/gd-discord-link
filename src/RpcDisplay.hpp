#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>

using namespace geode::prelude;

namespace rpc_display {

    class PriorityMenu : public CCMenu {
    public:
        static PriorityMenu* create();
        void registerWithTouchDispatcher() override;
    };

    class RpcPopup : public geode::Popup {
    protected:
        std::vector<matjson::Value> m_rpcArray;
        int m_currentIndex = 0;
        int m_accountID = 0;
        std::string m_lastRpcData = "";
        geode::async::TaskHolder<web::WebResponse> m_pollTask;
        matjson::Value m_data;
        CCSize m_size;
        CCNode* m_contentNode = nullptr;
        long long m_start = 0;
        long long m_end = 0;
        CCLabelBMFont* m_timeLabel = nullptr;
        CCScale9Sprite* m_progressBarBg = nullptr;
        CCScale9Sprite* m_progressBarFg = nullptr;
        CCMenu* m_arrowMenu = nullptr;

        ccColor3B hexToColor(const std::string& hex);
        bool init(int accountID, std::vector<matjson::Value> rpcArray, std::string initialData, int currentIndex, bool isMyProfile);
        void loadRpc(int index);
        void setupTimestamps();
        void update(float dt) override;
        std::string formatTime(long long seconds);
        void pollData(float dt);

        void onNext(CCObject*);
        void onPrev(CCObject*);
        void onLargeImageClick(CCObject*);
        void onSmallImageClick(CCObject*);
        void onSettingsClicked(CCObject* sender);

    public:
        static RpcPopup* create(int accountID, std::vector<matjson::Value> rpcArray, std::string initialData, int currentIndex = 0, bool isMyProfile = false);
    };

    inline bool isLoadingRpc = false;
    void showRpcDetailsPopup(int accountID, bool isLinked, bool hasRpc, bool isMyProfile);
}