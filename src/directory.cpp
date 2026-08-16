#include "directory.hpp"
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/ui/LazySprite.hpp>

std::string DirectoryPopup::getFirebaseUrl() {
    std::string baseUrl = Mod::get()->getSettingValue<std::string>("firebase-url");
    if (baseUrl.empty()) baseUrl = "https://discordgdlinker-default-rtdb.firebaseio.com";
    if (baseUrl.back() == '/') baseUrl.pop_back();
    return baseUrl;
}

DirectoryPopup* DirectoryPopup::create() {
    auto ret = new DirectoryPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool DirectoryPopup::init() {
    if (!Popup::init(360.f, 240.f)) return false;
    this->setTitle("Online Players");
    m_scrollLayer = ScrollLayer::create({320.f, 180.f});
    m_scrollLayer->setPosition(m_mainLayer->getContentSize() / 2 - m_scrollLayer->getContentSize() / 2);
    m_mainLayer->addChild(m_scrollLayer);
    fetchDirectory();
    return true;
}

void DirectoryPopup::fetchDirectory() {
    std::string url = fmt::format("{}/directory.json", getFirebaseUrl());
    m_fetchTask.spawn(
        web::WebRequest().get(url),
        [this](web::WebResponse response) {
            if (!response.ok()) {
                this->showError("Failed to get Directory");
                return;
            }
            auto jsonRes = response.json();
            if (!jsonRes.isOk()) {
                this->showError("Server retuned invalid data!");
                return;
            }
            auto data = jsonRes.unwrap();
            if (!data.isObject()) {
                this->showError("Nobody is online...");
                return;
            }
            this->populateList(data);
        }
    );
}

void DirectoryPopup::showError(const std::string& message) {
    auto label = CCLabelBMFont::create(message.c_str(), "bigFont.fnt");
    label->setScale(0.6f);
    label->setPosition(m_mainLayer->getContentSize() / 2);
    m_mainLayer->addChild(label);
}

void DirectoryPopup::populateList(const matjson::Value& data) {
    auto contentNode = m_scrollLayer->m_contentLayer;
    contentNode->removeAllChildren();

    float cellHeight = 50.f;
    int count = 0;
    for (auto const& [key, val] : data) {
        count++;
    }

    float totalHeight = std::max(180.f, count * cellHeight);
    contentNode->setContentSize({320.f, totalHeight});

    float yOffset = totalHeight - cellHeight;

    static std::unordered_map<int, std::unordered_map<int, std::string>> s_userCache;

    for (auto const& [key, val] : data) {
        std::string keyStr(key);
        int accountID = 0;
        try {
            accountID = std::stoi(keyStr);
        } catch (...) {}

        std::string imageUrl = "";
        if (val.contains("image_url")) {
            imageUrl = val["image_url"].asString().unwrapOr("");
        }

        std::string rpcName = "No RPC Data";
        if (val.contains("rpc_name")) {
            rpcName = val["rpc_name"].asString().unwrapOr("No RPC Data");
        }

        if (val.contains("extra")) {
            rpcName += fmt::format(" + {}", val["extra"].asString().unwrapOr("more"));
        }

        auto cell = CCNode::create();
        cell->setContentSize({320.f, cellHeight});
        cell->setPosition({0.f, yOffset});

        auto playerIcon = SimplePlayer::create(1);
        playerIcon->setScale(0.85f);
        playerIcon->setPosition({20.f, cellHeight / 2});
        cell->addChild(playerIcon);

        auto discordImg = LazySprite::create({18.f, 18.f});
        discordImg->setAutoResize(true);
        discordImg->setLoadCallback([discordImg](Result<> res) {
            if (!res) {
                auto fallback = CCSprite::create("unknown.png"_spr);
                discordImg->setTexture(fallback->getTexture());
                discordImg->setTextureRect(fallback->getTextureRect());
                discordImg->setScale(15.f / fallback->getContentSize().width);
            }
        });
        discordImg->loadFromUrl(imageUrl);
        discordImg->setPosition({playerIcon->getPositionX() + 8.f, playerIcon->getPositionY() - 8.f});
        cell->addChild(discordImg, 5);

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        cell->addChild(menu);

        auto usernameLabel = CCLabelBMFont::create(fmt::format("User {}", accountID).c_str(), "bigFont.fnt");
        usernameLabel->setScale(0.5f);
        
        auto usernameBtn = CCMenuItemSpriteExtra::create(
            usernameLabel, this, menu_selector(DirectoryPopup::onUserClick)
        );
        usernameBtn->setTag(accountID);
        usernameBtn->setPosition({50.f + usernameLabel->getScaledContentSize().width / 2, cellHeight / 2 + 8.f});
        menu->addChild(usernameBtn);

        auto rpcLabel = CCLabelBMFont::create(rpcName.c_str(), "chatFont.fnt");
        rpcLabel->setScale(0.8f);
        rpcLabel->setAnchorPoint({0.f, 0.5f});
        rpcLabel->setPosition({50.f, cellHeight / 2 - 10.f});
        cell->addChild(rpcLabel);

        Ref<SimplePlayer> safePlayerIcon(playerIcon);
        Ref<CCLabelBMFont> safeUsernameLabel(usernameLabel);
        Ref<CCMenuItemSpriteExtra> safeUsernameBtn(usernameBtn);

        auto applyUserData = [safePlayerIcon, safeUsernameLabel, safeUsernameBtn](const std::unordered_map<int, std::string>& mapped) {
            if (!safePlayerIcon || !safeUsernameLabel || !safeUsernameBtn) return;

            if (mapped.count(1)) {
                safeUsernameLabel->setString(mapped.at(1).c_str());
                safeUsernameBtn->setPosition({50.f + safeUsernameLabel->getScaledContentSize().width / 2, safeUsernameBtn->getPositionY()});
            }

            int iconTypeVal = mapped.count(14) ? std::stoi(mapped.at(14)) : 0;
            int iconIDVal = mapped.count(9) ? std::stoi(mapped.at(9)) : 1;

            IconType iconType = IconType::Cube;
            switch (iconTypeVal) {
                case 0: iconType = IconType::Cube; break;
                case 1: iconType = IconType::Ship; break;
                case 2: iconType = IconType::Ball; break;
                case 3: iconType = IconType::Ufo; break;
                case 4: iconType = IconType::Wave; break;
                case 5: iconType = IconType::Robot; break;
                case 6: iconType = IconType::Spider; break;
                case 7: iconType = IconType::Swing; break;
                case 8: iconType = IconType::Jetpack; break;
                default: iconType = IconType::Cube; break;
            }

            safePlayerIcon->updatePlayerFrame(iconIDVal, iconType);
            safePlayerIcon->setColor(GameManager::get()->colorForIdx(std::stoi(mapped.at(10))));
            safePlayerIcon->setSecondColor(GameManager::get()->colorForIdx(std::stoi(mapped.at(11))));
            if (mapped.at(15) == "2") safePlayerIcon->setGlowOutline(GameManager::get()->colorForIdx(std::stoi(mapped.at(51))));
            safePlayerIcon->updateColors();
        };

        if (s_userCache.contains(accountID)) {
            applyUserData(s_userCache[accountID]);
        } else {
            // Get the Player ID 
            std::string userInfoPost = fmt::format("gameVersion=22&binaryVersion=47&targetAccountID={}&secret=Wmfd2893gb7", accountID);
            geode::async::spawn(
                web::WebRequest().bodyString(userInfoPost).post("https://www.boomlings.com/database/getGJUserInfo20.php"),
                [accountID, applyUserData](web::WebResponse response) {
                    if (!response.ok()) return;
                    auto res = response.string().unwrapOr("");
                    
                    std::unordered_map<int, std::string> mappedInfo;
                    std::stringstream ss(res);
                    std::string keyStr, valStr;
                    while (std::getline(ss, keyStr, ':') && std::getline(ss, valStr, ':')) {
                        try {
                            mappedInfo[std::stoi(keyStr)] = valStr;
                        } catch (...) {}
                    }

                    if (!mappedInfo.count(2)) return;
                    std::string playerID = mappedInfo[2];

                    std::string usersPost = fmt::format("gameVersion=22&binaryVersion=47&str={}&secret=Wmfd2893gb7", playerID);
                    geode::async::spawn(
                        web::WebRequest().bodyString(usersPost).post("https://www.boomlings.com/database/getGJUsers20.php"),
                        [accountID, applyUserData](web::WebResponse usersResponse) {
                            if (usersResponse.ok()) {
                                auto res = usersResponse.string().unwrapOr("");
                                
                                auto hashPos = res.find('#');
                                if (hashPos != std::string::npos) {
                                    res = res.substr(0, hashPos);
                                }

                                std::unordered_map<int, std::string> mapped;
                                std::stringstream ss(res);
                                std::string keyStr, valStr;
                                
                                while (std::getline(ss, keyStr, ':') && std::getline(ss, valStr, ':')) {
                                    try {
                                        mapped[std::stoi(keyStr)] = valStr;
                                    } catch (...) {}
                                }

                                if (!mapped.empty()) {
                                    s_userCache[accountID] = mapped;
                                    applyUserData(mapped);
                                }
                            }
                        }
                    );
                }
            );
        }

        contentNode->addChild(cell);
        yOffset -= cellHeight;
    }

    m_scrollLayer->moveToTop();
}

void DirectoryPopup::onUserClick(CCObject* sender) {
    int accountID = sender->getTag();
    if (accountID > 0) {
        bool isMyProfile = (accountID == GJAccountManager::get()->m_accountID);
        ProfilePage::create(accountID, isMyProfile)->show();
    }
}