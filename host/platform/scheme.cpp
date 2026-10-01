#include "platform/scheme.h"

#include <string>

#include "include/cef_parser.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "platform/ipc.h"

namespace mockterm::platform {

namespace {

constexpr char kNotFound[] = "404 Not Found";

class AppSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  explicit AppSchemeHandlerFactory(std::filesystem::path root) : root_(std::move(root)) {}

  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
                                       const CefString&, CefRefPtr<CefRequest> request) override {
    CefURLParts parts;
    if (!CefParseURL(request->GetURL(), parts)) return NotFound();
    std::string path = CefString(&parts.path).ToString();
    std::string decoded = CefURIDecode(path, true, UU_SPACES).ToString();
    while (!decoded.empty() && decoded.front() == '/') decoded.erase(0, 1);
    if (decoded.empty() || decoded.back() == '/') decoded += "index.html";

    std::filesystem::path rel = std::filesystem::u8path(decoded).lexically_normal();
    for (const auto& part : rel) {
      if (part == "..") return NotFound();
    }
    std::filesystem::path file = root_ / rel;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(file, ec)) return NotFound();

    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForFile(file.wstring());
    if (!stream) return NotFound();
    std::string ext = file.extension().string();
    if (!ext.empty()) ext.erase(0, 1);
    std::string mime = CefGetMimeType(ext).ToString();
    if (mime.empty()) mime = "application/octet-stream";
    CefResponse::HeaderMap headers;
    headers.insert({"Cache-Control", "no-cache"});
    return new CefStreamResourceHandler(200, "OK", mime, headers, stream);
  }

 private:
  static CefRefPtr<CefResourceHandler> NotFound() {
    CefRefPtr<CefStreamReader> stream =
        CefStreamReader::CreateForData(const_cast<char*>(kNotFound), sizeof(kNotFound) - 1);
    return new CefStreamResourceHandler(404, "Not Found", "text/plain", {}, stream);
  }

  std::filesystem::path root_;
  IMPLEMENT_REFCOUNTING(AppSchemeHandlerFactory);
};

}  // namespace

void RegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
  registrar->AddCustomScheme(ipc::kScheme, CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE |
                                               CEF_SCHEME_OPTION_CORS_ENABLED |
                                               CEF_SCHEME_OPTION_FETCH_ENABLED);
}

void RegisterAppSchemeHandler(const std::filesystem::path& apps_dir) {
  CefRegisterSchemeHandlerFactory(ipc::kScheme, ipc::kAppsHost, new AppSchemeHandlerFactory(apps_dir));
}

}  // namespace mockterm::platform
