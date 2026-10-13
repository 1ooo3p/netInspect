#include <Geode/Geode.hpp>

#ifdef GEODE_IS_ANDROID
#define STRING_LENGTH(s) s.size()
#else
#define STRING_LENGTH(s) s.length()
#endif

using namespace geode::prelude;

inline gd::string charVectorToString(gd::vector<char> charVector) {
#ifdef GEODE_IS_ANDROID
    std::string str(charVector.begin(), charVector.end());
    gd::string gdStr(str.c_str());
#else
    std::string gdStr(charVector.begin(), charVector.end());
#endif
    return gdStr;
}
inline gd::vector<char> stringToCharVector(gd::string str) {
#ifdef GEODE_IS_ANDROID
    gd::vector<char> charVector(str.c_str(), str.c_str()+str.size());
#else
    gd::vector<char> charVector(str.begin(), str.end());
#endif
    return charVector;
}

// replacement for CCHttpRequest (somehow the pointer became a CCObject*)
// p.s. i didnt know dynamic_cast at that time lol
struct MyRequestObject {
    CCHttpRequest::HttpRequestType requestType;
    gd::string url;
    gd::vector<gd::string> headers;
    gd::string body;
    gd::string tag;
    CCObject *targetObj;
    SEL_HttpResponse callback;
    void *userData;
    int type;
};

MyRequestObject toMyRequestObject(CCHttpRequest *request) {
    return {
        request->getRequestType(),
        request->getUrl(),
        request->getHeaders(),
        request->getRequestData(),
        request->getTag(),
        request->getTarget(),
        request->getSelector(),
        request->getUserData(),
        request->getType()
    };
}
CCHttpRequest *toCCHttpRequestPtr(MyRequestObject request) {
    auto *ccRequest = new CCHttpRequest();
    ccRequest->setRequestType(request.requestType);
    ccRequest->setUrl(request.url.c_str());
    ccRequest->setHeaders(request.headers);
    ccRequest->setRequestData(request.body.c_str(), request.body.size());
    ccRequest->setTag(request.tag.c_str());
    ccRequest->setResponseCallback(request.targetObj, request.callback);
    ccRequest->setUserData(request.userData);
    ccRequest->setType(request.type);
    return ccRequest;
}

// also one for CCHttpResponse
struct MyResponseObject {
    MyRequestObject request;
    bool succeed;
    gd::string body;
    gd::string header;
    int statusCode;
    gd::string errorBuffer;
};

MyResponseObject toMyResponseObject(CCHttpResponse *response) {
    return {
        toMyRequestObject(response->getHttpRequest()),
        response->isSucceed(),
        charVectorToString(*(response->getResponseData())),
        charVectorToString(*(response->getResponseHeader())),
        response->getResponseCode(),
        response->getErrorBuffer()
    };
}
CCHttpResponse* toCCHttpResponsePtr(MyResponseObject response) {
    auto respBody = stringToCharVector(response.body);
    auto respHeader = stringToCharVector(response.header);
    auto ccRequest = toCCHttpRequestPtr(response.request);
    auto ccResponse = new CCHttpResponse(ccRequest);
    ccResponse->setSucceed(response.succeed);
    ccResponse->setResponseData(&respBody);
    ccResponse->setResponseHeader(&respHeader);
    ccResponse->setResponseCode(response.statusCode);
    ccResponse->setErrorBuffer(response.errorBuffer.c_str());
    return ccResponse;
}

void logRequest(CCHttpRequest *request) {
    log::info("new request: {}", request->getUrl());
    switch (request->getRequestType()) {
        case CCHttpRequest::HttpRequestType::kHttpGet: log::info("request type: get"); break;
        case CCHttpRequest::HttpRequestType::kHttpPost: log::info("request type: post"); break;
        case CCHttpRequest::HttpRequestType::kHttpPut: log::info("request type: put"); break;
        case CCHttpRequest::HttpRequestType::kHttpDelete: log::info("request type: delete"); break;
        default: break;
    }
}

bool captureMode = false;
bool sentFromPopup = false;
MyRequestObject theCurrentRequest;
MyResponseObject theCurrentResponse;

