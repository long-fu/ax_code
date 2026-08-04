# File Naming — Google C++ Style Compliance Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rename all project source/header files to lowercase with underscores per Google C++ Style Guide.

**Architecture:** Pure mechanical rename — `git mv` each file, then `sed` all `#include` directives and CMakeLists.txt references. Build verification after each batch.

**Tech Stack:** bash, git mv, sed, cmake --build

## Global Constraints

- File names must be all lowercase with underscores (`_`) per Google C++ Style
- Every `#include` directive must be updated to match the new filename
- CMakeLists.txt `GLOB_RECURSE` or explicit file lists must be updated
- Build must pass after each batch of renames
- Cross-compiler: `aarch64-linux-gnu-gcc/g++`

---
