#pragma once

#include <Preferences.h>
#include <WebServer.h>
#include "NukiWrapper.h"
#include "Network.h"
#include "NukiOpenerWrapper.h"
#include "Ota.h"

enum class TokenType
{
    None,
    MqttServer,
    MqttPort,
    MqttUser,
    MqttPass,
    MqttPath,
    QueryIntervalLockstate,
    QueryIntervalBattery,
};

class WebCfgServer
{
public:
    WebCfgServer(NukiWrapper* nuki, NukiOpenerWrapper* nukiOpener, Network* network, EthServer* ethServer, Preferences* preferences, bool allowRestartToPortal);
    ~WebCfgServer() = default;

    void initialize();
    void update();


private:
    bool processArgs(String& message);
    void buildHtml(String& response);
    void buildCredHtml(String& response);
    void buildOtaHtml(String& response);
    void buildMqttConfigHtml(String& response);
    void buildNukiConfigHtml(String& response);
    void buildConfirmHtml(String& response, const String &message, uint32_t redirectDelay = 5);
    void buildConfigureWifiHtml(String& response);
    void sendNewCss();
    void sendFontsInterMinCss();
    void processUnpair(bool opener);

    void buildHtmlHeader(String& response);
    void printInputField(String& response, const char* token, const char* description, const char* value, const size_t maxLength, const bool isPassword = false);
    void printInputField(String& response, const char* token, const char* description, const int value, size_t maxLength);
    void printCheckBox(String& response, const char* token, const char* description, const bool value);
    void printTextarea(String& response, const char *token, const char *description, const char *value, const size_t maxLength);
    void buildNavigationButton(String& response, const char* caption, const char* targetPath);

    void printParameter(String& response, const char* description, const char* value);

    String generateConfirmCode();
    void waitAndProcess(const bool blocking, const uint32_t duration);
    void handleOtaUpload();

    WebServer _server;
    NukiWrapper* _nuki;
    NukiOpenerWrapper* _nukiOpener;
    Network* _network;
    Preferences* _preferences;
    Ota _ota;

