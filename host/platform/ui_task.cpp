#include "platform/ui_task.h"

#include "include/cef_task.h"

namespace mockterm::platform {

namespace {
class FunctionTask : public CefTask {
 public:
  explicit FunctionTask(std::function<void()> fn) : fn_(std::move(fn)) {}
  void Execute() override { fn_(); }

 private:
  std::function<void()> fn_;
  IMPLEMENT_REFCOUNTING(FunctionTask);
};
}  // namespace

void PostUiTask(std::function<void()> fn) { CefPostTask(TID_UI, new FunctionTask(std::move(fn))); }

}  // namespace mockterm::platform
