#!/usr/bin/env python3
"""Linux GUI regression: build Guitar first; requires Xvfb and xdotool.

Runs exclusively against disposable local repositories and a private app config.
The Git wrapper records actual invocations, detecting redundant history reloads.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, default=Path(__file__).resolve().parents[2] / "_bin/Guitar")
    args = parser.parse_args()
    for tool in ("git", "Xvfb", "xdotool"):
        if not shutil.which(tool):
            parser.error(f"Required executable is missing: {tool}")
    git_program = shutil.which("git")
    app = server = None
    with tempfile.TemporaryDirectory(prefix="guitar fetch ui ") as temporary:
        root = Path(temporary)
        env = os.environ.copy()
        env.update(GIT_CONFIG_GLOBAL=str(root / "gitconfig"), GIT_CONFIG_NOSYSTEM="1",
                   XDG_CONFIG_HOME=str(root / "config"), QT_QPA_PLATFORM="xcb")
        (root / "gitconfig").write_text("[user]\nname = Test\nemail = test@example.invalid\n")

        def git(*tail, cwd=None):
            return subprocess.check_output([git_program, *tail], cwd=cwd, env=env, stderr=subprocess.STDOUT)

        def command(*tail):
            return subprocess.check_output(tail, env=env, stderr=subprocess.PIPE).decode().strip()

        source, remote, work = root / "source", root / "remote.git", root / "work"
        source.mkdir()
        git("init", "-q", "--initial-branch=main", str(source))
        for index in range(3):
            (source / "file").write_text(f"content {index}\n")
            git("add", "file", cwd=source)
            git("commit", "-qm", f"Commit {index}", cwd=source)
        git("clone", "-q", "--bare", str(source), str(remote))
        git("clone", "-q", str(remote), str(work))
        git("branch", "older", "HEAD~1", cwd=work)
        git("branch", "same-head", "HEAD", cwd=work)
        selected = git("rev-parse", "HEAD~1", cwd=work).decode().strip()
        log = root / "commands.jsonl"
        wrapper = root / "git-wrapper"
        wrapper.write_text("#!/usr/bin/env python3\nimport json,os,sys\n"
                           f"with open({str(log)!r}, 'a') as f: f.write(json.dumps(sys.argv[1:])+'\\n')\n"
                           f"os.execv({git_program!r}, ['git']+sys.argv[1:])\n")
        wrapper.chmod(0o700)
        config = root / "config/soramimi.jp/Guitar"
        config.mkdir(parents=True)
        (config / "Guitar.ini").write_text(f"[Global]\nGitCommand={wrapper}\n\n"
                                           "[Behavior]\nAutomaticFetch=false\n\n"
                                           "[UI]\nLanguage=en\nShowAvatars=false\n")
        try:
            # Let Xvfb allocate a free display; no shared desktop is touched.
            with (root / "display").open("w") as display, (root / "xvfb.log").open("w") as output:
                server = subprocess.Popen(["Xvfb", "-displayfd", str(display.fileno()), "-screen", "0", "1280x900x24", "-nolisten", "tcp"],
                                          pass_fds=(display.fileno(),), stdout=output, stderr=output)
            deadline = time.monotonic() + 15
            while not (number := (root / "display").read_text().strip()):
                if server.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("Xvfb startup failed")
                time.sleep(.05)
            env["DISPLAY"] = ":" + number
            with (root / "app.log").open("w") as output:
                app = subprocess.Popen([str(args.app.resolve()), str(work)], env=env, stdout=output, stderr=output)
            deadline = time.monotonic() + 20
            while True:
                windows = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(app.pid)], env=env, capture_output=True, text=True).stdout.split()
                if windows:
                    window = windows[-1]
                    break
                if app.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError(f"Guitar window did not open (exit={app.poll()}): " + (root / "app.log").read_text())
                time.sleep(.05)
            command("xdotool", "windowsize", window, "1024", "700")

            def records():
                return [json.loads(line) for line in log.read_text().splitlines()] if log.exists() else []

            def settle(start=0, require_fetch=False):
                deadline = time.monotonic() + 20
                last, stable = -1, time.monotonic()
                while time.monotonic() < deadline:
                    current = records()
                    if len(current) != last:
                        last, stable = len(current), time.monotonic()
                    fetched = not require_fetch or any("fetch" in row for row in current[start:])
                    if fetched and current and time.monotonic() - stable > 1:
                        return current[start:]
                    if app.poll() is not None:
                        raise RuntimeError("Guitar exited during the test")
                    time.sleep(.05)
                raise RuntimeError("Git operations did not settle")

            def click(x, y):
                command("xdotool", "mousemove", "--window", window, str(x), str(y), "click", "1")

            def fetch(name, reloads, online=True):
                start = len(records())
                click(100, 50)
                after = settle(start, require_fetch=online)
                counts = {key: sum(key in row for row in after) for key in ("fetch", "rev-list")}
                assert counts == {"fetch": int(online), "rev-list": reloads}, (name, counts)
                print(name, counts, flush=True)
                return after

            settle()
            click(470, 135)  # Select the middle commit, rather than HEAD.
            settle()
            fetch("unchanged", 0)
            fetch("repeated unchanged", 0)  # PTY completion must allow another fetch.
            start = len(records())
            click(100, 50)
            click(100, 50)
            queued = settle(start, require_fetch=True)
            assert sum("fetch" in row for row in queued) == 2, "Queued fetch was lost"
            assert not any("rev-list" in row for row in queued), "Unchanged queued fetch reloaded history"
            print("two queued fetches completed", flush=True)

            def checkout_case(branch, row_y=115):
                click(470, row_y)  # Select the commit carrying the target branch.
                settle()
                expected = git("rev-parse", branch, cwd=work).decode().strip()
                start = len(records())
                command("xdotool", "windowfocus", window, "key", "alt+r")
                command("xdotool", "key", "o")
                deadline = time.monotonic() + 15
                while True:
                    visible = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(app.pid)], env=env, capture_output=True, text=True).stdout.split()
                    dialogs = [w for w in visible if command("xdotool", "getwindowname", w) == "Checkout"]
                    if dialogs:
                        break
                    if time.monotonic() > deadline:
                        raise RuntimeError("Checkout dialog did not open: " + repr([command("xdotool", "getwindowname", w) for w in visible]))
                    time.sleep(.05)
                # The selected commit has a local branch: its radio has focus.
                # Tab to its combo box and select the uniquely named branch.
                command("xdotool", "windowfocus", dialogs[0], "key", "Tab")
                command("xdotool", "type", "--clearmodifiers", branch)
                command("xdotool", "key", "Return")
                deadline = time.monotonic() + 20
                while git("branch", "--show-current", cwd=work).decode().strip() != branch:
                    if time.monotonic() > deadline:
                        raise RuntimeError("GUI checkout did not complete")
                    time.sleep(.05)
                after = settle(start)
                assert sum("rev-list" in row for row in after) == 1, "Checkout must reload history once"
                assert not any("fetch" in row for row in after), "Checkout triggered a fetch"
                assert any("diff" in row and "--raw" in row and row[-1] == expected for row in after), "Checkout HEAD file list did not refresh"
                assert git("rev-parse", "HEAD", cwd=work).decode().strip() == expected
                print("checkout", branch, "HEAD file list refreshed", flush=True)

            checkout_case("older", 135)
            checkout_case("main")
            checkout_case("same-head")  # The branch changes while the commit ID stays the same.
            checkout_case("main")
            click(470, 135)  # Restore the selection used by the changed-fetch check.
            settle()
            (source / "file").write_text("remote change\n")
            git("add", "file", cwd=source)
            git("commit", "-qm", "New remote commit", cwd=source)
            git("push", str(remote), "HEAD:refs/heads/main", cwd=source)
            after = fetch("changed", 1)
            assert any("diff" in row and "--raw" in row and row[-1] == selected for row in after), "Selected commit was not retained"
            (work / "file").write_text("dirty\n")
            fetch("dirty appeared", 1)
            (work / "file").write_text("still dirty\n")
            fetch("dirty content changed", 0)
            click(962, 38)  # Offline radio button; Fetch becomes Update.
            settle()
            fetch("offline update", 1, online=False)
            click(895, 38)
            settle()
            git("remote", "set-url", "origin", str(root / "missing-remote.git"), cwd=work)
            fetch("failed fetch", 0)
            fetch("repeated failed fetch", 0)

            def commit_case(name, leave_dirty):
                (work / "file").write_text(name + " staged\n")
                git("add", "file", cwd=work)
                if leave_dirty:
                    (work / "file").write_text(name + " unstaged\n")
                before = git("rev-parse", "HEAD", cwd=work).decode().strip()
                start = len(records())
                # Repository > Commit. The editor starts with keyboard focus.
                command("xdotool", "windowfocus", window, "key", "alt+r")
                command("xdotool", "key", "c")
                deadline = time.monotonic() + 15
                while True:
                    visible = subprocess.run(["xdotool", "search", "--onlyvisible", "--pid", str(app.pid)], env=env, capture_output=True, text=True).stdout.split()
                    dialogs = [w for w in visible if command("xdotool", "getwindowname", w) == "Commit"]
                    if dialogs:
                        break
                    if time.monotonic() > deadline:
                        raise RuntimeError("Commit dialog did not open")
                    time.sleep(.05)
                command("xdotool", "windowfocus", dialogs[0], "type", "--clearmodifiers", name)
                command("xdotool", "key", "ctrl+Return")
                deadline = time.monotonic() + 20
                while (head := git("rev-parse", "HEAD", cwd=work).decode().strip()) == before:
                    if time.monotonic() > deadline:
                        raise RuntimeError("GUI commit did not complete")
                    time.sleep(.05)
                after = settle(start)
                assert sum("rev-list" in row for row in after) == 1, "Commit history reloaded repeatedly"
                assert not any("fetch" in row for row in after), "Commit triggered a fetch"
                assert any("diff" in row and "--raw" in row and row[-1] == head for row in after), "HEAD file list did not refresh"
                assert bool(git("status", "--porcelain", cwd=work).strip()) == leave_dirty
                print(name, "HEAD selected and file list refreshed", flush=True)

            commit_case("Commit leaves clean worktree", False)
            commit_case("Commit retains unstaged changes", True)
        finally:
            for process in (app, server):
                if process and process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=10)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


if __name__ == "__main__":
    main()
