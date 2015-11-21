#include <picotorrent/app/controllers/application_update_controller.hpp>

#include <picotorrent/picojson.hpp>
#include <picotorrent/common/string_operations.hpp>
#include <picotorrent/common/version_info.hpp>
#include <picotorrent/config/configuration.hpp>
#include <picotorrent/logging/log.hpp>
#include <picotorrent/net/http_client.hpp>
#include <picotorrent/net/http_response.hpp>
#include <picotorrent/net/uri.hpp>
#include <picotorrent/ui/main_window.hpp>
#include <picotorrent/ui/task_dialog.hpp>
#include <semver.hpp>
#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>

using namespace picotorrent::logging;

using picotorrent::app::controllers::application_update_controller;
using picotorrent::common::to_wstring;
using picotorrent::common::version_info;
using picotorrent::config::configuration;
using picotorrent::net::http_client;
using picotorrent::net::http_response;
using picotorrent::net::uri;
using picotorrent::ui::main_window;

application_update_controller::application_update_controller(
    const std::shared_ptr<main_window> &wnd)
    : http_(std::make_shared<http_client>()),
    wnd_(wnd)
{
}

application_update_controller::~application_update_controller()
{
}

void application_update_controller::execute()
{
    uri api(L"https://api.github.com/repos/picotorrent/picotorrent/releases/latest");
    http_->get_async(api, std::bind(&application_update_controller::on_response, this, std::placeholders::_1));
}

void application_update_controller::on_response(const http_response &response)
{
    if (!response.is_success_status())
    {
        LOG(error) << "GitHub returned status " << response.status_code();
        return;
    }

    picojson::value v;
    std::string err = picojson::parse(v, response.content());

    if (!err.empty())
    {
        LOG(error) << "Error when parsing release JSON: " << err;
        return;
    }

    picojson::object obj = v.get<picojson::object>();
    std::string version = obj["tag_name"].get<std::string>();
    if (version[0] == 'v') { version = version.substr(1); }

    semver::version parsedVersion(version);
    std::wstring wideVersion = to_wstring(version);

    configuration &cfg = configuration::instance();

    if (cfg.ignored_update() == wideVersion)
    {
        // Just return if we have ignored this update.
        return;
    }

    if (parsedVersion > semver::version(version_info::current_version()))
    {
        TCHAR title[100];
        StringCchPrintf(title, ARRAYSIZE(title), TEXT("PicoTorrent %s available"), to_wstring(version).c_str());

        uri uri(to_wstring(obj["html_url"].get<std::string>()));

        notify(title, uri, wideVersion);
    }
}

void application_update_controller::notify(const std::wstring &title, const uri& uri, const std::wstring &version)
{
    ui::task_dialog dlg;
    dlg.add_button(1000, L"Show on GitHub",
        [this, uri]()
    {
        ShellExecute(
            wnd_->handle(),
            L"open",
            uri.raw().c_str(),
            NULL,
            NULL,
            SW_SHOWNORMAL);

        return false;
    });

    dlg.set_common_buttons(TDCBF_CLOSE_BUTTON);
    dlg.set_content(L"A newer version of PicoTorrent is available.");
    dlg.set_main_icon(TD_INFORMATION_ICON);
    dlg.set_main_instruction(title);
    dlg.set_parent(wnd_->handle());
    dlg.set_title(L"PicoTorrent");
    dlg.set_verification_text(L"Ignore this update.");
    dlg.show();

    if (dlg.is_verification_checked())
    {
        configuration &cfg = configuration::instance();
        cfg.set_ignored_update(version);
    }
}
