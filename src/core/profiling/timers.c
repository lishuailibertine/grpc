/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifdef GRPC_LATENCY_PROFILER

#include "timers.h"

#include <grpc/support/alloc.h>
#include <grpc/support/log.h>
#include <grpc/support/time.h>
#include <grpc/support/sync.h>
#include <stdio.h>

typedef struct grpc_timer_entry {
#ifdef GRPC_TIMERS_RDTSC
#error Rdtsc timers not supported yet
  /* TODO(vpai): Fill in rdtsc support if desired */
#else
  gpr_timespec timer;
#endif
  const char* tag;
  int seq;
  const char* file;
  int line;
} grpc_timer_entry;

struct grpc_timers_log {
  gpr_mu mu;
  grpc_timer_entry* log;
  int num_entries;
  int capacity;
  int capacity_limit;
  FILE *fp;
  const char *fmt;
};

grpc_timers_log* grpc_timers_log_global = NULL;

static int timer_now(grpc_timer_entry *tm) {
#ifdef GRPC_TIMERS_RDTSC
#error Rdtsc not supported yet
#else
  tm->timer = gpr_now();
  return(1);
#endif
}

grpc_timers_log* grpc_timers_log_create(int capacity_limit, FILE *dump,
                                        const char *fmt) {
  grpc_timers_log* log = gpr_malloc(sizeof(*log));
  GPR_ASSERT(log);

  /* TODO (vpai): Allow allocation below limit */
  log->log = gpr_malloc(capacity_limit*sizeof(*log->log));
  GPR_ASSERT(log->log);

  /* TODO (vpai): Improve concurrency, do per-thread logging? */
  gpr_mu_init(&log->mu);

  log->num_entries = 0;
  log->capacity = log->capacity_limit = capacity_limit;

  log->fp = dump;
  log->fmt = fmt;

  return log;
}

static void log_report_locked(grpc_timers_log *log) {
  FILE *fp = log->fp;
  const char *fmt = log->fmt;
  int i;
  for (i=0;i<log->num_entries;i++) {
    grpc_timer_entry* entry = &(log->log[i]);
    fprintf(fp, fmt,
#ifdef GRPC_TIMERS_RDTSC
#error Rdtsc not supported
#else
            entry->timer.tv_sec, entry->timer.tv_nsec,
#endif
            entry->tag, entry->seq, entry->file, entry->line);
  }

  /* Now clear out the log */
  log->num_entries=0;
}

void grpc_timers_log_destroy(grpc_timers_log *log) {
  gpr_mu_lock(&log->mu);
  log_report_locked(log);
  gpr_mu_unlock(&log->mu);

  gpr_free(log->log);
  gpr_mu_destroy(&log->mu);

  gpr_free(log);
}

void grpc_timers_log_add(grpc_timers_log *log, const char *tag, int seq,
                         const char *file, int line) {
  grpc_timer_entry* entry;

  /* TODO (vpai) : Improve concurrency */
  gpr_mu_lock(&log->mu);
  if (log->num_entries == log->capacity_limit) {
    log_report_locked(log);
  }

  entry = &log->log[log->num_entries++];

  timer_now(entry);
  entry->tag = tag;
  entry->seq = seq;
  entry->file = file;
  entry->line = line;

  gpr_mu_unlock(&log->mu);
}

void grpc_timers_log_global_init(void) {
  grpc_timers_log_global =
      grpc_timers_log_create(100000, stdout,
#ifdef GRPC_TIMERS_RDTSC
#error Rdtsc not supported
#else
                            "TIMER %1$ld.%2$09d %3$s seq %4$d @ %5$s:%6$d\n"
#endif
                            );
  /* Use positional arguments as an example for others to change fmt */
}

void grpc_timers_log_global_destroy(void) {
  grpc_timers_log_destroy(grpc_timers_log_global);
}

#else /* !GRPC_LATENCY_PROFILER */
void grpc_timers_log_global_init(void) {
}
void grpc_timers_log_global_destroy(void) {
}
#endif /* GRPC_LATENCY_PROFILER */
