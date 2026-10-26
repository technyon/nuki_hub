#include "WebCfgServer.h"
#include "WebCfgServerConstants.h"
#include "PreferencesKeys.h"
#include "Logger.h"
#include "RestartReason.h"
extern bool timeSynced;

static const Setting credentialsSettings[] =
{
    {"DUOBYPASS", "Bypass MFA (Duo/TOTP) authentication by pressing the BOOT button while logging in", preference_cred_bypass_boot_btn_enabled, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartNoReconnect},
    {"DUOBYPASSHIGH", "Bypass MFA (Duo/TOTP) authentication by pulling GPIO High", preference_cred_bypass_gpio_high, SettingType::Int, -1, 0, 0, 2, "", SettingEffect::RestartNoReconnect},
    {"DUOBYPASSLOW", "Bypass MFA (Duo/TOTP) authentication by pulling GPIO Low", preference_cred_bypass_gpio_low, SettingType::Int, -1, 0, 0, 2, "", SettingEffect::RestartNoReconnect},
    {"DUOAPPROVAL", "Require MFA (Duo/TOTP) authentication for all sensitive Nuki Hub operations (changing/exporting settings)", preference_cred_duo_approval, SettingType::Bool, 0, 0, 0, 0, "", SettingEffect::RestartNoReconnect},
    {"CREDLFTM", "Session validity (in seconds)", preference_cred_session_lifetime, SettingType::Int, 3600, 0, 0, 12, "", SettingEffect::ClearSession},
    {"CREDLFTMRMBR", "Session validity remember (in hours)", preference_cred_session_lifetime_remember, SettingType::Int, 720, 0, 0, 12, "", SettingEffect::ClearSession},
    {"CREDDUOLFTM", "Duo Session validity (in seconds)", preference_cred_session_lifetime_duo, SettingType::Int, 3600, 0, 0, 12, "", SettingEffect::ClearSession},
    {"CREDDUOLFTMRMBR", "Duo Session validity remember (in hours)", preference_cred_session_lifetime_duo_remember, SettingType::Int, 720, 0, 0, 12, "", SettingEffect::ClearSession},
    {"CREDTOTPLFTM", "TOTP Session validity (in seconds)", preference_cred_session_lifetime_totp, SettingType::Int, 3600, 0, 0, 12, "", SettingEffect::ClearSession},
    {"CREDTOTPLFTMRMBR", "TOTP Session validity remember (in hours)", preference_cred_session_lifetime_totp_remember, SettingType::Int, 720, 0, 0, 12, "", SettingEffect::ClearSession},
};

static const size_t credentialsSettingsCount = sizeof(credentialsSettings) / sizeof(credentialsSettings[0]);

bool WebCfgServer::isAuthenticated(PsychicRequest *request, int type)
{
    String cookieKey = "sessionId";

    if (type == 1)
    {
        cookieKey = "duoId";
    }
    else if (type == 2)
    {
        cookieKey = "totpId";
    }
    else if (type == 3)
    {
        cookieKey = "bypassId";
    }

    if (request->hasCookie(cookieKey.c_str()))
    {
        String cookie = request->getCookie(cookieKey.c_str());

        if ((type == 0 && _sessionProvider->httpSessions()[cookie].is<JsonVariant>()) || (type == 1 && _importExport->_duoSessions[cookie].is<JsonVariant>()) || (type == 2 && _importExport->_totpSessions[cookie].is<JsonVariant>()) || (type == 3 && _importExport->_bypassSessions[cookie].is<JsonVariant>()))
        {
            struct timeval time;
            gettimeofday(&time, NULL);
            int64_t time_us = 0;

            if (timeSynced)
            {
                time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;
            }

            if ((type == 0 && _sessionProvider->httpSessions()[cookie].as<signed long long>() > time_us) || (type == 1 && _importExport->_duoSessions[cookie].as<signed long long>() > time_us) || (type == 2 && _importExport->_totpSessions[cookie].as<signed long long>() > time_us) || (type == 3 && _importExport->_bypassSessions[cookie].as<signed long long>() > time_us))
            {
                return true;
            }
            else
            {
                Log->println("Cookie found, but not valid anymore");
            }
        }
    }
    return false;
}

