# commes: Commit Message

`commes` is a small command-line tool that generates Git commit messages with AI.
In practical terms, it is a C++ implementation of the same idea as tools such as `aicommits` or `opencommit`.

The name `commes` stands for `Commit Message`.

## Overview

`commes` was extracted from the Guitar Git client as a focused CLI utility.
It reuses Guitar's existing commit-message generation pipeline and wraps it in a terminal-oriented workflow.

The tool does the following:

1. Checks the current repository for staged changes.
2. Collects the diff for the staged content.
3. Sends that diff to a supported AI provider.
4. Receives multiple commit message candidates.
5. Lets you choose one candidate in an interactive terminal menu.
6. Runs `git commit -m` with the selected message.

This makes `commes` useful when you want a lightweight AI commit assistant without launching the full Guitar GUI.

## Implementation Notes

`commes` itself is intentionally thin.
Most of the AI-specific logic is shared with Guitar and comes from the following reusable components:

- `CommitMessageGenerator`: builds the prompt, submits the request, and parses AI responses.
- `GenerativeAI`: abstracts model/provider definitions and request construction.
- `selectitem`: provides a small terminal UI for choosing one generated message.

Because of that structure, `commes` is best understood as a CLI frontend over Guitar's existing AI commit message engine.

## Current Behavior

- Works on staged changes only.
- Uses the Git command-line tool internally.
- Reads the API key from the environment variable expected by the selected model/provider.
- Presents generated candidates in an interactive terminal selector.
- Commits immediately after a candidate is chosen.

At the moment, the implementation is still experimental and keeps the workflow intentionally simple.

## Build

This project uses qmake and Qt Core.

Example:

```bash
cd extra/commes
qmake6 commes.pro
make
```

The executable is produced as:

```bash
extra/commes/_bin/commes
```

## Usage

Run the tool inside a Git repository after staging your changes:

```bash
commes
```

You can also specify the target repository explicitly:

```bash
commes -C /path/to/repository
```

Typical workflow:

```bash
git add -A
commes
```

## Positioning

If you already know tools like `aicommits` or `opencommit`, `commes` targets the same problem space:
generate commit messages from Git diffs with an AI model, then let the user apply one quickly.

The main difference is that `commes` is implemented in C++ and is built from the same codebase as Guitar.

## Status

`commes` is currently a small, experimental companion project inside the Guitar repository rather than a fully standalone product.
Its value is simplicity: it exposes only the AI commit message flow, without the rest of the GUI client.