    // escaped by https://www.cescaper.com/
    // source: https://cdn.jsdelivr.net/npm/@exampledev/new.css@1.1.2/new.min.css
    const String newcss = ":root{--nc-font-sans:\\'Inter\\',-apple-system,BlinkMacSystemFont,\\'Segoe UI\\',Roboto,Oxygen,Ubuntu,Cantarell,\\'Open Sans\\',\\'Helvetica Neue\\',sans-serif,\\\"Apple Color Emoji\\\",\\\"Segoe UI Emoji\\\",\\\"Segoe UI Symbol\\\";--nc-font-mono:Consolas,monaco,\\'Ubuntu Mono\\',\\'Liberation Mono\\',\\'Courier New\\',Courier,monospace;--nc-tx-1:#000000;--nc-tx-2:#1A1A1A;--nc-bg-1:#FFFFFF;--nc-bg-2:#F6F8FA;--nc-bg-3:#E5E7EB;--nc-lk-1:#0070F3;--nc-lk-2:#0366D6;--nc-lk-tx:#FFFFFF;--nc-ac-1:#79FFE1;--nc-ac-tx:#0C4047}@media (prefers-color-scheme:dark){:root{--nc-tx-1:#ffffff;--nc-tx-2:#eeeeee;--nc-bg-1:#000000;--nc-bg-2:#111111;--nc-bg-3:#222222;--nc-lk-1:#3291FF;--nc-lk-2:#0070F3;--nc-lk-tx:#FFFFFF;--nc-ac-1:#7928CA;--nc-ac-tx:#FFFFFF}}*{margin:0;padding:0}address,area,article,aside,audio,blockquote,datalist,details,dl,fieldset,figure,form,iframe,img,input,meter,nav,ol,optgroup,option,output,p,pre,progress,ruby,section,table,textarea,ul,video{margin-bottom:1rem}button,html,input,select{font-family:var(--nc-font-sans)}body{margin:0 auto;max-width:750px;padding:2rem;border-radius:6px;overflow-x:hidden;word-break:break-word;overflow-wrap:break-word;background:var(--nc-bg-1);color:var(--nc-tx-2);font-size:1.03rem;line-height:1.5}::selection{background:var(--nc-ac-1);color:var(--nc-ac-tx)}h1,h2,h3,h4,h5,h6{line-height:1;color:var(--nc-tx-1);padding-top:.875rem}h1,h2,h3{color:var(--nc-tx-1);padding-bottom:2px;margin-bottom:8px;border-bottom:1px solid var(--nc-bg-2)}h4,h5,h6{margin-bottom:.3rem}h1{font-size:2.25rem}h2{font-size:1.85rem}h3{font-size:1.55rem}h4{font-size:1.25rem}h5{font-size:1rem}h6{font-size:.875rem}a{color:var(--nc-lk-1)}a:hover{color:var(--nc-lk-2)}abbr:hover{cursor:help}blockquote{padding:1.5rem;background:var(--nc-bg-2);border-left:5px solid var(--nc-bg-3)}abbr{cursor:help}blockquote :last-child{padding-bottom:0;margin-bottom:0}header{background:var(--nc-bg-2);border-bottom:1px solid var(--nc-bg-3);padding:2rem 1.5rem;margin:-2rem calc(0px - (50vw - 50%)) 2rem;padding-left:calc(50vw - 50%);padding-right:calc(50vw - 50%)}header h1,header h2,header h3{padding-bottom:0;border-bottom:0}header>:first-child{margin-top:0;padding-top:0}header>:last-child{margin-bottom:0}a button,button,input[type=button],input[type=reset],input[type=submit]{font-size:1rem;display:inline-block;padding:6px 12px;text-align:center;text-decoration:none;white-space:nowrap;background:var(--nc-lk-1);color:var(--nc-lk-tx);border:0;border-radius:4px;box-sizing:border-box;cursor:pointer;color:var(--nc-lk-tx)}a button[disabled],button[disabled],input[type=button][disabled],input[type=reset][disabled],input[type=submit][disabled]{cursor:default;opacity:.5;cursor:not-allowed}.button:focus,.button:hover,button:focus,button:hover,input[type=button]:focus,input[type=button]:hover,input[type=reset]:focus,input[type=reset]:hover,input[type=submit]:focus,input[type=submit]:hover{background:var(--nc-lk-2)}code,kbd,pre,samp{font-family:var(--nc-font-mono)}code,kbd,pre,samp{background:var(--nc-bg-2);border:1px solid var(--nc-bg-3);border-radius:4px;padding:3px 6px;font-size:.9rem}kbd{border-bottom:3px solid var(--nc-bg-3)}pre{padding:1rem 1.4rem;max-width:100%;overflow:auto}pre code{background:inherit;font-size:inherit;color:inherit;border:0;padding:0;margin:0}code pre{display:inline;background:inherit;font-size:inherit;color:inherit;border:0;padding:0;margin:0}details{padding:.6rem 1rem;background:var(--nc-bg-2);border:1px solid var(--nc-bg-3);border-radius:4px}summary{cursor:pointer;font-weight:700}details[open]{padding-bottom:.75rem}details[open] summary{margin-bottom:6px}details[open]>:last-child{margin-bottom:0}dt{font-weight:700}dd::before{content:\\'→ \\'}hr{border:0;border-bottom:1px solid var(--nc-bg-3);margin:1rem auto}fieldset{margin-top:1rem;padding:2rem;border:1px solid var(--nc-bg-3);border-radius:4px}legend{padding:auto .5rem}table{border-collapse:collapse;width:100%}td,th{border:1px solid var(--nc-bg-3);text-align:left;padding:.5rem}th{background:var(--nc-bg-2)}tr:nth-child(even){background:var(--nc-bg-2)}table caption{font-weight:700;margin-bottom:.5rem}textarea{max-width:100%}ol,ul{padding-left:2rem}li{margin-top:.4rem}ol ol,ol ul,ul ol,ul ul{margin-bottom:0}mark{padding:3px 6px;background:var(--nc-ac-1);color:var(--nc-ac-tx)}input,select,textarea{padding:6px 12px;margin-bottom:.5rem;background:var(--nc-bg-2);color:var(--nc-tx-2);border:1px solid var(--nc-bg-3);border-radius:4px;box-shadow:none;box-sizing:border-box}img{max-width:100%}";