esp_err_t WebCfgServer::logoutSession(PsychicRequest *request, PsychicResponse* resp)
{
    Log->println("Logging out");

    if (!_isSSL)
    {
        resp->setCookie("sessionId", "", 0, "HttpOnly");
    }
    else
    {
        resp->setCookie("sessionId", "", 0, "Secure; HttpOnly");
    }

    if (request->hasCookie("sessionId"))
    {
        String cookie = request->getCookie("sessionId");
        _sessionProvider->httpSessions().remove(cookie);
        _sessionProvider->saveSessions();
    }
    else
    {
        Log->println("No session cookie found");
    }

    if (_duoEnabled)
    {
        if (!_isSSL)
        {
            resp->setCookie("duoId", "", 0, "HttpOnly");
        }
        else
        {
            resp->setCookie("duoId", "", 0, "Secure; HttpOnly");
        }

        if (request->hasCookie("duoId"))
        {
            String cookie2 = request->getCookie("duoId");
            _importExport->_duoSessions.remove(cookie2);
            _sessionProvider->saveSessions(1);
        }
        else
        {
            Log->println("No duo session cookie found");
        }
    }

    if (_importExport->getTOTPEnabled())
    {
        if (!_isSSL)
        {
            resp->setCookie("totpId", "", 0, "HttpOnly");
        }
        else
        {
            resp->setCookie("totpId", "", 0, "Secure; HttpOnly");
        }

        if (request->hasCookie("totpId"))
        {
            String cookie2 = request->getCookie("totpId");
            _importExport->_totpSessions.remove(cookie2);
            _sessionProvider->saveSessions(2);
        }
        else
        {
            Log->println("No totp session cookie found");
        }
    }

    if (_importExport->getBypassEnabled())
    {
        if (!_isSSL)
        {
            resp->setCookie("bypassId", "", 0, "HttpOnly");
        }
        else
        {
            resp->setCookie("bypassId", "", 0, "Secure; HttpOnly");
        }

        if (request->hasCookie("bypassId"))
        {
            String cookie2 = request->getCookie("bypassId");
            _importExport->_bypassSessions.remove(cookie2);
        }
    }

    return buildConfirmHtml(request, resp, "Logging out", 3, true);
}

bool WebCfgServer::useTOTP(PsychicRequest *request)
{
    // Use TOTP when Duo is not enabled; when both are enabled, use the method chosen at login.
    // Basic/Digest auth has no login form to choose, so the session flag is never set there.
    if(!_importExport->getTOTPEnabled())
    {
        return false;
    }

    return !_duoEnabled || _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"].as<bool>();
}

int WebCfgServer::doAuthentication(PsychicRequest *request)
{
    if (!_network->isApOpen() && _preferences->getString(preference_bypass_proxy, "") != "" && request->client()->localIP().toString() == _preferences->getString(preference_bypass_proxy, ""))
    {
        return 4;
    }
    else if(strlen(_credUser) > 0 && strlen(_credPassword) > 0)
    {
        int savedAuthType = _preferences->getInt(preference_http_auth_type, 0);
        if (savedAuthType == 2)
        {
            if (!isAuthenticated(request))
            {
                Log->println("Authentication Failed");
                return savedAuthType;
            }
        }
        else
        {
            if (!request->authenticate(_credUser, _credPassword))
            {
                Log->println("Authentication Failed");
                return savedAuthType;
            }
        }

        if (_duoEnabled || _importExport->getTOTPEnabled())
        {
            if (_bypassGPIO)
            {
                if (digitalRead(BOOT_BUTTON_GPIO) == LOW)
                {
                    Log->println("Duo bypassed because boot button pressed");
                    return 4;
                }
            }
            if (_bypassGPIOHigh > -1)
            {
                if (digitalRead(_bypassGPIOHigh) == HIGH)
                {
                    Log->println("Duo bypassed because bypass GPIO pin pulled high");
                    return 4;
                }
            }
            if (_bypassGPIOLow > -1)
            {
                if (digitalRead(_bypassGPIOLow) == LOW)
                {
                    Log->println("Duo bypassed because bypass GPIO pin pulled low");
                    return 4;
                }
            }

            if(_duoEnabled && isAuthenticated(request, 1))
            {
                _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] = false;
                return 4;
            }
            else if(_importExport->getTOTPEnabled() && isAuthenticated(request, 2))
            {
                _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] = true;
                return 4;
            }
            else if(!timeSynced && _importExport->getBypassEnabled() && isAuthenticated(request, 3))
            {
                _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] = false;
                return 4;
            }

            Log->println("Authentication Failed");

            if(useTOTP(request))
            {
                return 5;
            }

            return 3;
        }
    }

    return 4;
}

