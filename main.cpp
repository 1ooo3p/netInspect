#include <Geode/Geode.hpp>

using namespace geode::prelude;

inline std::string charVectorToString(std::vector<char> charVector) {
    std::string str(charVector.begin(), charVector.end());
    return str;
}
inline std::vector<char> stringToCharVector(std::string str) {
    std::vector<char> charVector(str.begin(), str.end());
    return charVector;
}

// replacement for CCHttpRequest (somehow the pointer became a CCObject*)
struct MyRequestObject {
    CCHttpRequest::HttpRequestType requestType;
    std::string url;
    std::vector<std::string> headers;
    std::string body;
    std::string tag;
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
CCHttpRequest *toCCHttpRequestPtr(MyRequestObject *request) {
    auto *ccRequest = new CCHttpRequest();
    ccRequest->setRequestType(request->requestType);
    ccRequest->setUrl(request->url.c_str());
    ccRequest->setHeaders(request->headers);
    ccRequest->setRequestData(request->body.c_str(), request->body.size());
    ccRequest->setTag(request->tag.c_str());
    ccRequest->setResponseCallback(request->targetObj, request->callback);
    ccRequest->setUserData(request->userData);
    ccRequest->setType(request->type);
    return ccRequest;
}

// also one for CCHttpResponse
struct MyResponseObject {
    MyRequestObject request;
    bool succeed;
    std::vector<char> body;
    std::vector<char> header;
    int statusCode;
    std::string errorBuffer;
};

MyResponseObject toMyResponseObject(CCHttpResponse *response) {
    return {
        toMyRequestObject(response->getHttpRequest()),
        response->isSucceed(),
        *(response->getResponseData()),
        *(response->getResponseHeader()),
        response->getResponseCode(),
        response->getErrorBuffer()
    };
}
CCHttpResponse* toCCHttpResponsePtr(MyResponseObject *response) {
    auto ccRequest = toCCHttpRequestPtr(&(response->request));
    auto ccResponse = new CCHttpResponse(ccRequest);
    ccResponse->setSucceed(response->succeed);
    ccResponse->setResponseData(&(response->body));
    ccResponse->setResponseHeader(&(response->header));
    ccResponse->setResponseCode(response->statusCode);
    ccResponse->setErrorBuffer(response->errorBuffer.c_str());
    return ccResponse;
}

void logRequest(MyRequestObject request) {
    log::info("request body: {}", request.body);
    log::info("request headers:");
    for (auto headerStr : request.headers) log::info("    {}", headerStr);
    if (request.requestType == CCHttpRequest::HttpRequestType::kHttpGet) log::info("request type: get");
    if (request.requestType == CCHttpRequest::HttpRequestType::kHttpPost) log::info("request type: post");
    if (request.requestType == CCHttpRequest::HttpRequestType::kHttpPut) log::info("request type: put");
    if (request.requestType == CCHttpRequest::HttpRequestType::kHttpDelete) log::info("request type: delete");
}
void logRequest(CCHttpRequest *request) {
    std::string reqBody = "";

    if (request->getRequestData() != nullptr)
        std::string reqBody(request->getRequestData());

    log::info("request body: {}", reqBody);
    log::info("request headers:");
    for (auto headerStr : request->getHeaders()) log::info("    {}", headerStr);
    if (request->getRequestType() == CCHttpRequest::HttpRequestType::kHttpGet) log::info("request type: get");
    if (request->getRequestType() == CCHttpRequest::HttpRequestType::kHttpPost) log::info("request type: post");
    if (request->getRequestType() == CCHttpRequest::HttpRequestType::kHttpPut) log::info("request type: put");
    if (request->getRequestType() == CCHttpRequest::HttpRequestType::kHttpDelete) log::info("request type: delete");
}

bool captureMode = false;
bool sentFromPopup = false;
MyRequestObject theCurrentRequest = {};
MyResponseObject theCurrentResponse = {};

class ResponseInspectPopup : public geode::Popup {
    MyResponseObject m_currentResponse = {};
    CCMenu *m_inspectorMenu;
    std::string m_originalReqBody;
    TextInput *m_bodyField;

protected:
    bool init(MyRequestObject request, MyResponseObject response) {
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        if (!Popup::init(winSize.width * 0.9f, std::min(winSize.width * 0.9f, winSize.height * 0.8f)))
            return false;

        m_currentResponse = response;
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

        statusCodeField->setCallback([this, statusCodeField](std::string newCodeStr){
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

        auto bodyLabel = CCLabelBMFont::create("body:", "bigFont.fnt");
        bodyLabel->setScale(0.6);

        auto copyBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "copy", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(ResponseInspectPopup::copyBody)
        );

        auto pasteBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "paste", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(ResponseInspectPopup::pasteBody)
        );
        
        auto resetBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "reset", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(ResponseInspectPopup::resetBody)
        );

