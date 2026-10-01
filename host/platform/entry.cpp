#include "platform/entry.h"

#include <filesystem>
#include <string>

#include "include/cef_app.h"
#include "include/cef_command_line.h"
#include "platform/app.h"

namespace mockterm::platform {

namespace {

std::filesystem::path ExeDir() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  return std::filesystem::path(std::wstring(buf, n)).parent_path();
}

std::filesystem::path CacheRoot() {
  wchar_t buf[MAX_PATH];
  DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
  std::filesystem::path base = n > 0 && n < MAX_PATH ? std::filesystem::path(buf) : ExeDir();
  return base / L"MockTerm";
}

}  // namespace

int RunMain(HINSTANCE instance) {
  CefMainArgs main_args(instance);

  CefRefPtr<CefCommandLine> cmd = CefCommandLine::CreateCommandLine();
  cmd->InitFromString(::GetCommandLineW());

  HostOptions options;
  options.apps_dir = cmd->HasSwitch("apps-dir") ? std::filesystem::path(cmd->GetSwitchValue("apps-dir").ToWString())
                                                : ExeDir() / L"apps";
  if (cmd->HasSwitch("url")) options.start_url = cmd->GetSwitchValue("url").ToString();
  if (cmd->HasSwitch("comms-port")) options.comms_port = std::stoi(cmd->GetSwitchValue("comms-port").ToString());
  if (cmd->HasSwitch("no-spawn-comms")) options.spawn_comms = false;
  if (cmd->HasSwitch("server-port")) {
    // Lets tests run a private mtcomm/mtserver pair alongside a normal instance.
    options.comms_args = {"--upstream-port", cmd->GetSwitchValue("server-port").ToString()};
  }
  options.log_dir = CacheRoot();

  CefRefPtr<MockTermApp> app = new MockTermApp(options);

  // Renderer/GPU/utility sub-processes re-enter here and return immediately.
  const int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) return exit_code;

  CefSettings settings;
  settings.no_sandbox = true;
  settings.background_color = CefColorSetARGB(255, 0, 0, 0);
  settings.log_severity = LOGSEVERITY_WARNING;
  std::filesystem::path root = CacheRoot();
  std::error_code ec;
  std::filesystem::create_directories(root / L"cache", ec);
  CefString(&settings.root_cache_path) = root.wstring();
  CefString(&settings.cache_path) = (root / L"cache").wstring();
  CefString(&settings.log_file) = (root / L"mockterm.log").wstring();
  if (cmd->HasSwitch("remote-debugging-port")) {
    settings.remote_debugging_port = std::stoi(cmd->GetSwitchValue("remote-debugging-port").ToString());
  }

  if (!CefInitialize(main_args, settings, app, nullptr)) return CefGetExitCode();

  CefRunMessageLoop();
  app->Shutdown();
  CefShutdown();
  return 0;
}

}  // namespace mockterm::platform
