﻿
#include "electron.h"

#include "lib/native.h"
#include "NodeRegisterHelp.h"

#include <windows.h>

using namespace v8;
using namespace node;

#if USING_VC6RT == 1
void __cdecl operator delete(void * p, unsigned int)
{
    DebugBreak();
    free(p);
}

extern "C" int __security_cookie = 0;
#endif

namespace atom {

struct TaskAsyncData {
	uv_async_t async;
    CoreMainTask call;
    void* data;
    HANDLE event;
    void* ret;
	uv_mutex_t mutex;
    uv_thread_t main_thread_id;
};
TaskAsyncData mainAsync;
uv_timer_t gcTimer;

static void mainAsyncCallback(uv_async_t* handle) {
    if (mainAsync.call) {
        mainAsync.ret = mainAsync.call(mainAsync.data);
        ::PulseEvent(mainAsync.event);
    }
}

void mainAsyncCall(CoreMainTask call, void* data) {
	uv_mutex_lock(&mainAsync.mutex);
	mainAsync.call = call;
    mainAsync.data = data;
    uv_async_send((uv_async_t*)&mainAsync);
}

void* mainAsyncWait() {
    ::WaitForSingleObject(mainAsync.event, INFINITE);
	uv_mutex_unlock(&mainAsync.mutex);
    return mainAsync.ret;
}

void* mainSyncCall(CoreMainTask call, void* data) {
    mainAsyncCall(call, data);
	void* ret = mainAsyncWait();
    return ret;
}

bool mainSyncCall(v8::FunctionCallback call, const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (uv_thread_self() == mainAsync.main_thread_id)
        return false;
    mainAsyncCall((CoreMainTask)call, (void *)&args);
    mainAsyncWait();
    return true;
}

NODE_MODULE_CONTEXT_AWARE_BUILTIN_SCRIPT_DECLARE_IN_MAIN(atom_browser_web_contents)
NODE_MODULE_CONTEXT_AWARE_BUILTIN_SCRIPT_DECLARE_IN_MAIN(atom_browser_app)
NODE_MODULE_CONTEXT_AWARE_BUILTIN_SCRIPT_DECLARE_IN_MAIN(atom_browser_window)

static void registerNodeMod() {
    NODE_MODULE_CONTEXT_AWARE_BUILTIN_SCRIPT_DEFINDE_IN_MAIN(atom_browser_web_contents);
    NODE_MODULE_CONTEXT_AWARE_BUILTIN_SCRIPT_DEFINDE_IN_MAIN(atom_browser_app);
    NODE_MODULE_CONTEXT_AWARE_BUILTIN_SCRIPT_DEFINDE_IN_MAIN(atom_browser_window);
}

static void gcTimerCallBack(uv_timer_t* handle) {
    v8::Isolate *isolate = (v8::Isolate *)(handle->data);
    if (isolate)
        isolate->LowMemoryNotification();
}
void nodeInitCallBack(NodeArgc* n) {
    gcTimer.data = n->childEnv->isolate();
    uv_timer_init(n->childLoop, &gcTimer);
    uv_timer_start(&gcTimer, gcTimerCallBack, 1000 * 10, 1);
}

} // atom

int APIENTRY wWinMain(HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPTSTR    lpCmdLine,
    int       nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    atom::registerNodeMod();

    wkeInitialize();

    uv_loop_t* loop = uv_default_loop();

    atom::mainAsync.event = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    uv_async_init(loop, (uv_async_t*)&atom::mainAsync, atom::mainAsyncCallback);
	uv_mutex_init(&atom::mainAsync.mutex);
    atom::mainAsync.main_thread_id= uv_thread_self();

    wchar_t* argv1[] = { L"electron.exe", L"init.js" };
    node::NodeArgc* node = node::runNodeThread(2, argv1, atom::nodeInitCallBack, NULL);

    MSG msg;
    bool more;
    while (true) {
        more = (0 != uv_run(loop, UV_RUN_NOWAIT));
        if (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        } else
            break;
    }

	wkeFinalize();
	return msg.message;
}

int main() {
    return wWinMain(0, 0, 0, 0);
}