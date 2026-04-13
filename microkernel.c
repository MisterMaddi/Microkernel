#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>

#define TOTAL_MODULES 3
#define MAX_BUF 256

enum { MOD_PROCESS = 0, MOD_FILES = 1, MOD_DEVICE = 2 };

ssize_t safe_write(int fd, const char *data, size_t size) {
    size_t done = 0;
    while (done < size) {
        ssize_t w = write(fd, data + done, size - done);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        done += w;
    }
    return done;
}

ssize_t get_line(int fd, char *buf, size_t maxlen) {
    size_t i = 0;
    while (i + 1 < maxlen) {
        ssize_t r = read(fd, buf + i, 1);
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        } else if (r == 0) break;
        else {
            if (buf[i] == '\n') {
                i++;
                break;
            }
            i++;
        }
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

void process_module(const char *cmd) {
    if (strncmp(cmd, "create_process", 14) == 0) {
        int n = 0;
        if (sscanf(cmd + 14, "%d", &n) != 1) n = 1;
        if (n <= 0) n = 1;
        printf("[Proc Unit] Launching %d simulated processes...\n", n);
        fflush(stdout);
        for (int i = 1; i <= n; i++) {
            pid_t pid = fork();
            if (pid < 0) {
                perror("[Proc Unit] Fork error");
                continue;
            }
            if (pid == 0) {
                printf("[Proc Unit] Process %d executing...\n", i);
                fflush(stdout);
                usleep(200000 + i * 40000);
                printf("[Proc Unit] Process %d done.\n", i);
                fflush(stdout);
                _exit(0);
            }
        }
        int status;
        while (wait(&status) > 0);
    } else if (strncmp(cmd, "delete_process", 14) == 0) {
        int pid = 0;
        if (sscanf(cmd + 14, "%d", &pid) == 1 && pid > 0)
            printf("[Proc Unit] Simulated termination of process %d.\n", pid);
        else
            printf("[Proc Unit] Invalid delete_process command.\n");
        fflush(stdout);
    } else {
        printf("[Proc Unit] Unknown command: %s\n", cmd);
        fflush(stdout);
    }
}

void file_module(const char *cmd) {
    if (strncmp(cmd, "read_file", 9) == 0) {
        const char *file = "test.txt";
        printf("[File Unit] Reading file: %s\n", file);
        fflush(stdout);
        FILE *fp = fopen(file, "r");
        if (!fp) {
            printf("[File Unit] Could not open %s: %s\n", file, strerror(errno));
            fflush(stdout);
            return;
        }
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            printf("[File Unit] %s", line);
        }
        fclose(fp);
        fflush(stdout);
    } else if (strncmp(cmd, "write_file", 10) == 0) {
        const char *file = "test.txt";
        const char *msg = cmd + 10;
        while (*msg == ' ') msg++;
        if (*msg == '\0') msg = "Appended text from File Subsystem.";
        printf("[File Unit] Appending to file: %s\n", file);
        fflush(stdout);
        FILE *fp = fopen(file, "a");
        if (!fp) {
            printf("[File Unit] Could not open %s: %s\n", file, strerror(errno));
            fflush(stdout);
            return;
        }
        fprintf(fp, "%s\n", msg);
        fclose(fp);
        printf("[File Unit] Write complete.\n");
        fflush(stdout);
    } else {
        printf("[File Unit] Unknown command: %s\n", cmd);
        fflush(stdout);
    }
}

void device_module(const char *cmd) {
    if (strncmp(cmd, "device_input", 12) == 0) {
        printf("[Device Unit] Simulating input from user device...\n");
        fflush(stdout);
        usleep(120000);
        printf("[Device Unit] Input received: Hello World\n");
        fflush(stdout);
    } else if (strncmp(cmd, "device_output", 13) == 0) {
        printf("[Device Unit] Simulating display output...\n");
        fflush(stdout);
        usleep(100000);
        printf("[Device Unit] Output displayed: Welcome message.\n");
        fflush(stdout);
    } else {
        printf("[Device Unit] Unknown command: %s\n", cmd);
        fflush(stdout);
    }
}

