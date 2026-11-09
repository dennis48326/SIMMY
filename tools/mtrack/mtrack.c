// mtrack: an LD_PRELOAD allocation tracker for finding memory that a game keeps across quit / re-entry
// cycles (Linux, glibc). Every malloc/calloc/realloc/memalign/posix_memalign/aligned_alloc is recorded
// with its size, its caller's backtrace and the current "epoch"; free forgets it. Nothing is reported by
// itself: the test script signals the game.
//   SIGUSR2  epoch + 1 (e.g. at each house load)
//   SIGUSR1  dump at the next allocation to $MTRACK_OUT.<n> (n = 0, 1, ...)
// A dump holds the live allocations (count, bytes) per epoch, per (backtrace, epoch), the backtraces as
// raw addresses and the executable mappings; mta.py symbolizes them. Allocations from an early epoch
// that are still live many epochs later, in about the same amount for every epoch, are what each cycle
// keeps. Build: gcc -O2 -fPIC -shared -o mtrack.so mtrack.c -lpthread
// Run: LD_PRELOAD=$PWD/mtrack.so MTRACK_OUT=/tmp/run simmy --headless ... (about 200 MB of extra RSS for
// its tables; an RSS measured with it is not the game's).
#define _GNU_SOURCE
#include <execinfo.h>
#include <fcntl.h>
#include <malloc.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

extern void *__libc_malloc(size_t);
extern void __libc_free(void *);
extern void *__libc_calloc(size_t, size_t);
extern void *__libc_realloc(void *, size_t);
extern void *__libc_memalign(size_t, size_t);

#define DEPTH 18
#define PT_BITS 23
#define PT_SIZE (1u << PT_BITS)
#define ST_BITS 20
#define ST_SIZE (1u << ST_BITS)
#define MAXEP 255

typedef struct { uintptr_t p; uint64_t size : 48, epoch : 16; uint32_t stack; uint32_t pad; } Ent;
typedef struct { uint32_t n; void *f[DEPTH]; } Stk;

static Ent *pt;
static uint32_t *st_slots; // stack id + 1
static Stk *stacks;
static uint32_t nstacks, nlive;
static volatile int epoch, want_dump, ndump;
static pthread_mutex_t lk = PTHREAD_MUTEX_INITIALIZER;
static __thread int guard;
static int ready, full;

static void *mm(size_t n) { void *p = mmap(0, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0); return p == MAP_FAILED ? 0 : p; }
static inline uint32_t hp(uintptr_t p) { uint64_t x = p * 0x9E3779B97F4A7C15ull; return (uint32_t)(x >> (64 - PT_BITS)); }

static void on_sig(int s) { if (s == SIGUSR2) epoch++; else want_dump = 1; }

static void init(void) {
  pt = mm(sizeof(Ent) * (size_t)PT_SIZE);
  st_slots = mm(sizeof(uint32_t) * (size_t)ST_SIZE);
  stacks = mm(sizeof(Stk) * (size_t)ST_SIZE);
  struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_sig; sa.sa_flags = SA_RESTART;
  sigaction(SIGUSR1, &sa, 0); sigaction(SIGUSR2, &sa, 0);
  void *b[4]; guard = 1; backtrace(b, 4); guard = 0; // load libgcc unwinder now
  ready = pt && st_slots && stacks;
}

static uint32_t stack_id(void) {
  Stk s; int n = backtrace(s.f, DEPTH); s.n = n > 2 ? n - 2 : 0; memmove(s.f, s.f + 2, sizeof(void *) * s.n);
  for (uint32_t i = s.n; i < DEPTH; i++) s.f[i] = 0;
  uint64_t h = 1469598103934665603ull;
  for (uint32_t i = 0; i < s.n; i++) h = (h ^ (uintptr_t)s.f[i]) * 1099511628211ull;
  uint32_t k = (uint32_t)(h >> (64 - ST_BITS));
  for (;;) {
    uint32_t v = st_slots[k];
    if (!v) {
      if (nstacks >= ST_SIZE - 1) return 0;
      stacks[nstacks] = s; st_slots[k] = ++nstacks; return nstacks - 1;
    }
    Stk *o = &stacks[v - 1];
    if (o->n == s.n && !memcmp(o->f, s.f, sizeof(void *) * s.n)) return v - 1;
    k = (k + 1) & (ST_SIZE - 1);
  }
}

static void wr(int fd, const char *b, int n) { while (n > 0) { int w = write(fd, b, n); if (w <= 0) return; b += w; n -= w; } }