class ResponseInspectPopup : public geode::Popup {
    MyResponseObject m_currentResponse;
    CCMenu *m_inspectorMenu;
    gd::string m_originalRespBody;
    TextInput *m_bodyField;

protected:
    bool init(MyRequestObject request, MyResponseObject response) {
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        if (!Popup::init(winSize.width * 0.9f, std::min(winSize.width * 0.9f, winSize.height * 0.8f)))
            return false;

        m_currentResponse = response;
        m_originalRespBody = response.body;
        this->setTitle("Response Inspector");
        
        createOkButton();

        auto inspectorLayout = ColumnLayout::create();
        inspectorLayout->setGap(m_buttonMenu->getContentSize().height * 0.05);
        inspectorLayout->setAxisReverse(true);
        
        m_inspectorMenu = CCMenu::create();
        m_inspectorMenu->setContentSize(m_buttonMenu->getContentSize());
        m_inspectorMenu->setLayout(inspectorLayout);
        m_mainLayer->addChildAtPosition(m_inspectorMenu, Anchor::Center);

        createStatusCodeField();
        createBodyField();

        m_inspectorMenu->updateLayout();

        return true;
    }

    void createStatusCodeField() {
        auto statusCodeMenu = CCMenu::create();
        statusCodeMenu->setContentWidth(m_inspectorMenu->getContentWidth());
        statusCodeMenu->setLayout(RowLayout::create());

        auto succeedLabel = CCLabelBMFont::create("Succeed:", "bigFont.fnt");
        succeedLabel->setScale(0.6);

        auto succeedCheck = CCMenuItemToggler::createWithStandardSprites(
            this, menu_selector(ResponseInspectPopup::toggleSucceed), 1
        );
        succeedCheck->toggle(m_currentResponse.succeed);

        auto statusCodeLabel = CCLabelBMFont::create("Status code:", "bigFont.fnt");
        statusCodeLabel->setScale(0.6);

        auto statusCodeField = TextInput::create(100, "200");
        statusCodeField->setFilter("0123456789");
        statusCodeField->setMaxCharCount(3);

        statusCodeField->setCallback([this, statusCodeField](gd::string newCodeStr){
            auto newCode = utils::numFromString<int>(newCodeStr);
            m_currentResponse.statusCode = newCode ? newCode.unwrap() : 200;
        });
        statusCodeField->setString(std::to_string(m_currentResponse.statusCode));

        statusCodeMenu->addChild(succeedLabel);
        statusCodeMenu->addChild(succeedCheck);
        statusCodeMenu->addChild(statusCodeLabel);
        statusCodeMenu->addChild(statusCodeField);
        statusCodeMenu->updateLayout();
        m_inspectorMenu->addChild(statusCodeMenu);

    }

    void createBodyField() {
        auto bodyTopMenu = CCMenu::create();
        bodyTopMenu->setContentWidth(m_inspectorMenu->getContentWidth());
        bodyTopMenu->setLayout(RowLayout::create());

        auto bodyLabel = CCLabelBMFont::create("Body:", "bigFont.fnt");
        bodyLabel->setScale(0.6);

        auto copyBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Copy", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(ResponseInspectPopup::copyBody)
        );

