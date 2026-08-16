#pragma once
#include <Geode/Geode.hpp>
#include <Geode/utils/web.hpp>
#include <Geode/utils/async.hpp>
#include <Geode/ui/ScrollLayer.hpp>

using namespace geode::prelude;

class DirectoryPopup : public geode::Popup {
protected:
    ScrollLayer* m_scrollLayer = nullptr;
    geode::async::TaskHolder<web::WebResponse> m_fetchTask;

    bool init(); 
    void fetchDirectory();
    void populateList(const matjson::Value& data);
    void showError(const std::string& message);
    void onUserClick(CCObject* sender);
    
    std::string getFirebaseUrl();

public:
    static DirectoryPopup* create();
};