#include "helix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/statvfs.h>

#define BOX_WIDTH 44        /* printable columns between the two borders */
#define BAR_CELLS 10

#define CYAN   "\033[36m"
#define YELLOW "\033[33m"
#define BOLD   "\033[1m"
#define RESET  "\033[0m"

/* MemTotal / MemAvailable, in kB, straight out of /proc/meminfo. */
static double get_mem_usage(double *total_gb)
{
    FILE *f = fopen("/proc/meminfo", "r");

    if (f == NULL)
        return 0.0;

    char line[256];
    double total = 0, available = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        sscanf(line, "MemTotal: %lf kB", &total);
        sscanf(line, "MemAvailable: %lf kB", &available);
    }

    fclose(f);

    if (total <= 0)
        return 0.0;

    if (total_gb != NULL)
        *total_gb = total / 1024.0 / 1024.0;

    return (total - available) / total * 100.0;
}

static int read_cpu_ticks(double *total, double *idle)
{
    FILE *f = fopen("/proc/stat", "r");

    if (f == NULL)
        return -1;

    double user, nice, system, idle_ticks, iowait, irq, softirq;

    int got = fscanf(f, "cpu %lf %lf %lf %lf %lf %lf %lf",
                     &user, &nice, &system, &idle_ticks,
                     &iowait, &irq, &softirq);

    fclose(f);

    if (got != 7)
        return -1;

    *idle  = idle_ticks + iowait;
    *total = user + nice + system + idle_ticks + iowait + irq + softirq;

    return 0;
}

/* /proc/stat counts ticks since boot, so usage needs two samples. */
static double get_cpu_usage(void)
{
    double total1, idle1, total2, idle2;

    if (read_cpu_ticks(&total1, &idle1) == -1)
        return 0.0;

    usleep(100000);

    if (read_cpu_ticks(&total2, &idle2) == -1)
        return 0.0;

    double total_delta = total2 - total1;
    double idle_delta  = idle2 - idle1;

    if (total_delta <= 0)
        return 0.0;

    return (total_delta - idle_delta) / total_delta * 100.0;
}

static double get_uptime_seconds(void)
{
    FILE *f = fopen("/proc/uptime", "r");

    if (f == NULL)
        return 0.0;

    double seconds = 0;

    if (fscanf(f, "%lf", &seconds) != 1)
        seconds = 0;

    fclose(f);

    return seconds;
}

static double get_disk_usage(void)
{
    struct statvfs fs;

    if (statvfs("/", &fs) == -1)
        return 0.0;

    double total = (double)fs.f_blocks;
    double free_blocks = (double)fs.f_bfree;

    if (total <= 0)
        return 0.0;

    return (total - free_blocks) / total * 100.0;
}

static void make_bar(char *out, size_t size, double percent)
{
    int filled = (int)(percent / 100.0 * BAR_CELLS + 0.5);

    if (filled < 0)
        filled = 0;

    if (filled > BAR_CELLS)
        filled = BAR_CELLS;

    out[0] = '\0';

    size_t len = 0;

    for (int i = 0; i < BAR_CELLS; i++)
        len += snprintf(out + len, size - len, "%s",
                        i < filled ? "█" : "░");
}

/* One boxed line, padded so the right border always lands in the same column. */
static void row(const char *fmt, ...)
{
    char text[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);

    int pad = BOX_WIDTH - (int)display_width(text);

    if (pad < 0)
        pad = 0;

    printf("║ %s%*s ║\n", text, pad, "");
}

static void rule(const char *left, const char *right)
{
    printf("%s", left);

    for (int i = 0; i < BOX_WIDTH + 2; i++)
        printf("═");

    printf("%s\n", right);
}

void render_dashboard(void)
{
    double mem_total = 0;
    double mem = get_mem_usage(&mem_total);
    double cpu = get_cpu_usage();
    double disk = get_disk_usage();
    double uptime = get_uptime_seconds();

    char cpu_bar[64], mem_bar[64], disk_bar[64];

    make_bar(cpu_bar, sizeof(cpu_bar), cpu);
    make_bar(mem_bar, sizeof(mem_bar), mem);
    make_bar(disk_bar, sizeof(disk_bar), disk);

    int days  = (int)(uptime / 86400);
    int hours = (int)(uptime / 3600) % 24;
    int mins  = (int)(uptime / 60) % 60;

    const char *user = getenv("USER");

    rule("╔", "╗");
    row(BOLD "helix v" HELIX_VERSION RESET "   a shell of your own");
    rule("╠", "╣");
    row(CYAN "CPU " RESET "  %s  %3.0f%%", cpu_bar, cpu);
    row(CYAN "MEM " RESET "  %s  %3.0f%%   %.1f GB", mem_bar, mem, mem_total);
    row(CYAN "DISK" RESET "  %s  %3.0f%%", disk_bar, disk);
    rule("╠", "╣");
    row(YELLOW "Uptime" RESET "  %dd %dh %dm", days, hours, mins);
    row(YELLOW "User" RESET "    %s", user ? user : "unknown");
    rule("╚", "╝");
}
