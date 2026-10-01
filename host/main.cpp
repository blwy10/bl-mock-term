// MockTerm entry point. All CEF specifics live in platform/.
#include <windows.h>

#include "platform/entry.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
  return mockterm::platform::RunMain(instance);
}
