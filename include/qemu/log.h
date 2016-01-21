#ifndef QEMU_LOG_H
#define QEMU_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include "qemu/compiler.h"
#include "qom/cpu.h"
#ifdef NEED_CPU_H
#include "disas/disas.h"
#endif

/* Private global variables, don't use */
extern FILE *qemu_logfile;
extern int qemu_loglevel;

/* 
 * The new API:
 *
 */

/* Log settings checking macros: */

/* Returns true if qemu_log() will really write somewhere
 */
static inline bool qemu_log_enabled(void)
{
    return qemu_logfile != NULL;
}

/* Returns true if qemu_log() will write somewhere else than stderr
 */
static inline bool qemu_log_separate(void)
{
    return qemu_logfile != NULL && qemu_logfile != stderr;
}

#define CPU_LOG_TB_OUT_ASM (1 << 0)
#define CPU_LOG_TB_IN_ASM  (1 << 1)
#define CPU_LOG_TB_OP      (1 << 2)
#define CPU_LOG_TB_OP_OPT  (1 << 3)
#define CPU_LOG_INT        (1 << 4)
#define CPU_LOG_EXEC       (1 << 5)
#define CPU_LOG_PCALL      (1 << 6)
#define CPU_LOG_TB_CPU     (1 << 8)
#define CPU_LOG_RESET      (1 << 9)
#define LOG_UNIMP          (1 << 10)
#define LOG_GUEST_ERROR    (1 << 11)
#define CPU_LOG_MMU        (1 << 12)
#define CPU_LOG_TB_NOCHAIN (1 << 13)
#define CPU_LOG_PAGE       (1 << 14)

/* Returns true if a bit is set in the current loglevel mask
 */
static inline bool qemu_loglevel_mask(int mask)
{
    return (qemu_loglevel & mask) != 0;
}

/* Logging functions: */

/* main logging function
 */
void GCC_FMT_ATTR(1, 2) qemu_log(const char *fmt, ...);

/* vfprintf-like logging function
 */
static inline void GCC_FMT_ATTR(1, 0)
qemu_log_vprintf(const char *fmt, va_list va)
{
    if (qemu_logfile) {
        vfprintf(qemu_logfile, fmt, va);
    }
}

/* log only if a bit is set on the current loglevel mask
 */
void GCC_FMT_ATTR(2, 3) qemu_log_mask(int mask, const char *fmt, ...);


/* Special cases: */

/* cpu_dump_state() logging functions: */
/**
 * log_cpu_state:
 * @cpu: The CPU whose state is to be logged.
 * @flags: Flags what to log.
 *
 * Logs the output of cpu_dump_state().
 */
static inline void log_cpu_state(CPUState *cpu, int flags)
{
    if (qemu_log_enabled()) {
        cpu_dump_state(cpu, qemu_logfile, fprintf, flags);
    }
}

/**
 * log_cpu_state_mask:
 * @mask: Mask when to log.
 * @cpu: The CPU whose state is to be logged.
 * @flags: Flags what to log.
 *
 * Logs the output of cpu_dump_state() if loglevel includes @mask.
 */
static inline void log_cpu_state_mask(int mask, CPUState *cpu, int flags)
{
    if (qemu_loglevel & mask) {
        log_cpu_state(cpu, flags);
    }
}

#ifdef NEED_CPU_H
/* disas() and target_disas() to qemu_logfile: */
static inline void log_target_disas(CPUState *cpu, target_ulong start,
                                    target_ulong len, int flags)
{
    target_disas(qemu_logfile, cpu, start, len, flags);
}

static inline void log_disas(void *code, unsigned long size)
{
    disas(qemu_logfile, code, size);
}

#if defined(CONFIG_USER_ONLY)
/* page_dump() output to the log file: */
static inline void log_page_dump(void)
{
    page_dump(qemu_logfile);
}
#endif
#endif


/* Maintenance: */

/* fflush() the log file */
static inline void qemu_log_flush(void)
{
    fflush(qemu_logfile);
}

/* Close the log file */
static inline void qemu_log_close(void)
{
    if (qemu_logfile) {
        if (qemu_logfile != stderr) {
            fclose(qemu_logfile);
        }
        qemu_logfile = NULL;
    }
}

/* Set up a new log file */
static inline void qemu_log_set_file(FILE *f)
{
    qemu_logfile = f;
}

/* define log items */
typedef struct QEMULogItem {
    int mask;
    const char *name;
    const char *help;
} QEMULogItem;

extern const QEMULogItem qemu_log_items[];

/* This is the function that actually does the work of
 * changing the log level; it should only be accessed via
 * the qemu_set_log() wrapper.
 */
void do_qemu_set_log(int log_flags, bool use_own_buffers);

static inline void qemu_set_log(int log_flags)
{
#ifdef CONFIG_USER_ONLY
    do_qemu_set_log(log_flags, true);
#else
    do_qemu_set_log(log_flags, false);
#endif
}

void qemu_set_log_filename(const char *filename);
int qemu_str_to_log_mask(const char *str);

/* Print a usage message listing all the valid logging categories
 * to the specified FILE*.
 */
void qemu_print_log_usage(FILE *f);

#endif