        m_bodyField = TextInput::create(m_inspectorMenu->getContentWidth() - 20.f, "", "chatFont.fnt");
        
        m_bodyField->setCallback([this](std::string newBody) {
            m_currentResponse.body = stringToCharVector(newBody);
        });
        m_bodyField->setString(charVectorToString(m_currentResponse.body));
        m_bodyField->setCommonFilter(CommonFilter::Any);
        
        bodyTopMenu->addChild(bodyLabel);
        bodyTopMenu->addChild(copyBodyButton);
        bodyTopMenu->addChild(pasteBodyButton);
        bodyTopMenu->addChild(resetBodyButton);
        bodyTopMenu->updateLayout();
        m_inspectorMenu->addChild(bodyLabel);
        m_inspectorMenu->addChild(m_bodyField);
    }

    void copyBody(CCObject*) {
        clipboard::write(m_currentResponse.body);
    }

    void pasteBody(CCObject*) {
        m_currentResponse.body = clipboard::read();
        m_bodyField->setString(m_currentResponse.body);
    }

    void resetBody(CCObject*) {
        m_currentResponse.body = m_originalReqBody;
        m_bodyField->setString(m_currentResponse.body);
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

        auto ccResponse = toCCHttpResponsePtr(&theCurrentResponse);
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
    MyRequestObject m_currentRequest = {};
    std::string m_originalReqBody;
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
        this->setTitle("request inspector");

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
        urlField->setCallback([this](std::string newUrl) {
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

        auto methodLabel = CCLabelBMFont::create("method:", "bigFont.fnt");
        methodLabel->setScale(0.6);

        m_getMethodButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "get", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::setHttpMethod)
        );
        m_postMethodButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "post", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::setHttpMethod)
        );
        m_putMethodButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "put", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::setHttpMethod)
        );
        m_deleteMethodButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "delete", 0, 0, 1, false,
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

        auto bodyLabel = CCLabelBMFont::create("body:", "bigFont.fnt");
        bodyLabel->setScale(0.6);

        auto copyBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "copy", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::copyBody)
        );

        auto pasteBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "paste", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::pasteBody)
        );
        
        auto resetBodyButton = CCMenuItemSpriteExtra::create(
            ButtonSprite::create(
                "reset", 0, 0, 1, false,
                "goldFont.fnt",
                "GJ_button_04.png", 30
            ),
            this, menu_selector(RequestInspectPopup::resetBody)
        );

        m_bodyField = TextInput::create(m_inspectorMenu->getContentWidth() - 20.f, "", "chatFont.fnt");
        
        m_bodyField->setCallback([this](std::string newBody) {
            m_currentRequest.body = newBody;
        });
        m_bodyField->setString(m_currentRequest.body);
        m_bodyField->setCommonFilter(CommonFilter::Any);
        
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
        m_bodyField->setString(m_currentRequest.body);
    }

    void resetBody(CCObject*) {
        m_currentRequest.body = m_originalReqBody;
        m_bodyField->setString(m_currentRequest.body);
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
        auto wrapperRequest = toCCHttpRequestPtr(&theCurrentRequest);
        wrapperRequest->setResponseCallback(this, httpresponse_selector(RequestInspectPopup::onWrapperRequest));

        CCHttpClient::getInstance()->send(wrapperRequest);
        wrapperRequest->release();
        this->onClose(nullptr);
    }

    void onWrapperRequest(CCHttpClient *client, CCHttpResponse *response) {
        auto respData = response->getResponseData();
        std::string respDataStr(respData->begin(), respData->end());

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
            theCurrentRequest = toMyRequestObject(request);
            logRequest(theCurrentRequest);
            
            OverlayManager::get()->scheduleOnce(
                schedule_selector(MyCCHttpClient::delayedShowRequestPopup), 0.1
            );

            return;
        }

        if (!sentFromPopup) {
            log::info("new request: {}", request->getUrl());
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