    // escaped by https://www.cescaper.com/
    // source: https://cdn.jsdelivr.net/npm/open-fonts@1.1.1/fonts/inter.min.css
    const String intercss = "@font-face{font-family:Inter;src:url(src/inter/Inter-Thin.woff2) format(\\'woff2\\'),url(src/inter/Inter-Thin.woff) format(\\'woff\\'),url(src/inter/Inter-Thin.ttf) format(\\'truetype\\');font-weight:100;font-style:normal}@font-face{font-family:Inter;src:url(src/inter/Inter-ExtraLight.woff2) format(\\'woff2\\'),url(src/inter/Inter-ExtraLight.woff) format(\\'woff\\'),url(src/inter/Inter-ExtraLight.ttf) format(\\'truetype\\');font-weight:200;font-style:normal}@font-face{font-family:Inter;src:url(src/inter/Inter-Light.woff2) format(\\'woff2\\'),url(src/inter/Inter-Light.woff) format(\\'woff\\'),url(src/inter/Inter-Light.ttf) format(\\'truetype\\');font-weight:300;font-style:normal}@font-face{font-family:Inter;src:url(src/inter/Inter-Regular.woff2) format(\\'woff2\\'),url(src/inter/Inter-Regular.woff) format(\\'woff\\'),url(src/inter/Inter-Regular.ttf) format(\\'truetype\\');font-weight:400;font-style:normal}@font-face{font-family:Inter;src:url(src/inter/Inter-Medium.woff2) format(\\'woff2\\'),url(src/inter/Inter-Medium.woff) format(\\'woff\\'),url(src/inter/Inter-Medium.ttf) format(\\'truetype\\');font-weight:500;font-style:normal}@font-face{font-family:Inter;src:url(src/inter/Inter-SemiBold.woff2) format(\\'woff2\\'),url(src/inter/Inter-SemiBold.woff) format(\\'woff\\'),url(src/inter/Inter-SemiBold.ttf) format(\\'truetype\\');font-weight:600;font-style:normal}@font-face{font-family:Inter;src:url(src/inter/Inter-Bold.woff2) format(\\'woff2\\'),url(src/inter/Inter-Bold.woff) format(\\'woff\\'),url(src/inter/Inter-Bold.ttf) format(\\'truetype\\');font-weight:700;font-style:normal}@font-face{font-family:Inter;src:url(src/inter/Inter-ExtraBold.woff2) format(\\'woff2\\'),url(src/inter/Inter-ExtraBold.woff) format(\\'woff\\'),url(src/inter/Inter-ExtraBold.ttf) format(\\'truetype\\');font-weight:800;font-style:normal}@font-face{font-family:Inter;src:url(src/inter/Inter-Black.woff2) format(\\'woff2\\'),url(src/inter/Inter-Black.woff) format(\\'woff\\'),url(src/inter/Inter-Black.ttf) format(\\'truetype\\');font-weight:900;font-style:normal}@font-face{font-family:Inter;src:url(src/inter/Inter-ThinItalic.woff2) format(\\'woff2\\'),url(src/inter/Inter-ThinItalic.woff) format(\\'woff\\'),url(src/inter/Inter-ThinItalic.ttf) format(\\'truetype\\');font-weight:100;font-style:italic}@font-face{font-family:Inter;src:url(src/inter/Inter-ExtraLightItalic.woff2) format(\\'woff2\\'),url(src/inter/Inter-ExtraLightItalic.woff) format(\\'woff\\'),url(src/inter/Inter-ExtraLightItalic.ttf) format(\\'truetype\\');font-weight:200;font-style:italic}@font-face{font-family:Inter;src:url(src/inter/Inter-LightItalic.woff2) format(\\'woff2\\'),url(src/inter/Inter-LightItalic.woff) format(\\'woff\\'),url(src/inter/Inter-LightItalic.ttf) format(\\'truetype\\');font-weight:300;font-style:italic}@font-face{font-family:Inter;src:url(src/inter/Inter-Italic.woff2) format(\\'woff2\\'),url(src/inter/Inter-Italic.woff) format(\\'woff\\'),url(src/inter/Inter-Italic.ttf) format(\\'truetype\\');font-weight:400;font-style:italic}@font-face{font-family:Inter;src:url(src/inter/Inter-MediumItalic.woff2) format(\\'woff2\\'),url(src/inter/Inter-MediumItalic.woff) format(\\'woff\\'),url(src/inter/Inter-MediumItalic.ttf) format(\\'truetype\\');font-weight:500;font-style:italic}@font-face{font-family:Inter;src:url(src/inter/Inter-SemiBoldItalic.woff2) format(\\'woff2\\'),url(src/inter/Inter-SemiBoldItalic.woff) format(\\'woff\\'),url(src/inter/Inter-SemiBoldItalic.ttf) format(\\'truetype\\');font-weight:600;font-style:italic}@font-face{font-family:Inter;src:url(src/inter/Inter-BoldItalic.woff2) format(\\'woff2\\'),url(src/inter/Inter-BoldItalic.woff) format(\\'woff\\'),url(src/inter/Inter-BoldItalic.ttf) format(\\'truetype\\');font-weight:700;font-style:italic}@font-face{font-family:Inter;src:url(src/inter/Inter-ExtraBoldItalic.woff2) format(\\'woff2\\'),url(src/inter/Inter-ExtraBoldItalic.woff) format(\\'woff\\'),url(src/inter/Inter-ExtraBoldItalic.ttf) format(\\'truetype\\');font-weight:800;font-style:italic}@font-face{font-family:Inter;src:url(src/inter/Inter-BlackItalic.woff2) format(\\'woff2\\'),url(src/inter/Inter-BlackItalic.woff) format(\\'woff\\'),url(src/inter/Inter-BlackItalic.ttf) format(\\'truetype\\');font-weight:900;font-style:italic}";

    bool _hasCredentials = false;
    char _credUser[20] = {0};
    char _credPassword[20] = {0};
    bool _allowRestartToPortal = false;
    uint32_t _transferredSize = 0;
    bool _otaStart = true;

    String _confirmCode = "----";

    bool _enabled = true;
};