esp_err_t WebCfgServer::buildLoginHtml(PsychicRequest *request, PsychicResponse* resp)
{
    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    response.print("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    response.print("<style>form{border:3px solid #f1f1f1; max-width: 400px;}input[type=password],input[type=text]{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;box-sizing:border-box}button{background-color:#04aa6d;color:#fff;padding:14px 20px;margin:8px 0;border:none;cursor:pointer;width:100%}button:hover{opacity:.8}.container{padding:16px}span.password{float:right;padding-top:16px}@media screen and (max-width:300px){span.psw{display:block;float:none}}</style>");
    response.print("</head><body><center><h2>Nuki Hub login</h2><form action=\"/post?page=login\" method=\"post\">");
    response.print("<div class=\"container\"><label for=\"username\"><b>Username</b></label><input type=\"text\" placeholder=\"Enter Username\" name=\"username\" required>");
    response.print("<label for=\"password\"><b>Password</b></label><input type=\"password\" placeholder=\"Enter Password\" name=\"password\" required>");
    if (_importExport->getTOTPEnabled() || _duoEnabled)
    {
        if (_importExport->getTOTPEnabled())
        {
            response.print("<button name=\"totp\" type=\"submit\" formaction=\"/post?page=login&totp=1\">Login with TOTP</button>");
        }
        if (_duoEnabled)
        {
            response.print("<button name=\"duo\" type=\"submit\">Login with Duo</button>");
        }
    }
    else
    {
        response.print("<button type=\"submit\">Login</button>");
    }
    response.print("<label><input type=\"checkbox\" name=\"remember\"> Remember me</label></div>");
    response.print("</form></center></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildTOTPHtml(PsychicRequest *request, PsychicResponse* resp, int type)
{
    if (!timeSynced)
    {
        return buildConfirmHtml(request, resp, "NTP time not synced yet, TOTP not available, please wait for NTP to sync or use <a href=\"/get?page=bypass\">one-time bypass</a>", 3, true);
    }

    if((pow(_importExport->_invalidCount, 5) + _importExport->_lastCodeCheck) > espMillis())
    {
        return buildConfirmHtml(request, resp, "Too many invalid TOTP tries, please wait before retrying or use <a href=\"/get?page=bypass\">one-time bypass</a>", 3, true);
    }

    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    response.print("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    response.print("<style>form{border:3px solid #f1f1f1; max-width: 400px;}input[type=password],input[type=text]{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;box-sizing:border-box}button{background-color:#04aa6d;color:#fff;padding:14px 20px;margin:8px 0;border:none;cursor:pointer;width:100%}button:hover{opacity:.8}.container{padding:16px}span.password{float:right;padding-top:16px}@media screen and (max-width:300px){span.psw{display:block;float:none}}</style>");
    response.print("</head><body><center><h2>Nuki Hub TOTP</h2>");

    String typeText = "Login";

    if(type == 0)
    {
        response.print("<form action=\"/post?page=totp\" method=\"post\">");
    }
    else
    {
        if(type == 1)
        {
            typeText = "Export";
            response.print((String)"<form action=\"" + request->uri() + "\" method=\"get\" target=\"_blank\">");
        }
        else if(type == 2)
        {
            typeText = "Import";
            response.print((String)"<form action=\"" + request->uri() + "\" method=\"post\">");
        }
        else
        {
            typeText = "Save";
            response.print((String)"<form action=\"" + request->uri() + "\" method=\"post\">");
        }

        if (request->hasParam("totpkey"))
        {
            const PsychicWebParameter* p = request->getParam("totpkey");
            response.print((String)"<input type=\"hidden\" name=\"" + p->name() + "\" value='" + p->value() + "' />");
        }
    }

    response.print("<div class=\"container\">");
    response.print("<label for=\"totpkey\"><b>TOTP</b></label><input type=\"text\" placeholder=\"Enter TOTP code\" name=\"totpkey\">");
    response.print("<button type=\"submit\" ");
    if(type == 1)
    {
        response.print("onclick=\"setTimeout(function() { window.location.href = '/' }, 1000);\"");
    }
    response.print((String)">" + typeText + "</button></div>");
    response.print("</form></center></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildBypassHtml(PsychicRequest *request, PsychicResponse* resp)
{
    if (timeSynced)
    {
        return buildConfirmHtml(request, resp, "One-time bypass is only available if NTP time is not synced</a>", 3, true);
    }

    if((pow(_importExport->_invalidCount2, 5) + _importExport->_lastCodeCheck2) > espMillis())
    {
        return buildConfirmHtml(request, resp, "Too many invalid bypass tries, please wait before retrying", 3, true);
    }

    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    response.print("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
    response.print("<style>form{border:3px solid #f1f1f1; max-width: 400px;}input[type=password],input[type=text]{width:100%;padding:12px 20px;margin:8px 0;display:inline-block;border:1px solid #ccc;box-sizing:border-box}button{background-color:#04aa6d;color:#fff;padding:14px 20px;margin:8px 0;border:none;cursor:pointer;width:100%}button:hover{opacity:.8}.container{padding:16px}span.password{float:right;padding-top:16px}@media screen and (max-width:300px){span.psw{display:block;float:none}}</style>");
    response.print("</head><body><center><h2>Nuki Hub One-time Bypass</h2>");
    response.print("<form action=\"/post?page=bypass\" method=\"post\">");
    response.print("<div class=\"container\">");
    response.print("<label for=\"bypass\"><b>Bypass code</b></label><input type=\"text\" placeholder=\"Enter bypass code\" name=\"bypass\">");
    response.print("<button type=\"submit\" ");
    response.print(">Login</button></div>");
    response.print("</form></center></body></html>");
    return response.endSend();
}

esp_err_t WebCfgServer::buildDuoCheckHtml(PsychicRequest *request, PsychicResponse* resp)
{
    char valueStr[2];
    itoa(_importExport->checkDuoAuth(request), valueStr, 10);
    resp->setCode(200);
    resp->setContentType("text/plain");
    resp->setContent(valueStr);
    return resp->send();
}

esp_err_t WebCfgServer::buildDuoHtml(PsychicRequest *request, PsychicResponse* resp, int type)
{
    if (!timeSynced)
    {
        return buildConfirmHtml(request, resp, "NTP time not synced yet, Duo not available, please wait for NTP to sync or use <a href=\"/get?page=bypass\">one-time bypass</a>", 3, true);
    }

    String duoText;

    if (type == 0)
    {
        duoText = "login";
    }
    else if (type == 1)
    {
        duoText = "export";
    }
    else if (type == 2)
    {
        duoText = "import";
    }
    else if (type == 3)
    {
        duoText = "save";
    }

    bool duo = _importExport->startDuoAuth((char*)((String("Approve Nuki Hub ") + duoText).c_str()));

    if (!duo)
    {
        return buildConfirmHtml(request, resp, "Duo check failed", 3, true);
    }
    else
    {
        PsychicStreamResponse response(resp, "text/html");
        char buffer[33];
        int i;
        for (i = 0; i < 4; i++)
        {
            sprintf(buffer + (i * 8), "%08lx", (unsigned long int)esp_random());
        }

        if(type == 0)
        {
            int64_t durationLength = 60*60*_preferences->getInt(preference_cred_session_lifetime_duo_remember, 720);

            if (!_importExport->_sessionsOpts[request->client()->localIP().toString()])
            {
                durationLength = _preferences->getInt(preference_cred_session_lifetime_duo, 3600);
            }

            if (!_isSSL)
            {
                response.setCookie("duoId", buffer, durationLength, "HttpOnly");
            }
            else
            {
                response.setCookie("duoId", buffer, durationLength, "Secure; HttpOnly");
            }
        }

        _importExport->setDuoCheckIP(request->client()->localIP().toString());
        _importExport->setDuoCheckId(buffer);

        response.beginSend();
        response.print("<html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
        response.print("<style>.container{border:3px solid #f1f1f1; max-width: 400px; padding:16px}</style>");
        response.print((String)"<script>let intervalId; let stop = 0; window.onload = function() { intervalId = setInterval(updateInfo, 2000); }; function updateInfo() { var request = new XMLHttpRequest(); request.open('GET', '/get?page=duocheck&id=" + String(buffer) + "&type=" + type + "', true); request.onload = () => { const obj = request.responseText; if ((obj == \"1\" || obj == \"0\") && stop == 0) { stop = 1; clearInterval(intervalId); if (obj == \"1\") { document.getElementById('duoresult').innerHTML = '" + duoText + " approved<br>Redirecting...'; ");
        if(type == 0)
        {
            response.print("setTimeout(function() { window.location.href = \"/\"");
        }
        else if (type == 1)
        {
            response.print((String)"window.open('" + request->uri() + "'); setTimeout(function() { window.location.href = \"/\"");
        }
        else
        {
            response.print("setTimeout(function() { document.getElementById('frmrepost').submit()");
        }
        response.print("; }, 2000); } else { document.getElementById('duoresult').innerHTML = '" + duoText + " failed<br>Refresh to retry'; } } }; request.send(); }</script>");
        response.print("</head><body><center><h2>Nuki Hub " + duoText + "</h2>");
        response.print("<div class=\"container\">Duo Push sent<br><br>");
        response.print("Please confirm " + duoText + " in the Duo app<br><br><div id=\"duoresult\"></div></div>");
        response.print("</div>");
        if (type > 1)
        {
            response.print((String)"<form id=\"frmrepost\" action=\"" + request->uri() + "\" method=\"post\">");

            if (request->hasParam("submit"))
            {
                const PsychicWebParameter* p = request->getParam("submit");

                response.print((String)"<input type=\"hidden\" name=\"" + p->name() + "\" value='" + p->value() + "' />");
            }
            response.print("</form>");
        }
        response.print("</center></body></html>");

        return response.endSend();
    }
}

bool WebCfgServer::processLogin(PsychicRequest *request, PsychicResponse* resp)
{
    if(request->hasParam("username") && request->hasParam("password"))
    {
        const PsychicWebParameter* user = request->getParam("username");
        const PsychicWebParameter* pass = request->getParam("password");
        if(user->value() != "" && pass->value() != "")
        {
            if (user->value() == _preferences->getString(preference_cred_user, "") && pass->value() == _preferences->getString(preference_cred_password, ""))
            {
                char buffer[33];
                int i;
                int64_t durationLength = 60*60*_preferences->getInt(preference_cred_session_lifetime_remember, 720);
                for (i = 0; i < 4; i++)
                {
                    sprintf(buffer + (i * 8), "%08lx", (unsigned long int)esp_random());
                }
                if(!request->hasParam("remember"))
                {
                    durationLength = _preferences->getInt(preference_cred_session_lifetime, 3600);
                }

                _importExport->_sessionsOpts[request->client()->localIP().toString()] = request->hasParam("remember");

                if (!_isSSL)
                {
                    resp->setCookie("sessionId", buffer, durationLength, "HttpOnly");
                }
                else
                {
                    resp->setCookie("sessionId", buffer, durationLength, "Secure; HttpOnly");
                }

                struct timeval time;
                gettimeofday(&time, NULL);
                int64_t time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;
                _sessionProvider->httpSessions()[buffer] = time_us + (durationLength*1000000L);
                _sessionProvider->saveSessions();

                _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] = request->hasParam("totp");

                return true;
            }
        }
    }
    return false;
}

bool WebCfgServer::processBypass(PsychicRequest *request, PsychicResponse* resp)
{
    if(!timeSynced && request->hasParam("bypass"))
    {
        const PsychicWebParameter* pass = request->getParam("bypass");
        if(pass->value() != "")
        {
            String bypass = pass->value();
            if (_importExport->checkBypass(bypass))
            {
                char buffer[33];
                int i;
                for (i = 0; i < 4; i++)
                {
                    sprintf(buffer + (i * 8), "%08lx", (unsigned long int)esp_random());
                }

                if (!_isSSL)
                {
                    resp->setCookie("bypassId", buffer, 3600, "HttpOnly");
                }
                else
                {
                    resp->setCookie("bypassId", buffer, 3600, "Secure; HttpOnly");
                }

                struct timeval time;
                gettimeofday(&time, NULL);
                int64_t time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;
                _importExport->_bypassSessions[buffer] = time_us + ((int64_t)3600*1000000L);

                char randomstr2[33];
                randomSeed(esp_random());
                char chars[] = {'1', '2', '3','4', '5', '6','7', '8', '9', '0', 'A', 'B', 'C', 'D','E', 'F', 'G','H', 'I', 'J','K', 'L', 'M', 'N', 'O','P', 'Q','R', 'S', 'T','U', 'V', 'W','X', 'Y', 'Z'};
                for(int i = 0; i < 32; i++)
                {
                    randomstr2[i] = chars[random(36)];
                }
                randomstr2[32] = '\0';
                _preferences->putString(preference_bypass_secret, randomstr2);

                return true;
            }
        }
    }
    return false;
}

bool WebCfgServer::processTOTP(PsychicRequest *request, PsychicResponse* resp)
{
    if(timeSynced && request->hasParam("totpkey"))
    {
        const PsychicWebParameter* pass = request->getParam("totpkey");
        if(pass->value() != "")
        {
            String totpkey = pass->value();
            if (_importExport->checkTOTP(&totpkey))
            {
                char buffer[33];
                int i;
                int64_t durationLength = 60*60*_preferences->getInt(preference_cred_session_lifetime_totp_remember, 720);
                for (i = 0; i < 4; i++)
                {
                    sprintf(buffer + (i * 8), "%08lx", (unsigned long int)esp_random());
                }

                if (!_importExport->_sessionsOpts[request->client()->localIP().toString()])
                {
                    durationLength = _preferences->getInt(preference_cred_session_lifetime_totp, 3600);
                }

                if (!_isSSL)
                {
                    resp->setCookie("totpId", buffer, durationLength, "HttpOnly");
                }
                else
                {
                    resp->setCookie("totpId", buffer, durationLength, "Secure; HttpOnly");
                }

                struct timeval time;
                gettimeofday(&time, NULL);
                int64_t time_us = (int64_t)time.tv_sec * 1000000L + (int64_t)time.tv_usec;
                _importExport->_totpSessions[buffer] = time_us + (durationLength*1000000L);
                _sessionProvider->saveSessions(2);
                return true;
            }
        }
    }
    return false;
}

#ifndef NUKI_HUB_UPDATER
esp_err_t WebCfgServer::buildCredHtml(PsychicRequest *request, PsychicResponse* resp)
{
    char chars[] = {'2', '3','4', '5', '6','7', 'A', 'B', 'C', 'D','E', 'F', 'G','H', 'I', 'J','K', 'L', 'M', 'N', 'O','P', 'Q','R', 'S', 'T','U', 'V', 'W','X', 'Y', 'Z'};
    char chars2[] = {'1', '2', '3','4', '5', '6','7', '8', '9', '0', 'A', 'B', 'C', 'D','E', 'F', 'G','H', 'I', 'J','K', 'L', 'M', 'N', 'O','P', 'Q','R', 'S', 'T','U', 'V', 'W','X', 'Y', 'Z'};

    char randomstr[17];
    randomSeed(esp_random());
    for(int i = 0; i < 16; i++)
    {
        randomstr[i] = chars[random(32)];
    }
    randomstr[16] = '\0';
    char randomstr2[33];
    randomSeed(esp_random());
    for(int i = 0; i < 32; i++)
    {
        randomstr2[i] = chars2[random(36)];
    }
    randomstr2[32] = '\0';
    char randomstr3[33];
    randomSeed(esp_random());
    for(int i = 0; i < 32; i++)
    {
        randomstr3[i] = chars2[random(36)];
    }
    randomstr3[32] = '\0';

    PsychicStreamResponse response(resp, "text/html");
    response.beginSend();
    buildHtmlHeader(&response);
    response.print("<form id=\"credfrm\" class=\"adapt\" onsubmit=\"return testcreds();\" method=\"post\" action=\"post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
    response.print("<h3>Credentials</h3>");
    response.print("<table>");
    printInputField(&response, "CREDUSER", "User (# to clear)", _preferences->getString(preference_cred_user).c_str(), 30, "id=\"inputuser\"", false, true);
    printInputField(&response, "CREDPASS", "Password", "*", 30, "id=\"inputpass\"", true, true);
    printInputField(&response, "CREDPASSRE", "Retype password", "*", 30, "id=\"inputpass2\"", true);
    std::vector<std::pair<String, String>> httpAuthOptions;
    httpAuthOptions.push_back(std::make_pair("0", "Basic"));
    httpAuthOptions.push_back(std::make_pair("1", "Digest"));
    httpAuthOptions.push_back(std::make_pair("2", "Form"));
    printDropDown(&response, "CREDDIGEST", "HTTP Authentication type", String(_preferences->getInt(preference_http_auth_type, 0)), httpAuthOptions, "");
    printInputField(&response, "CREDTRUSTPROXY", "Bypass authentication for reverse proxy with IP", _preferences->getString(preference_bypass_proxy, "").c_str(), 255, "");
    printCheckBox(&response, "DUOENA", "Duo Push authentication enabled", _preferences->getBool(preference_cred_duo_enabled, false), "");
    printSetting(&response, credentialsSettings, credentialsSettingsCount, "DUOAPPROVAL");
    printSetting(&response, credentialsSettings, credentialsSettingsCount, "DUOBYPASS");
    printSetting(&response, credentialsSettings, credentialsSettingsCount, "DUOBYPASSHIGH");
    printSetting(&response, credentialsSettings, credentialsSettingsCount, "DUOBYPASSLOW");
    printInputField(&response, "DUOHOST", "Duo API hostname", "*", 255, "", true, false);
    printInputField(&response, "DUOIKEY", "Duo integration key", "*", 255, "", true, false);
    printInputField(&response, "DUOSKEY", "Duo secret key", "*", 255, "", true, false);
    printInputField(&response, "DUOUSER", "Duo user", "*", 255, "", true, false);
    printCheckBox(&response, "TOTPENA", "TOTP authentication enabled", _preferences->getBool(preference_totp_enabled, false), "");
    printInputField(&response, "CREDTOTP", "TOTP Secret Key", "*", 16, "", true, false);
    response.print("<tr id=\"totpgentr\" ><td><input type=\"button\" id=\"totpgen\" onclick=\"document.getElementsByName('CREDTOTP')[0].type='text'; document.getElementsByName('CREDTOTP')[0].value='");
    response.print(randomstr);
    response.print("'; document.getElementById('totpgentr').style.display='none';\" value=\"Generate new TOTP key\"></td></tr>");
    printInputField(&response, "CREDBYPASS", "One-time MFA Bypass", "*", 32, "", true, false);
    response.print("<tr id=\"bypassgentr\" ><td><input type=\"button\" id=\"bypassgen\" onclick=\"document.getElementsByName('CREDBYPASS')[0].type='text'; document.getElementsByName('CREDBYPASS')[0].value='");
    response.print(randomstr2);
    response.print("'; document.getElementById('bypassgentr').style.display='none';\" value=\"Generate new Bypass\"></td></tr>");
    printInputField(&response, "CREDADMIN", "Admin key", "*", 32, "", true, false);
    response.print("<tr id=\"admingentr\" ><td><input type=\"button\" id=\"admingen\" onclick=\"document.getElementsByName('CREDADMIN')[0].type='text'; document.getElementsByName('CREDADMIN')[0].value='");
    response.print(randomstr3);
    response.print("'; document.getElementById('admingentr').style.display='none';\" value=\"Generate new Admin key\"></td></tr>");
    printSetting(&response, credentialsSettings, credentialsSettingsCount, "CREDLFTM");
    printSetting(&response, credentialsSettings, credentialsSettingsCount, "CREDLFTMRMBR");
    printSetting(&response, credentialsSettings, credentialsSettingsCount, "CREDDUOLFTM");
    printSetting(&response, credentialsSettings, credentialsSettingsCount, "CREDDUOLFTMRMBR");
    printSetting(&response, credentialsSettings, credentialsSettingsCount, "CREDTOTPLFTM");
    printSetting(&response, credentialsSettings, credentialsSettingsCount, "CREDTOTPLFTMRMBR");
    response.print("</table>");
    response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
    response.print("</form><script>function testcreds() { var input_user = document.getElementById(\"inputuser\").value; var input_pass = document.getElementById(\"inputpass\").value; var input_pass2 = document.getElementById(\"inputpass2\").value; var pattern = /^[ -~]*$/; if(input_user == '#' || input_user == '') { return true; } if (input_pass != input_pass2) { alert('Passwords do not match'); return false;} if(!pattern.test(input_user) || !pattern.test(input_pass)) { alert('Only non unicode characters are allowed in username and password'); return false;} else { return true; } }</script>");
    if(_nuki != nullptr)
    {
        response.print("<br><br><form class=\"adapt\" method=\"post\" action=\"post\">");
        response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
        response.print("<h3>Nuki Lock PIN</h3>");
        response.print("<table>");
        printInputField(&response, "NUKIPIN", "PIN Code (# to clear)", "*", 20, "", true);
        response.print("</table>");
        response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        response.print("</form>");
    }
    if(_nukiOpener != nullptr)
    {
        response.print("<br><br><form class=\"adapt\" method=\"post\" action=\"post\">");
        response.print("<input type=\"hidden\" name=\"page\" value=\"savecfg\">");
        response.print("<h3>Nuki Opener PIN</h3>");
        response.print("<table>");
        printInputField(&response, "NUKIOPPIN", "PIN Code (# to clear)", "*", 20, "", true);
        response.print("</table>");
        response.print("<br><input type=\"submit\" name=\"submit\" value=\"Save\">");
        response.print("</form>");
    }
    if(_nuki != nullptr)
    {
        response.print("<br><br><h3>Unpair Nuki Lock</h3>");
        response.print("<form class=\"adapt\" method=\"post\" action=\"/post\">");
        response.print("<input type=\"hidden\" name=\"page\" value=\"unpairlock\">");
        response.print("<table>");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField(&response, "CONFIRMTOKEN", message.c_str(), "", 10, "");
        response.print("</table>");
        response.print("<br><button type=\"submit\">OK</button></form>");
    }
    if(_nukiOpener != nullptr)
    {
        response.print("<br><br><h3>Unpair Nuki Opener</h3>");
        response.print("<form class=\"adapt\" method=\"post\" action=\"/post\">");
        response.print("<input type=\"hidden\" name=\"page\" value=\"unpairopener\">");
        response.print("<table>");
        String message = "Type ";
        message.concat(_confirmCode);
        message.concat(" to confirm unpair");
        printInputField(&response, "CONFIRMTOKEN", message.c_str(), "", 10, "");
        response.print("</table>");
        response.print("<br><button type=\"submit\">OK</button></form>");
    }
    response.print("<br><br><h3>Factory reset Nuki Hub</h3>");
    response.print("<h4 class=\"warning\">This will reset all settings to default and unpair Nuki Lock and/or Opener.");
#ifndef CONFIG_IDF_TARGET_ESP32H2
    response.print(" Optionally will also reset WiFi settings and reopen WiFi manager portal.");
#endif
    response.print("</h4>");
    response.print("<form class=\"adapt\" method=\"post\" action=\"/post\">");
    response.print("<input type=\"hidden\" name=\"page\" value=\"factoryreset\">");
    response.print("<table>");
    String message = "Type ";
    message.concat(_confirmCode);
    message.concat(" to confirm factory reset");
    printInputField(&response, "CONFIRMTOKEN", message.c_str(), "", 10, "");
#ifndef CONFIG_IDF_TARGET_ESP32H2
    printCheckBox(&response, "WIFI", "Also reset WiFi settings", false, "");
#endif
    response.print("</table>");
    response.print("<br><button type=\"submit\">OK</button></form>");
    response.print("</body></html>");
    return response.endSend();
}

void WebCfgServer::processCredentialsArgs(PsychicRequest *request, ProcessArgsState& state, String& message)
{
    bool clearCredentials = false;
    String pass1 = "";
    String pass2 = "";
    String value;

    if(request->hasParam("DUOHOST"))
    {
        value = request->getParam("DUOHOST")->value();
        if(value != "*")
        {
            if(_preferences->getString(preference_cred_duo_host, "") != value)
            {
                _preferences->putString(preference_cred_duo_host, value);
                state.restartServicesNoReconnect = true;
                state.clearSession = true;
                state.newMFA = true;
            }
        }
    }

    if(request->hasParam("DUOIKEY"))
    {
        value = request->getParam("DUOIKEY")->value();
        if(value != "*")
        {
            if(_preferences->getString(preference_cred_duo_ikey, "") != value)
            {
                _preferences->putString(preference_cred_duo_ikey, value);
                state.restartServicesNoReconnect = true;
                state.clearSession = true;
                state.newMFA = true;
            }
        }
    }

    if(request->hasParam("DUOSKEY"))
    {
        value = request->getParam("DUOSKEY")->value();
        if(value != "*")
        {
            if(_preferences->getString(preference_cred_duo_skey, "") != value)
            {
                _preferences->putString(preference_cred_duo_skey, value);
                state.restartServicesNoReconnect = true;
                state.clearSession = true;
                state.newMFA = true;
            }
        }
    }

    if(request->hasParam("DUOUSER"))
    {
        value = request->getParam("DUOUSER")->value();
        if(value != "*")
        {
            if(_preferences->getString(preference_cred_duo_user, "") != value)
            {
                _preferences->putString(preference_cred_duo_user, value);
                state.restartServicesNoReconnect = true;
                state.clearSession = true;
                state.newMFA = true;
            }
        }
    }

    if(request->hasParam("DUOENA"))
    {
        bool v = isParameterTrue(request, "DUOENA");
        if(_preferences->getBool(preference_cred_duo_enabled, false) != v)
        {
            _preferences->putBool(preference_cred_duo_enabled, v);
            if (v)
            {
                _preferences->putBool(preference_update_time, true);
            }

            state.restartServicesNoReconnect = true;
            state.clearSession = true;
            state.newMFA = true;
        }
    }

    applySettings(request, credentialsSettings, credentialsSettingsCount, state);

    if(request->hasParam("CREDDIGEST"))
    {
        value = request->getParam("CREDDIGEST")->value();
        if(_preferences->getInt(preference_http_auth_type, 0) != value.toInt())
        {
            _preferences->putInt(preference_http_auth_type, value.toInt());

            state.restartServicesNoReconnect = true;
            state.clearSession = true;
        }
    }

    if(request->hasParam("CREDTRUSTPROXY"))
    {
        value = request->getParam("CREDTRUSTPROXY")->value();
        if(value != "*")
        {
            if(_preferences->getString(preference_bypass_proxy, "") != value)
            {
                _preferences->putString(preference_bypass_proxy, value);
                state.restartServicesNoReconnect = true;
                state.clearSession = true;
            }
        }
    }

    if(request->hasParam("CREDUSER"))
    {
        value = request->getParam("CREDUSER")->value();
        if(value == "#")
        {
            clearCredentials = true;
        }
        else
        {
            if(_preferences->getString(preference_cred_user, "") != value)
            {
                _preferences->putString(preference_cred_user, value);
                state.restartServicesNoReconnect = true;
                state.clearSession = true;
            }
        }
    }

    if(request->hasParam("CREDPASS"))
    {
        pass1 = request->getParam("CREDPASS")->value();
    }

    if(request->hasParam("CREDPASSRE"))
    {
        pass2 = request->getParam("CREDPASSRE")->value();
    }

    if(request->hasParam("TOTPENA"))
    {
        _preferences->putBool(preference_totp_enabled, isParameterTrue(request, "TOTPENA"));
    }

    if(request->hasParam("CREDTOTP"))
    {
        value = request->getParam("CREDTOTP")->value();
        if(value != "*")
        {
            if(_preferences->getString(preference_totp_secret, "") != value)
            {
                _preferences->putString(preference_totp_secret, value);
                state.restartServicesNoReconnect = true;
                state.clearSession = true;
                state.newMFA = true;
                _importExport->_sessionsOpts[request->client()->localIP().toString() + "totp"] = true;
            }
        }
    }

    if(request->hasParam("CREDBYPASS"))
    {
        value = request->getParam("CREDBYPASS")->value();
        if(value != "*")
        {
            if(_preferences->getString(preference_bypass_secret, "") != value)
            {
                _preferences->putString(preference_bypass_secret, value);
                state.restartServicesNoReconnect = true;
            }
        }
    }

    if(request->hasParam("CREDADMIN"))
    {
        value = request->getParam("CREDADMIN")->value();
        if(value != "*")
        {
            if(_preferences->getString(preference_admin_secret, "") != value)
            {
                _preferences->putString(preference_admin_secret, value);
            }
        }
    }

    if(request->hasParam("NUKIPIN") && _nuki != nullptr)
    {
        value = request->getParam("NUKIPIN")->value();
        if(value == "#")
        {
            if (_preferences->getBool(preference_lock_gemini_enabled, false))
            {
                message = "Nuki Lock Ultra/Go/5th gen PIN cleared";
                _nuki->setUltraPin(0xffffffff);
                _preferences->putInt(preference_lock_gemini_pin, 0);
            }
            else
            {
                message = "Nuki Lock PIN cleared";
                _nuki->setPin(0xffff);
            }
            state.restartServicesNoReconnect = true;
        }
        else
        {
            if (_preferences->getBool(preference_lock_gemini_enabled, false))
            {
                if(_nuki->getUltraPin() != value.toInt())
                {
                    message = "Nuki Lock Ultra/Go/5th gen PIN saved";
                    _nuki->setUltraPin(value.toInt());
                    _preferences->putInt(preference_lock_gemini_pin, value.toInt());
                    state.restartServicesNoReconnect = true;
                }
            }
            else
            {
                if(_nuki->getPin() != value.toInt())
                {
                    message = "Nuki Lock PIN saved";
                    _nuki->setPin(value.toInt());
                    state.restartServicesNoReconnect = true;
                }
            }
        }
    }

    if(request->hasParam("NUKIOPPIN") && _nukiOpener != nullptr)
    {
        value = request->getParam("NUKIOPPIN")->value();
        if(value == "#")
        {
            message = "Nuki Opener PIN cleared";
            _nukiOpener->setPin(0xffff);
            state.restartServicesNoReconnect = true;
        }
        else
        {
            if(_nukiOpener->getPin() != value.toInt())
            {
                message = "Nuki Opener PIN saved";
                _nukiOpener->setPin(value.toInt());
                state.restartServicesNoReconnect = true;
            }
        }
    }

    if(pass1 != "" && pass1 != "*" && pass1 == pass2)
    {
        if(_preferences->getString(preference_cred_password, "") != pass1)
        {
            _preferences->putString(preference_cred_password, pass1);
            Log->print("Setting changed: ");
            Log->println("CREDPASS");
            state.restartServicesNoReconnect = true;
            state.clearSession = true;
        }
    }

    if(clearCredentials)
    {
        if(_preferences->getString(preference_cred_user, "") != "")
        {
            _preferences->putString(preference_cred_user, "");
            Log->print("Setting changed: ");
            Log->println("CREDUSER");
            state.restartServicesNoReconnect = true;
            state.clearSession = true;
        }
        if(_preferences->getString(preference_cred_password, "") != "")
        {
            _preferences->putString(preference_cred_password, "");
            Log->print("Setting changed: ");
            Log->println("CREDPASS");
            state.restartServicesNoReconnect = true;
            state.clearSession = true;
        }
    }
}

#endif