        auto pasteBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Paste", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(ResponseInspectPopup::pasteBody)
        );
        
        auto resetBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Reset", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(ResponseInspectPopup::resetBody)
        );

        m_bodyField = TextInput::create(m_inspectorMenu->getContentWidth() - 20.f, "", "chatFont.fnt");
        if (STRING_LENGTH(m_originalRespBody) > 300) {
            m_bodyField->setPlaceholder(
                fmt::format("Too much data, use the buttons ({} bytes)", STRING_LENGTH(m_originalRespBody))
            );
            m_bodyField->setEnabled(false);
        } else {
            m_bodyField->setCallback([this](gd::string newBody) {
                m_currentResponse.body = newBody;
            });
            m_bodyField->setString(m_currentResponse.body);
            m_bodyField->setCommonFilter(CommonFilter::Any);
        }
        
        bodyTopMenu->addChild(bodyLabel);
        bodyTopMenu->addChild(copyBodyButton);
        bodyTopMenu->addChild(pasteBodyButton);
        bodyTopMenu->addChild(resetBodyButton);
        bodyTopMenu->updateLayout();
        m_inspectorMenu->addChild(bodyTopMenu);
        m_inspectorMenu->addChild(m_bodyField);
    }

    void copyBody(CCObject*) {
        clipboard::write(m_currentResponse.body);
    }

    void pasteBody(CCObject*) {
        m_currentResponse.body = clipboard::read();
        if (STRING_LENGTH(m_originalRespBody) <= 300) m_bodyField->setString(m_currentResponse.body);
    }

    void resetBody(CCObject*) {
        m_currentResponse.body = m_originalRespBody;
        if (STRING_LENGTH(m_originalRespBody) <= 300) m_bodyField->setString(m_currentResponse.body);
    }

    void createOkButton() {
        m_closeBtn->setVisible(false);

        auto okBtn = CCMenuItemSpriteExtra::create(
            ButtonSprite::create("OK"),
            this,
            menu_selector(ResponseInspectPopup::onOk)
        );

        okBtn->setPosition(
            m_buttonMenu->getContentSize().width / 2.0,
            25
        );

        m_buttonMenu->addChild(okBtn);
    }

    void toggleSucceed(CCObject* sender) {
        auto checkbox = static_cast<CCMenuItemToggler*>(sender);
        m_currentResponse.succeed = checkbox->isToggled();
    }

    void onOk(CCObject*) {
        theCurrentResponse = m_currentResponse;
        CCDirector::sharedDirector()->getRunningScene()->runAction(
            CCCallFunc::create(this, callfunc_selector(ResponseInspectPopup::onPostClose))
        );
        this->onClose(nullptr);
    }

    void onPostClose() {
        SEL_HttpResponse responseCallback = theCurrentRequest.callback;
        CCObject* targetObj = theCurrentRequest.targetObj;

        auto ccResponse = toCCHttpResponsePtr(theCurrentResponse);
        (targetObj->*responseCallback)(CCHttpClient::getInstance(), ccResponse);
    }

public:
    static ResponseInspectPopup *create(MyRequestObject req, MyResponseObject resp) {
        auto ret = new ResponseInspectPopup();
        if (ret->init(req, resp)) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }
};

class RequestInspectPopup : public geode::Popup {
    MyRequestObject m_currentRequest;
    gd::string m_originalReqBody;
    CCMenu *m_inspectorMenu;
    CCMenuItemSpriteExtra *m_getMethodButton; // me when idk that CCObject tags exists
    CCMenuItemSpriteExtra *m_postMethodButton;
    CCMenuItemSpriteExtra *m_putMethodButton;
    CCMenuItemSpriteExtra *m_deleteMethodButton;
    TextInput *m_bodyField;

protected:
    bool init(MyRequestObject request) {
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        if (!Popup::init(winSize.width * 0.9f, std::min(winSize.width * 0.9f, winSize.height * 0.9f)))
            return false;

        m_currentRequest = request;
        m_originalReqBody = request.body;
        this->setTitle("Request Inspector");

        this->createOkButton();

        auto inspectorLayout = ColumnLayout::create();
        inspectorLayout->setGap(m_buttonMenu->getContentSize().height * 0.05);
        inspectorLayout->setAxisReverse(true);
        
        m_inspectorMenu = CCMenu::create();
        m_inspectorMenu->setContentSize(m_buttonMenu->getContentSize());
        m_inspectorMenu->setLayout(inspectorLayout);
        m_mainLayer->addChildAtPosition(m_inspectorMenu, Anchor::Center);
        
        this->createUrlField();
        this->createMethodField();
        this->createBodyField();

        m_inspectorMenu->updateLayout();

        return true;
    }

    void createUrlField() {
        auto urlMenu = CCMenu::create();
        urlMenu->setContentWidth(m_inspectorMenu->getContentWidth());
        urlMenu->setLayout(RowLayout::create());
        urlMenu->setID("url-menu");
        
        auto urlLabel = CCLabelBMFont::create("url:", "bigFont.fnt");
        urlLabel->setScale(0.6);

        float urlFieldWidth = urlMenu->getContentWidth() - urlLabel->getContentWidth()*2;
        
        auto urlField = TextInput::create(urlFieldWidth, "enter url here...");
        urlField->setCallback([this](gd::string newUrl) {
            m_currentRequest.url = newUrl;
        });
        urlField->setTextAlign(TextInputAlign::Left);
        urlField->setString(m_currentRequest.url);
        
        m_inspectorMenu->addChild(urlMenu);
        urlMenu->addChild(urlLabel);
        urlMenu->addChild(urlField);
        urlMenu->updateLayout();
    }

