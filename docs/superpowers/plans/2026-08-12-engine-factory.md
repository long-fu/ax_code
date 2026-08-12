# Engine Factory Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** YAML-driven factory creating Yolov5/Scrfd Engine for InfProcess.

**Architecture:** yaml-cpp LoadFile → fill Yolov5Config/ScrfdConfig → `make_unique` subclass; InfProcess Init calls factory.

**Tech Stack:** C++17, yaml-cpp, existing Engine/Yolov5/Scrfd

## Global Constraints

- Spec: `docs/superpowers/specs/2026-08-12-engine-factory-design.md`
- Use `third-party/yaml-cpp` (link in CMakeLists)
- `aux_source_directory(nodes)` picks up new cpp

---

### Task 1: Sample configs

- [x] Add `configs/yolov5.yaml`, `configs/scrfd.yaml`

### Task 2: Factory + parser

- [x] Add `nodes/engine_factory.h/.cpp`

### Task 3: Wire InfProcess + main_rtsp

- [x] InfProcess stores path, Init uses factory
- [x] main_rtsp passes config path

### Task 4: Build

- [x] Reconfigure + build Release/Debug as used by project