static void dump(void) {
  char name[512]; const char *o = getenv("MTRACK_OUT"); if (!o) o = "mtrack";
  snprintf(name, sizeof name, "%s.%d", o, ndump++);
  int fd = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (fd < 0) return;
  char buf[1024]; int n;
  static uint64_t ec[MAXEP + 1], eb[MAXEP + 1];
  memset(ec, 0, sizeof ec); memset(eb, 0, sizeof eb);
  // aggregate (stack, epoch) in a temp table
  uint32_t AS = 1u << 22;
  typedef struct { uint32_t key_st; uint16_t ep; uint16_t used; uint64_t c, b; } Ag;
  Ag *ag = mm(sizeof(Ag) * (size_t)AS);
  for (uint32_t i = 0; i < PT_SIZE; i++) {
    Ent *e = &pt[i]; if (!e->p) continue;
    ec[e->epoch]++; eb[e->epoch] += e->size;
    if (!ag) continue;
    uint32_t k = (uint32_t)(((uint64_t)e->stack * 0x9E3779B1u + e->epoch * 0x85EBCA6Bu) & (AS - 1));
    while (ag[k].used && !(ag[k].key_st == e->stack && ag[k].ep == e->epoch)) k = (k + 1) & (AS - 1);
    ag[k].used = 1; ag[k].key_st = e->stack; ag[k].ep = e->epoch; ag[k].c++; ag[k].b += e->size;
  }
  struct mallinfo2 mi = mallinfo2();
  n = snprintf(buf, sizeof buf, "I epoch %d live %u stacks %u full %d arena %zu hblkhd %zu uordblks %zu fordblks %zu keepcost %zu\n",
               epoch, nlive, nstacks, full, mi.arena, mi.hblkhd, mi.uordblks, mi.fordblks, mi.keepcost);
  wr(fd, buf, n);
  for (int e = 0; e <= MAXEP; e++) if (ec[e]) { n = snprintf(buf, sizeof buf, "E %d %llu %llu\n", e, (unsigned long long)ec[e], (unsigned long long)eb[e]); wr(fd, buf, n); }
  if (ag) {
    static uint8_t *used_st; if (!used_st) used_st = mm(ST_SIZE);
    memset(used_st, 0, ST_SIZE);
    for (uint32_t i = 0; i < AS; i++) if (ag[i].used) {
      n = snprintf(buf, sizeof buf, "A %u %u %llu %llu\n", ag[i].key_st, ag[i].ep, (unsigned long long)ag[i].c, (unsigned long long)ag[i].b);
      wr(fd, buf, n); used_st[ag[i].key_st] = 1;
    }
    for (uint32_t s = 0; s < nstacks; s++) if (used_st[s]) {
      n = snprintf(buf, sizeof buf, "S %u", s);
      for (uint32_t j = 0; j < stacks[s].n; j++) n += snprintf(buf + n, sizeof buf - n, " %lx", (unsigned long)(uintptr_t)stacks[s].f[j]);
      buf[n++] = '\n'; wr(fd, buf, n);
    }
    munmap(ag, sizeof(Ag) * (size_t)AS);
  }
  int mf = open("/proc/self/maps", O_RDONLY);
  if (mf >= 0) {
    static char mb[1 << 20]; int t = 0, r;
    while (t < (int)sizeof mb - 1 && (r = read(mf, mb + t, sizeof mb - 1 - t)) > 0) t += r;
    close(mf); mb[t] = 0;
    char *l = mb;
    while (*l) { char *e = strchr(l, '\n'); if (!e) break; *e = 0; if (strstr(l, "r-xp")) { n = snprintf(buf, sizeof buf, "M %s\n", l); wr(fd, buf, n); } l = e + 1; }
  }
  close(fd);
}

static void add(void *p, size_t sz) {
  if (!p || guard) return;
  if (!ready) { if (ready == 0) { guard = 1; pthread_mutex_lock(&lk); if (!pt) init(); pthread_mutex_unlock(&lk); guard = 0; } if (!ready) return; }
  guard = 1;
  uint32_t sid = stack_id();
  pthread_mutex_lock(&lk);
  if (want_dump) { want_dump = 0; dump(); }
  if (nlive < PT_SIZE * 7u / 10u) {
    uint32_t k = hp((uintptr_t)p);
    while (pt[k].p && pt[k].p != (uintptr_t)p) k = (k + 1) & (PT_SIZE - 1);
    if (!pt[k].p) nlive++;
    pt[k].p = (uintptr_t)p; pt[k].size = sz; pt[k].epoch = epoch > MAXEP ? MAXEP : epoch; pt[k].stack = sid;
  } else full = 1;
  pthread_mutex_unlock(&lk);
  guard = 0;
}

static void del(void *p) {
  if (!p || !ready) return;
  pthread_mutex_lock(&lk);
  uint32_t k = hp((uintptr_t)p);
  while (pt[k].p && pt[k].p != (uintptr_t)p) k = (k + 1) & (PT_SIZE - 1);
  if (pt[k].p) {
    nlive--;
    // backward-shift deletion
    uint32_t i = k, j = k;
    for (;;) {
      pt[i].p = 0;
      for (;;) {
        j = (j + 1) & (PT_SIZE - 1);
        if (!pt[j].p) goto done;
        uint32_t h = hp(pt[j].p);
        if (i <= j ? (i < h && h <= j) : (i < h || h <= j)) continue;
        break;
      }
      pt[i] = pt[j]; i = j;
    }
  }
done:
  pthread_mutex_unlock(&lk);
}

void *malloc(size_t n) { void *p = __libc_malloc(n); add(p, n); return p; }
void free(void *p) { del(p); __libc_free(p); }
void *calloc(size_t a, size_t b) { void *p = __libc_calloc(a, b); add(p, a * b); return p; }
void *realloc(void *o, size_t n) { void *p = __libc_realloc(o, n); if (p || !n) del(o); if (p) add(p, n); return p; }
void *memalign(size_t a, size_t n) { void *p = __libc_memalign(a, n); add(p, n); return p; }
void *aligned_alloc(size_t a, size_t n) { void *p = __libc_memalign(a, n); add(p, n); return p; }
int posix_memalign(void **r, size_t a, size_t n) { void *p = __libc_memalign(a, n); if (!p) return 12; add(p, n); *r = p; return 0; }
void *valloc(size_t n) { void *p = __libc_memalign(4096, n); add(p, n); return p; }
