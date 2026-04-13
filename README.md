# 🧠 Microkernel OS Simulation (C)

A simulation of a microkernel-based operating system using C. The system consists of independent modules (process manager, file system, device driver) communicating via pipes.

## 🚀 Features
- Process creation using fork()
- File system simulation (read/write)
- Device input/output simulation
- IPC using pipes
- Modular kernel architecture

## 🛠️ Tech Used
- C Programming
- UNIX system calls (fork, pipe, read, write)
- IPC (pipes)

## ▶️ Run

gcc microkernel.c -o microkernel
./microkernel

## 📁 Note
Ensure `test.txt` is present in the same directory.
