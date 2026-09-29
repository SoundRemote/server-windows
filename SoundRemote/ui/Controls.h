#pragma once

#include <Windows.h>

#include <functional>
#include <string>

struct Rect;

class MuteButton {
public:
	MuteButton(HWND hParent, const Rect& rect, const std::wstring& name);
    void onClick();
    HWND handle() const;
    void setStateCallback(std::function<void(bool)> callback);
private:
    HWND button_ = nullptr;
    HANDLE iconSoundOn_ = nullptr;
    HANDLE iconSoundOff_ = nullptr;
    std::function<void(bool)> stateCallback_ = nullptr;

    bool isMuted() const;
    void updateIcon(bool muted);
};

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    Rect() {}
    Rect(int _x, int _y, int _w, int _h) : x(_x), y(_y), w(_w), h(_h) {}
};