    void createMethodField() {
        auto methodMenu = CCMenu::create();
        methodMenu->setContentWidth(m_inspectorMenu->getContentWidth());
        methodMenu->setLayout(RowLayout::create());
        methodMenu->setID("method-menu");

        auto methodLabel = CCLabelBMFont::create("Method:", "bigFont.fnt");
        methodLabel->setScale(0.6);

        m_getMethodButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Get", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::setHttpMethod)
        );
        m_postMethodButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Post", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::setHttpMethod)
        );
        m_putMethodButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Put", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::setHttpMethod)
        );
        m_deleteMethodButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Delete", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::setHttpMethod)
        );

        this->updateMethodButtons(m_currentRequest.requestType);
        methodMenu->addChild(m_getMethodButton);
        methodMenu->addChild(m_postMethodButton);
        methodMenu->addChild(m_putMethodButton);
        methodMenu->addChild(m_deleteMethodButton);
        methodMenu->updateLayout();
        m_inspectorMenu->addChild(methodMenu);

    }

    void createBodyField() {
        auto bodyTopMenu = CCMenu::create();
        bodyTopMenu->setContentWidth(m_inspectorMenu->getContentWidth());
        bodyTopMenu->setLayout(RowLayout::create());

        auto bodyLabel = CCLabelBMFont::create("Body:", "bigFont.fnt");
        bodyLabel->setScale(0.6);

        auto copyBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Copy", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::copyBody)
        );

        auto pasteBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Paste", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::pasteBody)
        );
        
        auto resetBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "Reset", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::resetBody)
        );

        m_bodyField = TextInput::create(m_inspectorMenu->getContentWidth() - 20.f, "", "chatFont.fnt");
        if (STRING_LENGTH(m_originalReqBody) > 300) {
            m_bodyField->setPlaceholder(
                fmt::format("Too much data, use the buttons ({} bytes)", STRING_LENGTH(m_originalReqBody))
            );
            m_bodyField->setEnabled(false);
        } else {
            m_bodyField->setCallback([this](gd::string newBody) {
                m_currentRequest.body = newBody;
            });
            m_bodyField->setString(m_currentRequest.body);
            m_bodyField->setCommonFilter(CommonFilter::Any);
        }
        
        bodyTopMenu->addChild(bodyLabel);
        bodyTopMenu->addChild(copyBodyButton);
        bodyTopMenu->addChild(pasteBodyButton);
        bodyTopMenu->addChild(resetBodyButton);
        bodyTopMenu->updateLayout();
        m_inspectorMenu->addChild(bodyTopMenu);
        m_inspectorMenu->addChild(m_bodyField);
    }

    void setHttpMethod(CCObject* triggeredButton) {
        if (triggeredButton == m_getMethodButton) {
            m_currentRequest.requestType = CCHttpRequest::HttpRequestType::kHttpGet;
        } else if (triggeredButton == m_postMethodButton) {
            m_currentRequest.requestType = CCHttpRequest::HttpRequestType::kHttpPost;
        } else if (triggeredButton == m_putMethodButton) {
            m_currentRequest.requestType = CCHttpRequest::HttpRequestType::kHttpPut;
        } else {
            m_currentRequest.requestType = CCHttpRequest::HttpRequestType::kHttpDelete;
        }
        this->updateMethodButtons(m_currentRequest.requestType);
    }

    void updateMethodButtons(CCHttpRequest::HttpRequestType method) {
        auto getBtnSpr = static_cast<ButtonSprite*>(m_getMethodButton->getNormalImage());
        auto postBtnSpr = static_cast<ButtonSprite*>(m_postMethodButton->getNormalImage());
        auto putBtnSpr = static_cast<ButtonSprite*>(m_putMethodButton->getNormalImage());
        auto deleteBtnSpr = static_cast<ButtonSprite*>(m_deleteMethodButton->getNormalImage());
        
        if (method == CCHttpRequest::HttpRequestType::kHttpGet)
            getBtnSpr->updateBGImage("GJ_button_01.png");
        else getBtnSpr->updateBGImage("GJ_button_04.png");
        
        if (method == CCHttpRequest::HttpRequestType::kHttpPost)
            postBtnSpr->updateBGImage("GJ_button_01.png");
        else postBtnSpr->updateBGImage("GJ_button_04.png");
        
        if (method == CCHttpRequest::HttpRequestType::kHttpPut)
            putBtnSpr->updateBGImage("GJ_button_01.png");
        else putBtnSpr->updateBGImage("GJ_button_04.png");
        
        if (method == CCHttpRequest::HttpRequestType::kHttpDelete)
            deleteBtnSpr->updateBGImage("GJ_button_01.png");
        else deleteBtnSpr->updateBGImage("GJ_button_04.png");
    }

    void copyBody(CCObject*) {
        clipboard::write(m_currentRequest.body);
    }

    void pasteBody(CCObject*) {
        m_currentRequest.body = clipboard::read();
        if (STRING_LENGTH(m_originalReqBody) <= 300) m_bodyField->setString(m_currentRequest.body);
    }

    void resetBody(CCObject*) {
        m_currentRequest.body = m_originalReqBody;
        if (STRING_LENGTH(m_originalReqBody) <= 300) m_bodyField->setString(m_currentRequest.body);
    }

    void createOkButton() {
        m_closeBtn->setVisible(false);

        auto okSpr = ButtonSprite::create("OK");
        auto okBtn = CCMenuItemSpriteExtra::create(
            okSpr,
            this,
            menu_selector(RequestInspectPopup::onOk));

        m_buttonMenu->addChild(okBtn);

        okBtn->setPosition(
            m_buttonMenu->getContentSize().width / 2.0,
            25
        );
    }

    void onOk(CCObject*) {
        sentFromPopup = true;
        theCurrentRequest = m_currentRequest;
        auto wrapperRequest = toCCHttpRequestPtr(theCurrentRequest);
        wrapperRequest->setResponseCallback(this, httpresponse_selector(RequestInspectPopup::onWrapperRequest));

        CCHttpClient::getInstance()->send(wrapperRequest);
        wrapperRequest->release();
        this->onClose(nullptr);
    }

    void onWrapperRequest(CCHttpClient *client, CCHttpResponse *response) {
        theCurrentResponse = toMyResponseObject(response);
        CCDirector::sharedDirector()->getRunningScene()->runAction(
            CCCallFunc::create(this, callfunc_selector(RequestInspectPopup::showResponsePopup))
        );
    }

    void showResponsePopup() {
        ResponseInspectPopup::create(theCurrentRequest, theCurrentResponse)->show();
    }

