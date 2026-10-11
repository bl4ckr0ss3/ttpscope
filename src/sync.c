#include "ttpscope/sync.h"
#include "ttpscope/paths.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static bool copy_file(const char *source, const char *destination, char *error, size_t error_size) {
    int input = open(source, O_RDONLY);
    if (input < 0) { snprintf(error, error_size, "%s: %s", source, strerror(errno)); return false; }
    char temporary[4096];
    int n = snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", destination, (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(temporary)) { close(input); snprintf(error, error_size, "path is too long"); return false; }
    int output = open(temporary, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output < 0) { snprintf(error, error_size, "%s: %s", temporary, strerror(errno)); close(input); return false; }
    char buffer[65536];
    bool ok = true;
    for (;;) {
        ssize_t got = read(input, buffer, sizeof(buffer));
        if (got == 0) break;
        if (got < 0) { ok = false; break; }
        ssize_t sent = 0;
        while (sent < got) {
            ssize_t written = write(output, buffer + sent, (size_t)(got - sent));
            if (written < 0) { ok = false; break; }
            sent += written;
        }
        if (!ok) break;
    }
    if (close(input) != 0 || fsync(output) != 0 || close(output) != 0) ok = false;
    if (ok && rename(temporary, destination) != 0) ok = false;
    if (!ok) { snprintf(error, error_size, "cannot import dataset: %s", strerror(errno)); unlink(temporary); }
    return ok;
}

bool ttpscope_import(const char *source, char *error, size_t error_size) {
    char dir[4096], destination[4096];
    if (!ttpscope_cache_dir(dir, sizeof(dir)) || !ttpscope_dataset_path(destination, sizeof(destination))) {
        snprintf(error, error_size, "cannot determine cache path"); return false;
    }
    if (!ttpscope_ensure_dir(dir)) { snprintf(error, error_size, "cannot create %s: %s", dir, strerror(errno)); return false; }
    return copy_file(source, destination, error, error_size);
}

bool ttpscope_sync(char *error, size_t error_size) {
    char dir[4096], destination[4096], temporary[4096];
    if (!ttpscope_cache_dir(dir, sizeof(dir)) || !ttpscope_dataset_path(destination, sizeof(destination))) {
        snprintf(error, error_size, "cannot determine cache path"); return false;
    }
    if (!ttpscope_ensure_dir(dir)) { snprintf(error, error_size, "cannot create %s: %s", dir, strerror(errno)); return false; }
    int n = snprintf(temporary, sizeof(temporary), "%s.tmp.%ld", destination, (long)getpid());
    if (n < 0 || (size_t)n >= sizeof(temporary)) { snprintf(error, error_size, "cache path is too long"); return false; }
    pid_t child = fork();
    if (child < 0) { snprintf(error, error_size, "cannot start curl: %s", strerror(errno)); return false; }
    if (child == 0) {
        execlp("curl", "curl", "--fail", "--location", "--retry", "3", "--connect-timeout", "15",
               "--output", temporary, TTPSCOPE_DATA_URL, (char *)NULL);
        _exit(127);
    }
    int status;
    if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        unlink(temporary);
        snprintf(error, error_size, "dataset download failed (curl is required)");
        return false;
    }
    struct stat info;
    if (stat(temporary, &info) != 0 || info.st_size < 1024 || rename(temporary, destination) != 0) {
        unlink(temporary);
        snprintf(error, error_size, "downloaded dataset is invalid or cannot be installed");
        return false;
    }
    return true;
}
