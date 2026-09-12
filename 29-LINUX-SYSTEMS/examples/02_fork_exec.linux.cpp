// 02_fork_exec.linux.cpp
// ============================================================
// fork() -> child COW address space; exec() -> naya program image;
// wait() -> exit status reap. Zombie aur orphan bhi dikhaya.
// ============================================================
//  LINUX-ONLY (fork/execvp/waitpid, <sys/wait.h>). MinGW pe fork nahi hai.
//  Linux / WSL pe:
//      g++ -std=c++20 -O2 -Wall -Wextra 02_fork_exec.linux.cpp -o fork_exec
//      ./fork_exec
//      strace -f ./fork_exec        # -f: child ko bhi follow karo
// ============================================================

#define _GNU_SOURCE 1   // glibc: expose MAP_POPULATE/ip_mreqn/accept4/recvmmsg/CLOCK_* under strict -std=c++20
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <sys/wait.h>

static long fork_latency_ns() {
    timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    pid_t pid = fork();
    if (pid == 0) {
        _exit(0);                    // child: turant nikal jao (atexit/flush skip)
    }
    clock_gettime(CLOCK_MONOTONIC, &b);
    int st = 0;
    waitpid(pid, &st, 0);
    return (b.tv_sec - a.tv_sec) * 1'000'000'000L + (b.tv_nsec - a.tv_nsec);
}

int main() {
    std::puts("=== 1. fork(): ek call, do return ===");
    pid_t pid = fork();
    if (pid < 0) { std::perror("fork"); return 1; }

    if (pid == 0) {
        // ---- CHILD ----
        std::printf("  [child]  mera pid=%d, parent pid=%d\n", getpid(), getppid());
        // exec: current process image ko REPLACE karo. Success pe yeh call
        // kabhi return nahi karti -- neeche ka code sirf exec fail hone pe chalega.
        char* const argv[] = { const_cast<char*>("echo"),
                               const_cast<char*>("  [child]  ab main /bin/echo hoon"),
                               nullptr };
        execvp("echo", argv);
        std::perror("  [child]  execvp fail");   // yahan aaye = exec fail
        _exit(127);
    }

    // ---- PARENT ----
    std::printf("  [parent] maine child banaya: pid=%d\n", pid);
    int status = 0;
    waitpid(pid, &status, 0);                     // child ko reap karo -> zombie nahi banega
    if (WIFEXITED(status))
        std::printf("  [parent] child exit code = %d\n", WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        std::printf("  [parent] child ko signal %d ne maara\n", WTERMSIG(status));

    std::puts("\n=== 2. fork() ki cost (COW page tables copy) ===");
    long best = 1L << 60;
    for (int i = 0; i < 200; ++i) {
        long t = fork_latency_ns();
        if (t < best) best = t;
    }
    std::printf("  fork()+immediate _exit, best of 200: %ld ns (~%.1f us)\n",
                best, best / 1000.0);
    std::puts("  (memory map jitna bada, page tables utni zyada COW-copy -> fork mehnga.\n"
              "   Isi liye HFT me hot process fork nahi karte; helper processes\n"
              "   startup pe hi bana lete, ya posix_spawn/vfork+exec use karte.)");

    std::puts("\n=== 3. Orphan: parent pehle mar gaya ===");
    pid = fork();
    if (pid == 0) {
        sleep(1);                                 // tab tak parent exit kar chuka hoga
        std::printf("  [orphan] ab mera parent pid=%d (init/systemd ne god liya)\n",
                    getppid());
        _exit(0);
    }
    std::printf("  [parent] main bina wait kiye nikal raha; child orphan ban jayega\n");
    // NOTE: yahan waitpid nahi kiya -> reaping ab init karega.
    _exit(0);
}

/* ============================================================
 * EXPECTED OUTPUT (typical Linux x86-64) -- aapke box pe NAHI napa gaya
 * ------------------------------------------------------------
 * === 1. fork(): ek call, do return ===
 *   [parent] maine child banaya: pid=48213
 *   [child]  mera pid=48213, parent pid=48212
 *   [child]  ab main /bin/echo hoon
 *   [parent] child exit code = 0
 *
 * === 2. fork() ki cost (COW page tables copy) ===
 *   fork()+immediate _exit, best of 200: 42000 ns (~42.0 us)
 *   (chhote process ke liye ~40-90 us; bade RSS wale ke liye ms-scale ho sakta)
 *
 * === 3. Orphan: parent pehle mar gaya ===
 *   [parent] main bina wait kiye nikal raha; child orphan ban jayega
 *   [orphan] ab mera parent pid=1 (init/systemd ne god liya)
 *
 * fork ka number machine/RSS/THP settings pe bahut nirbhar hai. `vfork` +
 * `exec` ya `posix_spawn` chhote hote hain kyunki page tables copy nahi hoti.
 * ============================================================ */
