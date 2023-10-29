This is a WIP testing framework, nothing to see here yet.

### Platform quirks

* **Old Windows** — Colored output requires Windows 10 or newer (to support ANSI escape sequences). Older versions also might have issues with unicode characters. If something doesn't work, run with `--no-color --no-unicode`.

* **MSYS2** — The default font in MinTTY (`Lucida Console`) seems to render box drawing characters poorly. Switch to something like `Cascadia Mono` for better results.

* **Wine** — Wine can't handle unicode and ANSI escape sequences. Piping output through `| cat` seems to fix that, or run with `--no-color --no-unicode`.

* **MacOS** — Debugger detection code is missing. If you want to break on failure, you need to pass `--break` manually.

   ✨ Help wanted! If you have a Mac and want to fix this, be my guest.
