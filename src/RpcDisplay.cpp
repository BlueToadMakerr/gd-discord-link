#include "RpcDisplay.hpp"
#include "RpcSettings.hpp"
#include <Geode/ui/GeodeUI.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <chrono>

using namespace geode::prelude;

namespace rpc_display {
    PriorityMenu* PriorityMenu::create() {
        auto ret = new PriorityMenu();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void PriorityMenu::registerWithTouchDispatcher() {
        CCDirector::get()->getTouchDispatcher()->addTargetedDelegate(this, -130, true); 
    }
    ccColor3B RpcPopup::hexToColor(const std::string& hex) {
        if (hex.length() < 6) return {255, 255, 255};
        std::string parse = hex[0] == '#' ? hex.substr(1) : hex;
        int r, g, b;
        if (sscanf(parse.c_str(), "%02x%02x%02x", &r, &g, &b) == 3) {
            return {(GLubyte)r, (GLubyte)g, (GLubyte)b};
        }
        return {255, 255, 255};
    }

    bool RpcPopup::init(int accountID, std::vector<matjson::Value> rpcArray, std::string initialData, int currentIndex, bool isMyProfile) {
        if (!Popup::init(320.f, 150.f)) return false;
        
        m_accountID = accountID;
        m_rpcArray = rpcArray;
        m_lastRpcData = initialData;
        m_currentIndex = currentIndex;
        m_size = m_mainLayer->getContentSize();

        if (isMyProfile) {
            auto settingsMenu = CCMenu::create();
            settingsMenu->setPosition({m_size.width - 3.0f, m_size.height - 3.0f});
            m_mainLayer->addChild(settingsMenu, 10);
            auto settingsSpr = CCSprite::createWithSpriteFrameName("GJ_optionsBtn_001.png");
            settingsSpr->setScale(0.75f);
            auto settingsBtn = CCMenuItemSpriteExtra::create(settingsSpr, this, menu_selector(RpcPopup::onSettingsClicked));
            settingsMenu->addChild(settingsBtn);
        }

        m_arrowMenu = CCMenu::create();
        m_arrowMenu->setPosition({0, 0});
        m_mainLayer->addChild(m_arrowMenu);

        auto leftSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        auto leftBtn = CCMenuItemSpriteExtra::create(leftSpr, this, menu_selector(RpcPopup::onPrev));
        leftBtn->setPosition({-20.f, m_size.height / 2});
        leftBtn->setScale(0.8f);
        m_arrowMenu->addChild(leftBtn);

        auto rightSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
        rightSpr->setFlipX(true);
        auto rightBtn = CCMenuItemSpriteExtra::create(rightSpr, this, menu_selector(RpcPopup::onNext));
        rightBtn->setPosition({m_size.width + 20.f, m_size.height / 2});
        rightBtn->setScale(0.8f);
        m_arrowMenu->addChild(rightBtn);

        m_arrowMenu->setVisible(m_rpcArray.size() > 1);

        loadRpc(m_currentIndex);
        this->schedule(schedule_selector(RpcPopup::pollData), 5.0f);
        return true;
    }

    void RpcPopup::loadRpc(int index) {
        if (m_contentNode) {
            m_contentNode->removeFromParent();
        }

        m_contentNode = CCNode::create();
        m_contentNode->setContentSize(m_size);
        m_contentNode->setPosition({0, 0});
        m_mainLayer->addChild(m_contentNode);
        m_data = m_rpcArray[index];
        m_start = 0;
        m_end = 0;
        m_timeLabel = nullptr;
        m_progressBarBg = nullptr;
        m_progressBarFg = nullptr;
        this->unscheduleUpdate(); 

        std::string name = m_data.contains("name") && m_data["name"].isString() ? m_data["name"].asString().unwrapOr("Unknown") : "Unknown";
        int type = m_data.contains("type") && m_data["type"].isNumber() ? m_data["type"].asInt().unwrapOr(0) : 0;
        std::string details = m_data.contains("details") && m_data["details"].isString() ? m_data["details"].asString().unwrapOr("") : "";
        std::string state = m_data.contains("state") && m_data["state"].isString() ? m_data["state"].asString().unwrapOr("") : "";
        std::string hexColor = m_data.contains("color") && m_data["color"].isString() ? m_data["color"].asString().unwrapOr("") : "";
        
        if (m_bgSprite) {
            m_bgSprite->setColor(hexToColor(hexColor));
        }
        
        matjson::Value assets;
        if (m_data.contains("assets") && m_data["assets"].isObject()) {
            assets = m_data["assets"];
        }

        std::string largeImage = assets.contains("large_image") && assets["large_image"].isString() ? assets["large_image"].asString().unwrapOr("") : "";
        std::string largeText = assets.contains("large_text") && assets["large_text"].isString() ? assets["large_text"].asString().unwrapOr("") : "";
        std::string smallImage = assets.contains("small_image") && assets["small_image"].isString() ? assets["small_image"].asString().unwrapOr("") : "";
        std::string smallText = assets.contains("small_text") && assets["small_text"].isString() ? assets["small_text"].asString().unwrapOr("") : "";

        if (m_data.contains("timestamps") && m_data["timestamps"].isObject()) {
            auto ts = m_data["timestamps"];
            m_start = ts.contains("start") && ts["start"].isNumber() ? ts["start"].asInt().unwrapOr(0) : 0;
            m_end = ts.contains("end") && ts["end"].isNumber() ? ts["end"].asInt().unwrapOr(0) : 0;
        }

        std::string prefix = "Playing";
        if (type == 1) prefix = "Streaming";
        else if (type == 2) prefix = "Listening to";
        else if (type == 3) prefix = "Watching";
        else if (type == 5) prefix = "Competing in";

        std::string topTitleStr = fmt::format("{} {}", prefix, name);

        auto topTitle = CCLabelBMFont::create(topTitleStr.c_str(), "bigFont.fnt");
        topTitle->setAnchorPoint({0.0f, 1.0f});
        topTitle->setScale(0.4f);
        topTitle->setPosition({20.f, m_size.height - 10.f});
        m_contentNode->addChild(topTitle);

        std::string line1Str, line2Str, line3Str;
        if (type == 0 || type == 1 || type == 5) {
            line1Str = name;
            line2Str = details;
            line3Str = state;
        } else if (type == 3) {
            line1Str = details;
            line2Str = state;
            line3Str = "";
        } else if (type == 2) {
            line1Str = details;
            line2Str = state;
            line3Str = largeText;
        }

        float textStartX = 110.f;
        float textStartY = m_size.height - 35.f;
        float maxTextWidth = m_size.width - textStartX - 55.f;

        auto textContainer = CCNode::create();
        textContainer->setContentSize({maxTextWidth, 75.f});
        textContainer->setAnchorPoint({0.0f, 1.0f});
        textContainer->setPosition({textStartX, textStartY});

        textContainer->setLayout(
            // OwO It's containers :3 using containers for those long song names for proprer text wrapping like the good boy I am >w<
            ColumnLayout::create()
                ->setAxisReverse(true)
                ->setAxisAlignment(AxisAlignment::End)
                ->setCrossAxisAlignment(AxisAlignment::Start)
                ->setCrossAxisLineAlignment(AxisAlignment::Start)
                ->setGap(4.f)
                ->setAutoScale(false)
        );

        auto addLabel = [&](const std::string& text, float scale) {
            if (text.empty()) return;
            auto label = CCLabelBMFont::create(text.c_str(), "chatFont.fnt", maxTextWidth / scale, kCCTextAlignmentLeft);
            label->setScale(scale);
            label->setAnchorPoint({0.0f, 0.5f});
            textContainer->addChild(label);
        };

        addLabel(line1Str, 0.85f);
        addLabel(line2Str, 0.6f);
        addLabel(line3Str, 0.6f);

        m_contentNode->addChild(textContainer);
        textContainer->updateLayout();

        auto imgMenu = CCMenu::create();
        imgMenu->setPosition({0, 0});
        m_contentNode->addChild(imgMenu);

        auto largeSpr = LazySprite::create({70.f, 70.f});
        largeSpr->setAutoResize(true);
        
        if (!largeImage.empty()) {
            largeSpr->setLoadCallback([largeSpr](Result<> res) {
                if (!res) {
                    auto fallback = CCSprite::create("unknown.png"_spr);
                    largeSpr->setTexture(fallback->getTexture());
                    largeSpr->setTextureRect(fallback->getTextureRect());
                    largeSpr->setScale(70.f / fallback->getContentSize().width);
                }
            });
            largeSpr->loadFromUrl(largeImage);
        } else {
            auto fallback = CCSprite::create("unknown.png"_spr);
            largeSpr->setTexture(fallback->getTexture());
            largeSpr->setTextureRect(fallback->getTextureRect());
            largeSpr->setScale(70.f / fallback->getContentSize().width);
        }

        auto largeBtn = CCMenuItemSpriteExtra::create(largeSpr, this, menu_selector(RpcPopup::onLargeImageClick));
        largeBtn->setPosition({60.f, m_size.height - 70.f});
        if (!largeText.empty()) {
            largeBtn->setUserObject("alphalaneous.tooltips/tooltip", CCString::create(largeText));
        }
        imgMenu->addChild(largeBtn);

        if (!smallImage.empty()) {
            auto smallSpr = LazySprite::create({24.f, 24.f});
            smallSpr->setAutoResize(true);

            smallSpr->setLoadCallback([smallSpr](Result<> res) {
                if (!res) {
                    auto fallback = CCSprite::create("unknown.png"_spr);
                    smallSpr->setTexture(fallback->getTexture());
                    smallSpr->setTextureRect(fallback->getTextureRect());
                    smallSpr->setScale(24.f / fallback->getContentSize().width);
                }
            });
            smallSpr->loadFromUrl(smallImage);

            auto smallBtn = CCMenuItemSpriteExtra::create(smallSpr, this, menu_selector(RpcPopup::onSmallImageClick));
            smallBtn->setPosition({largeBtn->getPositionX() + 25.f, largeBtn->getPositionY() - 25.f});
            if (!smallText.empty()) {
                smallBtn->setUserObject("alphalaneous.tooltips/tooltip", CCString::create(smallText));
            }

            auto smallMenu = PriorityMenu::create();
            smallMenu->setPosition({0, 0});
            smallMenu->addChild(smallBtn);
            m_contentNode->addChild(smallMenu);
        }

        if (m_start > 0 || m_end > 0) {
            setupTimestamps();
            this->scheduleUpdate(); 
        }
    }

    void RpcPopup::setupTimestamps() {
        float timeY = 25.f;
        float barScale = 0.3f;
        float barWidth = 260.f;
        float barHeight = 8.f;

        if (m_start > 0 && m_end > 0) {
            // The progress bar
            m_progressBarBg = CCScale9Sprite::create("square02b_001.png");
            m_progressBarBg->setColor({30, 30, 30});
            m_progressBarBg->setOpacity(150);
            m_progressBarBg->setContentSize({barWidth / barScale, barHeight / barScale});
            m_progressBarBg->setScale(barScale);
            m_progressBarBg->setPosition({m_size.width / 2, timeY});
            m_contentNode->addChild(m_progressBarBg);
            m_progressBarFg = CCScale9Sprite::create("square02b_001.png");
            m_progressBarFg->setColor({255, 255, 255});
            m_progressBarFg->setAnchorPoint({0.f, 0.5f});
            m_progressBarFg->setContentSize({0.f, barHeight / barScale});
            m_progressBarFg->setScale(barScale);
            m_progressBarFg->setPosition({m_size.width / 2.f - (barWidth / 2.f), timeY});
            m_contentNode->addChild(m_progressBarFg);
            m_timeLabel = CCLabelBMFont::create("OwO it's the progress bar", "chatFont.fnt");
        } else {
            m_timeLabel = CCLabelBMFont::create("Haii :3", "chatFont.fnt");
        }

        m_timeLabel->setScale(0.75f);
        m_timeLabel->setPosition({m_size.width / 2, timeY - 12.f});
        m_contentNode->addChild(m_timeLabel);
    }

    void RpcPopup::update(float dt) {
        long long now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        if (m_start > 0 && m_end > 0) {
            long long total = m_end - m_start;
            long long current = now - m_start;
            if (current < 0) current = 0;
            if (current > total) current = total;
            float percent = (total > 0) ? (float)current / (float)total : 0.0f;
            float barScale = 0.3f;
            float barWidth = 260.f;
            float targetWidth = (barWidth * percent) / barScale;

            if (targetWidth < 20.f) {
                m_progressBarFg->setVisible(false);
            } else {
                m_progressBarFg->setVisible(true);
                m_progressBarFg->setContentSize({targetWidth, 8.f / barScale});
            }

            m_timeLabel->setString(fmt::format("{} / {}", formatTime(current), formatTime(total)).c_str());
        } else if (m_start > 0) {
            long long elapsed = now - m_start;
            if (elapsed < 0) elapsed = 0;
            m_timeLabel->setString(fmt::format("Started {} ago", formatTime(elapsed)).c_str());

        } else if (m_end > 0) {
            long long left = m_end - now;
            if (left < 0) left = 0;
            m_timeLabel->setString(fmt::format("{} left", formatTime(left)).c_str());
        }
    }

    std::string RpcPopup::formatTime(long long seconds) {
        long long h = seconds / 3600;
        long long m = (seconds % 3600) / 60;
        long long s = seconds % 60;
        if (h > 0) {
            return fmt::format("{:02}:{:02}:{:02}", h, m, s);
        }
        return fmt::format("{:02}:{:02}", m, s);
    }

    void RpcPopup::pollData(float dt) {
        std::string baseUrl = Mod::get()->getSettingValue<std::string>("firebase-url");
        if (baseUrl.empty()) baseUrl = "https://discordgdlinker-default-rtdb.firebaseio.com";
        if (baseUrl.back() == '/') baseUrl.pop_back();
        std::string targetUrl = fmt::format("{}/users/{}.json", baseUrl, m_accountID);
        
        m_pollTask.spawn(
            web::WebRequest().get(targetUrl),
            [this](web::WebResponse response) {
                if (response.ok()) {
                    auto jsonRes = response.json();
                    if (jsonRes.isOk()) {
                        auto data = jsonRes.unwrap();
                        if (data.contains("rpc") && data["rpc"].isArray()) {
                            auto rpcNode = data["rpc"];
                            std::string newData = rpcNode.dump();
                            if (newData != m_lastRpcData) {
                                m_lastRpcData = newData;
                                m_rpcArray = rpcNode.asArray().unwrap();
                                if (m_currentIndex >= m_rpcArray.size()) {
                                    m_currentIndex = 0;
                                }
                                if (m_arrowMenu) {
                                    m_arrowMenu->setVisible(m_rpcArray.size() > 1);
                                }
                                if (!m_rpcArray.empty()) {
                                    this->loadRpc(m_currentIndex);
                                }
                            }
                        } else {
                            if (m_lastRpcData != "[]") {
                                m_lastRpcData = "[]";
                                this->onClose(nullptr);
                                FLAlertLayer::create("Discord Status", "This user has gone offline.. 3:", "OK")->show();
                            }
                        }
                    }
                }
            }
        );
    }

    void RpcPopup::onNext(CCObject*) {
        m_currentIndex = (m_currentIndex + 1) % m_rpcArray.size();
        loadRpc(m_currentIndex);
    }

    void RpcPopup::onPrev(CCObject*) {
        m_currentIndex = (m_currentIndex - 1 + m_rpcArray.size()) % m_rpcArray.size();
        loadRpc(m_currentIndex);
    }

    void RpcPopup::onLargeImageClick(CCObject*) {
        std::string text = "No details provided.";
        if (m_data.contains("assets") && m_data["assets"].contains("large_text")) {
            text = m_data["assets"]["large_text"].asString().unwrapOr(text);
        }
        FLAlertLayer::create("Large Image", text, "OK")->show();
    }

    void RpcPopup::onSmallImageClick(CCObject*) {
        std::string text = "No details provided.";
        if (m_data.contains("assets") && m_data["assets"].contains("small_text")) {
            text = m_data["assets"]["small_text"].asString().unwrapOr(text);
        }
        FLAlertLayer::create("Small Image", text, "OK")->show();
    }

    void RpcPopup::onSettingsClicked(CCObject* sender) {
        RpcSettingsPopup::create(m_accountID)->show();
    }

    RpcPopup* RpcPopup::create(int accountID, std::vector<matjson::Value> rpcArray, std::string initialData, int currentIndex, bool isMyProfile) {
        auto ret = new RpcPopup();
        if (ret && ret->init(accountID, rpcArray, initialData, currentIndex, isMyProfile)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }

    void showRpcDetailsPopup(int accountID, bool isLinked, bool hasRpc, bool isMyProfile) {
        if(isLoadingRpc) return;
        isLoadingRpc = true;
        if (!hasRpc) {
            FLAlertLayer::create("Discord Status", "This user is offline..", "OK")->show();
            isLoadingRpc = false;
            return;
        }
        std::string baseUrl = Mod::get()->getSettingValue<std::string>("firebase-url");
        if (baseUrl.empty()) baseUrl = "https://discordgdlinker-default-rtdb.firebaseio.com";
        if (baseUrl.back() == '/') baseUrl.pop_back();
        std::string targetUrl = fmt::format("{}/users/{}.json", baseUrl, accountID);
        geode::async::spawn(
            web::WebRequest().get(targetUrl),
            [accountID, isMyProfile](web::WebResponse response) {
                if (response.ok()) {
                    auto jsonRes = response.json();
                    if (jsonRes.isOk()) {
                        auto data = jsonRes.unwrap();
                        if (data.contains("rpc") && data["rpc"].isArray()) {
                            auto rpcNode = data["rpc"];
                            auto rpcArray = rpcNode.asArray().unwrap();
                            if (!rpcArray.empty()) {
                                RpcPopup::create(accountID, rpcArray, rpcNode.dump(), 0, isMyProfile)->show();
                                isLoadingRpc = false;
                                return;
                            }
                        }
                    }
                }
                FLAlertLayer::create("Error", "Failed to get RPC data... this user might be offline..", "OK")->show();
                isLoadingRpc = false;
            }
        );
    }
}