This is a WIP testing framework, nothing to see here yet.

---

# 🪢Taut

> adjective<br/>
> &nbsp; 1\. having no give or slack, tightly drawn<br/>
> &nbsp; 2\. kept in proper order or condition<br/>
> &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; — [Merriam-Webster dictionary](https://www.merriam-webster.com/dictionary/taut)

**T**aut is **a** **u**nit **t**est framework[*](https://en.wikipedia.org/wiki/Recursive_acronym) for modern C++. It prioritizes having a clean minimal API (exactly one way of doing things; while not compromising on features) and good diagnostics, and uses a new kind of assertion syntax.

---

### Platform quirks

* **MSYS2** — The default font in MinTTY (`Lucida Console`) seems to render box drawing characters poorly. Switch to something like `Cascadia Mono` for better results.

* **Wine** — Wine can't handle unicode and ANSI escape sequences. Piping output through `| cat` seems to fix that, or run with `--no-color --no-unicode`.

* **MacOS** — Debugger detection code is missing. If you want to break on failure, you need to pass `--break` manually.

   ✨ Help wanted! If you have a Mac and want to fix this, be my guest.

* **Old Windows** — Colored output requires Windows 10 or newer (to support ANSI escape sequences). Older versions also might have issues with unicode characters. If something doesn't work, run with `--no-color --no-unicode`.
