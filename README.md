# RedPanda C++

> [!NOTE]
> This is a personal-use build with experimental WakaTime integration.

Red Panda C++ (Old name: Red Panda Dev-C++ 7) is an fast ,lightweight, open source, and cross platform C/C++/GNU Assembly IDE.

Upstream Simplified Chinese Website: [http://royqh.net/redpandacpp](http://royqh.net/redpandacpp)

Upstream English Website: [https://sourceforge.net/projects/redpanda-cpp](https://sourceforge.net/projects/redpanda-cpp)

[Donate to Upstream project](https://ko-fi.com/royqh1979)

## WakaTime Integration

![WakaTime settings](docs/images/wakatime.png)

### Setup

1. Install `wakatime-cli` and locate its executable.
2. Open **Options > Tools > WakaTime**.
3. Enable WakaTime, select the CLI executable, and enter your API URL and API key.
4. Click **Test** to verify that the CLI can run.
5. Click **Apply** or **OK** to save the settings.

The default API URL is `https://api.wakatime.com/api/v1`. Enable **API URLs** to
route matching files to different endpoints or accounts using regular-expression
rules.

**New Features (Compared with Red Panda Dev-C++ 6):**

* WakaTime integration for tracking coding activity
* Cross Platform (Windows/Linux/MacOS)
* Problem Set (run and test program against predefined input / expected output data)
* Competitive Companion support ( It's an chrome/firefox extension that can fetch problems from OJ websites)
* Edit/compile/run/debug Assembly language programs ( GNU Assember / NASM ).
* Find symbol occurrences
* Memory View for debugging
* TODO View
* Support SDCC Compiler

**UI Improvements:**

* Full high-dpi support, including fonts and icons
* Better dark theme support
* Better editor color scheme support
* Redesigned Find/Replace in Files UI
* Redesigned bookmark UI

**Editing Improvements:**

* Enhanced auto indent
* Enhanced code completion
* Better code folding support

**Debugging Improvements:**

* Use gdb/mi interface
* Enhanced watch
* gdbserver mode

**Code Intellisense Improvements:**

* Better support identifiers for complex expressions
* Support UTF-8 identifiers
* Support C++ 14 using type alias
* Support C-Style enum variable definitions
* Support MACRO with arguments
* Support C++ lambdas

And many other improvements and bug fixes. See NEWS.md for full information.

## Acknowledgement

- [Lua](https://www.lua.org/) 5.4.6 ([source mirror](https://github.com/lua/lua/tree/v5.4.6)) is used as add-on runtime.
- Original project [royqh1979/RedPanda-CPP](https://github.com/royqh1979/RedPanda-CPP) by royqh1979
