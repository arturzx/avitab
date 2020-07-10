/*
 *   AviTab - Aviator's Virtual Tablet
 *   Copyright (C) 2018 Folke Will <folko@solhost.org>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Affero General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Affero General Public License for more details.
 *
 *   You should have received a copy of the GNU Affero General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifdef _WIN32
#   define WIN32_LEAN_AND_MEAN
#   define _WIN32_WINNT 0x0600
#   include <windows.h>
#   include <shellapi.h>
#   define realpath(N, R) _fullpath((R), (N), AVITAB_PATH_LEN_MAX)
#else
#   include <unistd.h>
#   include <uuid/uuid.h>
#endif

#include <ctime>
#include <dirent.h>
#include <sys/stat.h>
#include <cstdlib>
#include <climits>
#include <libgen.h>
#include <algorithm>
#include <regex>
#include <fstream>
#include <algorithm>
#include "Platform.h"
#include "src/Logger.h"

/*
 * The purpose of this module is to put all platform (as in Posix or Win32)
 * specific functions into a single module so that the rest of the application
 * can stay 'clean'. It's mainly about file system related stuff: Windows
 * returns ANSI strings when using the POSIX wrapper for file system access,
 * but most libraries (such as mupdf) expect UTF8 strings when passing file
 * names.
 *
 * The idea is to use UTF8 in AviTab as well, so these wrapper functions
 * should always accept and return UTF8 strings.
 */

// The maximum length that WE support
#define AVITAB_PATH_LEN_MAX 2048

namespace platform {

platform::Platform getPlatform() {
#ifdef _WIN32
    return Platform::WINDOWS;
#elif defined __linux__
    return Platform::LINUX;
#elif defined __APPLE__
    return Platform::MAC;
#else
#   error "Unknown platform"
#endif
}

#ifdef _WIN32
int64_t measureTime() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

int getElapsedMillis(int64_t startAt)  {
    LARGE_INTEGER endAt, frq;
    QueryPerformanceCounter(&endAt);
    QueryPerformanceFrequency(&frq);

    int64_t elapsed = endAt.QuadPart - startAt;

    elapsed *= 1000;
    elapsed /= frq.QuadPart;

    return elapsed;
}

#else
std::chrono::time_point<std::chrono::steady_clock> measureTime() {
    return std::chrono::steady_clock::now();
}

int getElapsedMillis(std::chrono::time_point<std::chrono::steady_clock> startAt) {
    auto endAt = measureTime();
    return std::chrono::duration_cast<std::chrono::milliseconds>(endAt - startAt).count();
}
#endif

constexpr size_t getMaxPathLen() {
    return AVITAB_PATH_LEN_MAX;
}

#ifdef _WIN32
std::string nativeToUTF8(const std::string& native) {
    wchar_t buf[AVITAB_PATH_LEN_MAX];
    char res[AVITAB_PATH_LEN_MAX];

    MultiByteToWideChar(CP_ACP, 0, native.c_str(), -1, buf, sizeof(buf));
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, res, sizeof(res), nullptr, nullptr);
    return res;
}
#else
std::string nativeToUTF8(const std::string& native) {
    return native;
}
#endif

#ifdef _WIN32
std::string UTF8ToNative(const std::string& utf8) {
    wchar_t buf[AVITAB_PATH_LEN_MAX];
    char res[AVITAB_PATH_LEN_MAX];

    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, buf, sizeof(buf));
    WideCharToMultiByte(CP_ACP, 0, buf, -1, res, sizeof(res), nullptr, nullptr);
    return res;
}
#else
std::string UTF8ToNative(const std::string& utf8) {
    return utf8;
}
#endif

#ifdef _WIN32
std::string getProgramPath() {
    char buf[AVITAB_PATH_LEN_MAX];
    DWORD size = AVITAB_PATH_LEN_MAX;
    HANDLE proc = GetCurrentProcess();
    QueryFullProcessImageNameA(proc, 0, buf, &size);

    std::string path = nativeToUTF8(std::string(buf));
    return getDirNameFromPath(path) + "/";
}
#else
std::string getProgramPath() {
    char buf[AVITAB_PATH_LEN_MAX];
    if (!getcwd(buf, sizeof(buf))) {
        throw std::runtime_error("Couldn't get current directory");
    }
    std::string path = nativeToUTF8(buf) + "/";
    return path;
}
#endif

std::vector<DirEntry> readDirectory(const std::string& utf8Path) {
    std::vector<DirEntry> entries;

    std::string path = UTF8ToNative(utf8Path);

    DIR *dir = opendir(path.c_str());
    if (!dir) {
        logger::verbose("Couldn't open directory '%s'", path.c_str());
        return entries;
    }

    struct dirent *dirEntry;

    while ((dirEntry = readdir(dir)) != nullptr) {
        std::string name = dirEntry->d_name;

        if (name.empty() || name[0] == '.') {
            continue;
        }

        std::string filePath = path + name;

        struct stat fileStat;
        if (stat(filePath.c_str(), &fileStat) != 0) {
            logger::verbose("Couldn't stat '%s'", filePath.c_str());
            continue;
        }

        DirEntry entry;
        entry.utf8Name = nativeToUTF8(name);
        entry.isDirectory = S_ISDIR(fileStat.st_mode);
        entries.push_back(entry);
    }
    closedir(dir);

    return entries;
}

