#pragma once

#ifndef __LAYRZ_WIFI_PLUGIN_UI_THREAD__
#define __LAYRZ_WIFI_PLUGIN_UI_THREAD__

#include <windows.h>

#include <flutter/plugin_registrar_windows.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <mutex>
#include <list>

class LayrzWifiPluginUiThreadHandler
{
public:

    explicit LayrzWifiPluginUiThreadHandler(flutter::PluginRegistrarWindows *registrar)
      : registrar_(registrar)
    {
      windowProcId_ = registrar_->RegisterTopLevelWindowProcDelegate(
        [this](HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
        {
            return HandleWindowMessage(hwnd, message, wparam, lparam);
        });
    }

    ~LayrzWifiPluginUiThreadHandler()
    {
      registrar_->UnregisterTopLevelWindowProcDelegate(windowProcId_);
    }

    LayrzWifiPluginUiThreadHandler(const LayrzWifiPluginUiThreadHandler &) = delete;
    LayrzWifiPluginUiThreadHandler &operator=(const LayrzWifiPluginUiThreadHandler &) = delete;

    void Post(std::function<void()> &&func)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queuedFuncs_.emplace_back(std::move(func));
      Notify();
    }

private:

    static const UINT kWmCallQueuedFunctions = WM_APP + 0x1d8;

    void Notify()
    {
        if (hwnd_ != 0)
        {
            PostMessage(hwnd_, kWmCallQueuedFunctions, 0, reinterpret_cast<LPARAM>(this));
        }
    }

    std::optional<LRESULT> HandleWindowMessage(
        HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (hwnd_ == 0)
        {
          hwnd_ = hwnd;
          Notify();
        }
        if (message == kWmCallQueuedFunctions && lparam == reinterpret_cast<LPARAM>(this))
        {
          std::list<std::function<void()>> queuedFuncs;
          {
            std::lock_guard<std::mutex> lock(mutex_);
            std::swap(queuedFuncs_, queuedFuncs);
          }
          for (auto &func : queuedFuncs)
          {
            func();
          }
        }
        return std::nullopt;
    }

    flutter::PluginRegistrarWindows *registrar_;
    int windowProcId_ = 0;
    HWND hwnd_ = 0;
    std::list<std::function<void()>> queuedFuncs_;
    std::mutex mutex_;
};

#endif // __LAYRZ_WIFI_PLUGIN_UI_THREAD__