void module_loop(int id, int read_end, int write_end) {
    char msg[MAX_BUF];
    while (1) {
        ssize_t n = get_line(read_end, msg, sizeof(msg));
        if (n < 0) break;
        if (n == 0) break;
        size_t len = strlen(msg);
        if (len > 0 && msg[len - 1] == '\n') msg[len - 1] = '\0';
        if (strcmp(msg, "shutdown") == 0) {
            printf("[Module %d] Shutdown acknowledged.\n", id);
            fflush(stdout);
            break;
        }
        if (id == MOD_PROCESS)
            process_module(msg);
        else if (id == MOD_FILES)
            file_module(msg);
        else if (id == MOD_DEVICE)
            device_module(msg);
        const char *done = "DONE\n";
        if (safe_write(write_end, done, strlen(done)) < 0) break;
    }
    close(read_end);
    close(write_end);
    _exit(0);
}

int main() {
    int parent_to_child[TOTAL_MODULES][2];
    int child_to_parent[TOTAL_MODULES][2];
    pid_t child_pids[TOTAL_MODULES];

    for (int i = 0; i < TOTAL_MODULES; i++) {
        if (pipe(parent_to_child[i]) < 0 || pipe(child_to_parent[i]) < 0) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < TOTAL_MODULES; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pid == 0) {
            for (int j = 0; j < TOTAL_MODULES; j++) {
                if (j != i) {
                    close(parent_to_child[j][0]);
                    close(parent_to_child[j][1]);
                    close(child_to_parent[j][0]);
                    close(child_to_parent[j][1]);
                }
            }
            close(parent_to_child[i][1]);
            close(child_to_parent[i][0]);
            module_loop(i, parent_to_child[i][0], child_to_parent[i][1]);
        } else {
            child_pids[i] = pid;
            close(parent_to_child[i][0]);
            close(child_to_parent[i][1]);
        }
    }

    printf("[Kernel] Dispatching tasks to all modules...\n");
    fflush(stdout);

    const char *cmd1 = "create_process 3\n";
    const char *cmd2 = "read_file\n";
    const char *cmd3 = "device_input\n";

    safe_write(parent_to_child[MOD_PROCESS][1], cmd1, strlen(cmd1));
    safe_write(parent_to_child[MOD_FILES][1], cmd2, strlen(cmd2));
    safe_write(parent_to_child[MOD_DEVICE][1], cmd3, strlen(cmd3));

    fd_set set;
    char buffer[MAX_BUF];
    int finished = 0;
    int maxfd = -1;

    for (int i = 0; i < TOTAL_MODULES; i++)
        if (child_to_parent[i][0] > maxfd) maxfd = child_to_parent[i][0];

    while (finished < TOTAL_MODULES) {
        FD_ZERO(&set);
        for (int i = 0; i < TOTAL_MODULES; i++)
            FD_SET(child_to_parent[i][0], &set);

        int ready = select(maxfd + 1, &set, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        for (int i = 0; i < TOTAL_MODULES; i++) {
            if (FD_ISSET(child_to_parent[i][0], &set)) {
                ssize_t r = get_line(child_to_parent[i][0], buffer, sizeof(buffer));
                if (r <= 0) continue;
                if (buffer[strlen(buffer) - 1] == '\n')
                    buffer[strlen(buffer) - 1] = '\0';
                if (strcmp(buffer, "DONE") == 0)
                    finished++;
                else {
                    printf("[Kernel] Message from module %d: %s\n", i, buffer);
                    fflush(stdout);
                }
            }
        }
    }

    const char *stop = "shutdown\n";
    for (int i = 0; i < TOTAL_MODULES; i++) {
        safe_write(parent_to_child[i][1], stop, strlen(stop));
        close(parent_to_child[i][1]);
        close(child_to_parent[i][0]);
    }

    for (int i = 0; i < TOTAL_MODULES; i++) {
        int status;
        waitpid(child_pids[i], &status, 0);
    }

    printf("[Kernel] All modules exited successfully.\n");
    fflush(stdout);
    return 0;
}