std::string realPath(const std::string& utf8Path) {
    std::string nativePath = UTF8ToNative(utf8Path);
    char *res = realpath(nativePath.c_str(), nullptr);
    if (!res) {
        throw std::runtime_error("realpath failed");
    }
    std::string resStr(res);
    free(res);
    return resStr;
}

std::string getFileNameFromPath(const std::string& utf8Path) {
    std::string nativePath = UTF8ToNative(utf8Path);
    std::string base = basename(&nativePath[0]);
    return nativeToUTF8(base);
}

std::string getDirNameFromPath(const std::string& utf8Path) {
    std::string nativePath = UTF8ToNative(utf8Path);
    std::string dir = dirname(&nativePath[0]);
    return nativeToUTF8(dir);
}

bool fileExists(const std::string& utf8Path) {
    std::string nativePath = UTF8ToNative(utf8Path);
    struct stat fileStat;
    return (stat(nativePath.c_str(), &fileStat) == 0);
}

void mkdir(const std::string& utf8Path) {
    std::string nativePath = UTF8ToNative(utf8Path);
#ifdef _WIN32
    (void) ::mkdir(utf8Path.c_str());
#else
    (void) ::mkdir(utf8Path.c_str(), S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif
}

void mkpath(const std::string& utf8Path) {
    std::string path = utf8Path;
    std::replace(path.begin(), path.end(), '\\', '/');

    for (std::string::iterator iter = path.begin(); iter != path.end(); ) {
        std::string::iterator newIter = std::find(iter, path.end(), '/');
        std::string newPath = std::string(path.begin(), newIter);

        if (!fileExists(newPath + "/")) {
            mkdir(newPath);
        }

        iter = newIter;
        if (newIter != path.end()) {
            ++iter;
        }
    }
}

void removeFile(const std::string& utf8Path) {
    std::string nativePath = UTF8ToNative(utf8Path);
    (void) ::remove(nativePath.c_str());
}

std::string readFully(const std::string &utf8Path) {
    std::string nativePath = UTF8ToNative(utf8Path);
    std::ifstream fileStream(nativePath);
    if (!fileStream) {
        throw std::runtime_error("Could not read file string");
    }
    std::string data((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    return data;
}

std::string getLocalTime(const std::string &format) {
    time_t now = time(nullptr);
    tm *local = localtime(&now);

    char buf[16];
    strftime(buf, sizeof(buf), format.c_str(), local);
    return buf;
}

#ifdef _WIN32
std::string getClipboardContent() {
    if (!OpenClipboard(NULL)) {
        return "No clipboard access";
    }

    std::string res;
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data) {
        WCHAR *text = (WCHAR *) GlobalLock(data);
        if (text) {
            size_t utf16len = wcslen(text);
            size_t utf8len = WideCharToMultiByte(CP_UTF8, 0, text, utf16len, nullptr, 0, nullptr, nullptr);
            res.reserve(utf8len + 1);
            WideCharToMultiByte(CP_UTF8, 0, text, utf16len, &res[0], utf8len, nullptr, nullptr);
            res[utf8len] = '\0';
            GlobalUnlock(data);
        }
    }

    CloseClipboard();

    return res;
}
#else
std::string getClipboardContent() {
    return "Not supported on your platform";
}
#endif

std::string formatStringArgs(const std::string format, va_list list) {
    char buf[2048];
    vsprintf(buf, format.c_str(), list);
    return buf;
}

std::string formatString(const std::string format, ...) {
    va_list args;
    va_start(args, format);
    std::string formatted = formatStringArgs(format, args);
    va_end(args);

    return formatted;
}

void controlMediaPlayer(MediaControl ctrl) {
#ifdef _WIN32
    uint8_t key = 0;
    switch (ctrl) {
    case MediaControl::MEDIA_PAUSE: key = VK_MEDIA_PLAY_PAUSE; break;
    case MediaControl::MEDIA_NEXT:  key = VK_MEDIA_NEXT_TRACK; break;
    case MediaControl::MEDIA_PREV:  key = VK_MEDIA_PREV_TRACK; break;
    }
    UINT scanCode = MapVirtualKeyA(key, 0);

    keybd_event(key, scanCode, KEYEVENTF_EXTENDEDKEY, 0);
    keybd_event(key, scanCode, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
#endif
}

std::string lower(const std::string& in) {
    std::string res;
    std::transform(in.begin(), in.end(), std::back_inserter(res), ::tolower);
    return res;
}

std::string upper(const std::string& in) {
    std::string res;
    std::transform(in.begin(), in.end(), std::back_inserter(res), ::toupper);
    return res;
}

std::string getMachineID() {
#ifdef _WIN32
    char buf[255];
    DWORD bufSiz = sizeof(buf);
    RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Cryptography", "MachineGuid", RRF_RT_REG_SZ, nullptr, buf, &bufSiz);
    return std::string(buf);
#elif defined(__APPLE__)
    uuid_t id;
    timespec wait;
    wait.tv_nsec = 0;
    wait.tv_sec = 0;
    gethostuuid(id, &wait);
    char buf[48];
    uuid_unparse(id, buf);
    return std::string(buf);
#else
    std::ifstream idStream("/var/lib/dbus/machine-id");
    std::string id;
    idStream >> id;
    return id;
#endif
}

void openBrowser(const std::string& url) {
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::string cmd = std::string("open \"") + url + "\"";
    system(cmd.c_str());
#else
    std::string browser_cmd = std::string("xdg-open '") + url + "'";
    auto res = system(browser_cmd.c_str());
    if (res != 0) {
        logger::warn("Couldn't launch browser: %d", res);
    }
#endif
}

}