public:
    static RequestInspectPopup *create(MyRequestObject req) {
        auto ret = new RequestInspectPopup();
        if (ret->init(req)) {
            ret->autorelease();
            return ret;
        }

        delete ret;
        return nullptr;
    }
};

$on_game(Loaded) {
    listenForKeybindSettingPresses("inspector-keybind", [](Keybind const &keybind, bool down, bool repeat, double timestamp) {
        if (!repeat) captureMode = down;
    });
}

#include <Geode/modify/CCHttpClient.hpp>
class $modify(MyCCHttpClient, CCHttpClient) {
    void send(cocos2d::extension::CCHttpRequest *request) {
        auto inspectorKeybinds = \
            Mod::get()->getSettingValue<std::vector<geode::Keybind>>("inspector-keybind");

        if (!sentFromPopup && (captureMode || inspectorKeybinds.size() == 0)) {
            logRequest(request);
            theCurrentRequest = toMyRequestObject(request);
            
            OverlayManager::get()->scheduleOnce(
                schedule_selector(MyCCHttpClient::delayedShowRequestPopup), 0.1
            );

            return;
        }

        if (!sentFromPopup) {
            logRequest(request);
        }
        
        sentFromPopup = false;
        CCHttpClient::send(request);
    }

    void delayedShowRequestPopup() {
        auto currentTransition = dynamic_cast<CCTransitionScene*>(
            CCDirector::sharedDirector()->getRunningScene()
        );

        if (currentTransition != nullptr) {
            CCDirector::sharedDirector()->replaceScene(currentTransition->m_pInScene);

            OverlayManager::get()->scheduleOnce(
                schedule_selector(MyCCHttpClient::showRequestPopup), 0.1
            );

            return;
        }

        showRequestPopup();
    }

    void showRequestPopup() {
        RequestInspectPopup::create(theCurrentRequest)->show();
    }
};
