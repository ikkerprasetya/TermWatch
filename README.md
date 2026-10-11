# 🖥️ TermWatch (Underwork)

<p align="center">
  <img src="https://img.shields.io/badge/Language-C99-00599C?style=for-the-badge&logo=c&logoColor=white" alt="C99">
  <img src="https://img.shields.io/badge/Platform-Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black" alt="Linux">
  <img src="https://img.shields.io/badge/Interface-Native_TUI-008080?style=for-the-badge" alt="Native TUI">
</p>

<p align="center">
  A lightweight, real-time system monitor and process viewer written in <b>pure C</b>. Monitor system resources, browse running processes, and inspect process information through an interactive, keyboard-driven claude code-like terminal interface.
</p>

---

## 🔍 Overview

**TermWatch** is a native terminal system monitor designed to provide real-time Linux system statistics and an interactive view of running processes in one place.

Instead of relying on external monitoring commands or a graphical interface, TermWatch aims to collect system information directly from Linux interfaces such as `/proc`, allowing users to monitor CPU and memory usage, navigate processes, and inspect process details without leaving the terminal.

The goal is to create a fast, lightweight, and practical Linux utility inspired by tools like `htop`, built from the ground up in C.

## ✨ Features

| Category | Detail |
|---|---|
| 🖥️ **System Monitoring** | Real-time CPU and memory usage, system uptime, and basic system information |
| 📊 **Resource Visualization** | Live resource statistics with readable terminal-based usage indicators |
| 📋 **Process Viewer** | Browse running processes and view details such as PID, process name, CPU usage, and memory usage |
| 🔎 **Process Search** | Find specific processes by name or PID |
| ↕️ **Process Sorting** | Sort processes by PID, CPU usage, or memory usage |
| ⌨️ **Interactive Navigation** | Navigate the interface and process list using keyboard controls |
| 🔄 **Live Updates** | Automatically refresh system statistics and process information |
| 🪶 **Lightweight Design** | Native terminal interface written in C, without requiring a graphical desktop |
| 🧹 **Graceful Exit** | Restore terminal state when exiting the application |
