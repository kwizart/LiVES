// machinestate.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

#include <sys/statvfs.h>

#include "main.h"
#include "support.h"

void init_random() {
  ssize_t randres = -1;
  uint64_t rseed;
  int randfd;

  // try to get randomness from /dev/urandom
  randfd = lives_open2("/dev/urandom", O_RDONLY);

  if (randfd > -1) {
    randres = read(randfd, &rseed, 8);
    close(randfd);
  }

  gettimeofday(&tv, NULL);
  rseed += tv.tv_sec + tv.tv_usec;

  lives_srandom((uint32_t)(rseed & 0xFFFFFFFF));

  randfd = lives_open2("/dev/urandom", O_RDONLY);

  if (randfd > -1) {
    randres = read(randfd, &rseed, 8);
    close(randfd);
  }

  if (randres != 8) {
    gettimeofday(&tv, NULL);
    rseed = tv.tv_sec + tv.tv_usec;
  }

  fastsrand(rseed);
}


/// load measuring function - TODO ----

boolean load_measure_idle(livespointer data) {
#ifdef LOADCHECK

  // function is called as an idlefunc, and we count how many calls per second
  // to give an estimate of machine load

  // measured values: 161 - 215, avg 154 - 181

  static int64_t load_count = -1; // count the fn calls until we reach load_check_count
  static int64_t load_check_count = INIT_LOAD_CHECK_COUNT;
  static int64_t tchecks = 0;
  static ticks_t last_check_ticks = 0; // last time we checked
  static ticks_t total_check = 0; // sum of all deltas
  static int nchecks = 0; // how many times checked
  static int nchecks_counted = 0; // how many valid checks counted

  static double low_avg = 1000000000.;
  static double high_avg = 0.;

  static int varcount = 0; // count of variant values within varratio, reset by vardecay
  static int vardecay = 10, xvardecay = 0; // how many sequential non-variant counts to reset varcount
  double varratio = .9; // ratio of variant to last variant to increment varcount
  static double cstate = -1.; // new state set by similar variant values within a limit
  static double cstate_count = 0;
  int cstate_limit = 4; // number of consecutive variant values within varratio to set cstate
  int cstate_reset = 10; // number of variants at cstate to reset avg
  static double vardelta = 0.; // delta time between variant and last variant
  static ticks_t varperiod = 0; // periodic variance
  int nvarperiod = 2; // number of variant deltas to reset varperiod
  static int xnvarperiod = 0;
  double perratio = .9;
  static ticks_t varticks = 0;
  boolean spike = FALSE;
  static double lvariance = 0.;
  static double xlvariance = 0.;
  static double hload = 0.;
  static double lload = 100000000.;
  static double rescale = 1.;
  int64_t timelimit = 0;
  static int64_t ntimer = -1;
  static boolean add_idle = TRUE;
  double tpi;

  char *msg;
  ticks_t delta_ticks;
  double check_time = TARGET_CHECK_TIME; // adjust load_check_count so we check this number of secs.
  double variance;
  static int phase = 0;

  sched_yield();

  if (mainw->loadmeasure == 0) return FALSE; // was not added so we shouldn't be here
  if (prefs->loadchecktime <= 0.) return FALSE; // function disabled

  if (ntimer > -1 && mainw->loadmeasure_phase == 2) {
    if (++ntimer < timelimit) {
      // during playback, the timer doesnt run so we need to simulate it
      return TRUE;
    }
    ntimer = 0;
  }

  if (mainw->loadmeasure_phase == 1) {
    // continuous checking uses 100% cpu, so we need to pause it
    mainw->loadmeasure = lives_timer_add(ME_DELAY, load_measure_idle, NULL);
    mainw->loadmeasure_phase = 2; // timer wait phase
    return FALSE;
  }

  if (mainw->loadmeasure_phase == 2) {
    // timer fired mode
    last_check_ticks = lives_get_current_ticks();
    mainw->loadmeasure = lives_idle_add_full(G_PRIORITY_LOW, load_measure_idle, NULL, NULL);
    mainw->loadmeasure_phase = 0;
    return FALSE;
  }

  // idlecount mode

  // count idle calls until we reach load_check_count
  if (++load_count < load_check_count) return TRUE;

  g_print("idlephase finished\n");

  // check once per QUICK_CHECK_TIME until we reach TARGET_CHECK_TIME, then once per TARGET_CHECK_TIME seconds
  if (total_check < TARGET_CHECK_TIME || nchecks < N_QUICK_CHECKS - 1) check_time = QUICK_CHECK_TIME;

  tpi = capable->time_per_idle;

  delta_ticks = lives_get_current_ticks() - last_check_ticks;
  g_print("delta_ticks was %ld\n", delta_ticks);

  if (delta_ticks < 100) {
    // too quick, run more idleloops
    load_check_count *= 2;
    return TRUE;
  }

  // reset to timer
  mainw->loadmeasure = lives_timer_add(ME_DELAY, load_measure_idle, NULL);
  add_idle = TRUE;

  //load_count /= rescale;
  tchecks += (int64_t)load_count;
  total_check += delta_ticks;

  capable->time_per_idle = (double)delta_ticks / (double)load_count / TICKS_PER_SECOND_DBL;

  fprintf(stderr, "%.3f %ld %ld\n", capable->time_per_idle, delta_ticks, load_count);

  if (capable->time_per_idle > 0.) {
    int64_t nload_check_count;
    nload_check_count = 1. + check_time / capable->time_per_idle;
    load_check_count = nload_check_count;
    if (nload_check_count > 1.5 * load_check_count) load_check_count = 1.5 * load_check_count;
  }
  if (nchecks > N_QUICK_CHECKS - 1) {
    // variance tells us the ratio of delta time to the current target time
    // if this is very large or small we ignore this check. For example when playing the, idlefunc is not called so time
    // passes without any checking, producing false results.
    variance = (double)delta_ticks / (check_time * TICKS_PER_SECOND_DBL);
    if (variance < .8) {
      rescale = variance;
      check_time = 1;
      return TRUE;
    }
    if (variance > VAR_MAX || variance < VAR_MIN) {
      double load_value = LOAD_SCALING / (double)load_count * check_time;
      double tvar = (double)delta_ticks / (check_time * TICKS_PER_SECOND_DBL);
      double cvar = (double)delta_ticks / (double)load_count / (tpi * TICKS_PER_SECOND_DBL);
      if (xlvariance > 0. && variance < xlvariance && variance * xlvariance > .95) {
        LIVES_INFO("Spike value detected");
        spike = TRUE;
      }
      if (variance > lvariance) {
        xlvariance = variance;
      } else xlvariance = 0.;
      lvariance = variance;
      msg = lives_strdup_printf("Load value is %.3f, avg is %3.f, total loops = %ld\nVariance was %.3f, so ignoring this value."
                                "time variance was %f and count variance was %f",
                                load_value, capable->avg_load, tchecks, variance, tvar, cvar);
      LIVES_INFO(msg);
      g_free(msg);
      if (spike) {
        if (varcount > 0) varcount--;
      } else {
        varcount++;
        xvardecay = 0;
        if (varcount > cstate_limit) {
          if (cstate == -1.) {
            cstate = cvar;
            cstate_count = 1;
          } else {
            if ((cvar > cstate && cstate / cvar > varratio) || (cvar < cstate && cvar / cstate > varratio)) {
              cstate_count++;
              if (cstate_count >= cstate_reset) {
                // reset avg
                nchecks_counted = 0;
                capable->avg_load = load_value;
                cstate_count = 0;
                varcount = 0;
                cstate = -1;
                LIVES_INFO("Load average was reset.");
              } else {
                if (cstate_count > 0) {
                  cstate_count--;
                }
                if (cstate_count == 0) {
                  cstate = cvar;
                  cstate_count = 1;
		  // *INDENT-OFF*
                }}}}}}
      // *INDENT-ON*

      // check for periodic variance
      if (varticks > 0) {
        vardelta = last_check_ticks + delta_ticks - varticks;
        if (varperiod == 0) {
          varperiod = vardelta;
          xnvarperiod = nvarperiod;
        } else {
          if ((varperiod > vardelta && (double)vardelta / (double)varperiod > perratio) ||
              (vardelta <= varperiod && (double)vardelta / (double)varperiod > perratio)) {
            xnvarperiod++;
            if (spike) nvarperiod++;
            if (xnvarperiod == 6) {
              msg = lives_strdup_printf("Possible periodic variance with time %.3f\n", varperiod / TICKS_PER_SECOND_DBL);
              LIVES_INFO(msg);
              lives_free(msg);
            }
          } else {
            if (xnvarperiod > 0) {
              xnvarperiod--;
              if (xnvarperiod == 0) {
                varperiod = vardelta;
		// *INDENT-OFF*
              }}}}}
      // *INDENT-ON*

      varticks = last_check_ticks + delta_ticks;
    } else {
      if (nchecks > 0 && nchecks != 0) {
        varcount--;
        xvardecay++;
        if (xvardecay >= vardecay) {
          cstate = -1.;
          cstate_count = 0;
          varcount = 0;
        }
        capable->load_value = LOAD_SCALING / (double)load_count * check_time;
        if (capable->load_value < lload) lload = capable->load_value;
        if (capable->load_value > hload) hload = capable->load_value;
        capable->avg_load = (capable->avg_load * (double)nchecks_counted + capable->load_value) / (double)(nchecks_counted + 1.);
        msg = lives_strdup_printf("Load value is %.3f (%.3f - %.3f), avg is %3.f (%.3f - %.3f), total loops = %ld, variance was %.3f",
                                  capable->load_value, lload, hload, capable->avg_load, low_avg, high_avg, tchecks, variance);
        LIVES_INFO(msg);
        g_free(msg);
        nchecks_counted++;
        //if (nchecks_counted > 6) {
        if (capable->avg_load > high_avg) high_avg = capable->avg_load;
        if (capable->avg_load < low_avg) low_avg = capable->avg_load;
        //}
      }
    }
  }
  load_count = 0;
  last_check_ticks += delta_ticks;
  nchecks++;

#endif
  return FALSE;
}


////// memory funcs ////

/// susbtitute memory functions. These must be real functions and not #defines since we need fn pointers
#define OIL_MEMCPY_MAX_BYTES 1024 // this can be tuned to provide optimal performance

#ifdef ENABLE_ORC
livespointer lives_orc_memcpy(livespointer dest, livesconstpointer src, size_t n) {
  if (n >= 32 && n <= OIL_MEMCPY_MAX_BYTES) {
    orc_memcpy((uint8_t *)dest, (const uint8_t *)src, n);
    return dest;
  }
  return memcpy(dest, src, n);
}
#endif


#ifdef ENABLE_OIL
livespointer lives_oil_memcpy(livespointer dest, livesconstpointer src, size_t n) {
  if (n >= 32 && n <= OIL_MEMCPY_MAX_BYTES) {
    oil_memcpy((uint8_t *)dest, (const uint8_t *)src, n);
    return dest;
  }
  return memcpy(dest, src, n);
}
#endif


livespointer proxy_realloc(livespointer ptr, size_t new_size) {
  livespointer nptr = lives_malloc(new_size);
  if (nptr && ptr) {
    lives_memmove(nptr, ptr, new_size);
    lives_free(ptr);
  }
  return nptr;
}


#define _cpy_if_nonnull(d, s, size) (d ? lives_memcpy(d, s, size) : d)

// functions with fixed pointers that we can pass to plugins ///
void *_ext_malloc(size_t n) {
  return (n == 0 ? NULL : lives_malloc(n));
}
void *_ext_malloc_and_copy(size_t bsize, const void *block) {
  if (!block || bsize == 0) return NULL;
#ifdef lives_malloc_and_copy
  return lives_malloc_and_copy(bsize, block);
#endif
  return (_cpy_if_nonnull(malloc(bsize), block, bsize));
}
void _ext_unmalloc_and_copy(size_t bsize, void *p) {
  if (!p || bsize == 0) return;
#ifdef lives_unmalloc_and_copy
  lives_unmalloc_and_copy(bsize, p);
#else
  _ext_free(p);
#endif
}
void _ext_free(void *p) {
  if (p) lives_free(p);
}

void *_ext_free_and_return(void *p) {
  _ext_free(p);
  return NULL;
}

void *_ext_memcpy(void *dest, const void *src, size_t n) {
  return lives_memcpy(dest, src, n);
}

void *_ext_memset(void *p, int i, size_t n) {
  return lives_memset(p, i, n);
}

void *_ext_memmove(void *dest, const void *src, size_t n) {
  return lives_memmove(dest, src, n);
}

void *_ext_realloc(void *p, size_t n) {
  return lives_realloc(p, n);
}

void *_ext_calloc(size_t nmemb, size_t msize) {
  return lives_calloc(nmemb, msize);
}

LIVES_GLOBAL_INLINE void *lives_calloc_safety(size_t nmemb, size_t xsize) {
  size_t totsize = nmemb * xsize;
  if (totsize == 0) return NULL;
  if (xsize < DEF_ALIGN) {
    xsize = DEF_ALIGN;
    nmemb = (totsize / xsize) + 1;
  }
  return lives_calloc(nmemb + (EXTRA_BYTES / xsize), xsize);
}

LIVES_GLOBAL_INLINE void *lives_recalloc(void *p, size_t nmemb, size_t omemb, size_t xsize) {
  /// realloc from omemb * size to nmemb * size
  /// memory allocated via calloc, with DEF_ALIGN alignment and EXTRA_BYTES extra padding
  void *np = lives_calloc_safety(nmemb, xsize);
  if (omemb > nmemb) omemb = nmemb;
  lives_memcpy(np, p, omemb * xsize);
  lives_free(p);
  return np;
}


// slice allocator //// TODO

static memheader_t base;           /* Zero sized block to get us started. */
static memheader_t *freep = &base; /* Points to first free block of memory. */
static memheader_t *usedp;         /* Points to first used block of memory. */

/*
   Scan the free list and look for a place to put the block. Basically, we're
   looking for any block the to be freed block might have been partitioned from.
*/
void quick_free(memheader_t *bp) {
  memheader_t *p;

  for (p = freep; !(bp > p && bp < p->next); p = p->next)
    if (p >= p->next && (bp > p || bp < p->next))
      break;

  if (bp + bp->size == p->next) {
    bp->size += p->next->size;
    bp->next = p->next->next;
  } else
    bp->next = p->next;

  if (p + p->size == bp) {
    p->size += bp->size;
    p->next = bp->next;
  } else
    p->next = bp;

  freep = p;
}


#define MIN_ALLOC_SIZE 4096     /* We allocate blocks in page sized chunks. */

/*
   Request more memory from the kernel.
*/
static memheader_t *morecore(size_t num_units) {
  void *vp;
  memheader_t *up;

  if (num_units > MIN_ALLOC_SIZE)
    num_units = MIN_ALLOC_SIZE / sizeof(memheader_t);

  if ((vp = sbrk(num_units * sizeof(memheader_t))) == (void *) - 1)
    return NULL;

  up = (memheader_t *) vp;
  up->size = num_units;
  quick_free(up); // add to freelist
  return freep;
}


/*
   Find a chunk from the free list and put it in the used list.
*/
LIVES_INLINE void *_quick_malloc(size_t alloc_size, size_t align) {
  size_t num_units;
  memheader_t *p, *prevp;

  num_units = (alloc_size + sizeof(memheader_t) - 1) / sizeof(memheader_t) + 1;
  prevp = freep;

  for (p = prevp->next;; prevp = p, p = p->next) {
    if (p->size >= num_units) { /* Big enough. */
      if (p->size == num_units) /* Exact size. */
        prevp->next = p->next;
      else {
        p->size -= num_units;
        p += p->size;
        p->size = num_units;
      }

      freep = prevp;

      /* Add to p to the used list. */
      if (usedp == NULL)
        usedp = p->next = p;
      else {
        p->next = usedp->next;
        usedp->next = p;
      }
      p->align = align;
      return (void *)(p + 1);
    }
    if (p == freep) { /* Not enough memory. */
      p = morecore(num_units);
      if (p == NULL) /* Request for more memory failed. */
        return NULL;
    }
  }
}


/* void *quick_malloc(size_t alloc_size) { */
/*   return _quick_malloc(alloc_size, 1); */
/* } */

/* void *quick_calloc(size_t nmemb, size_t size) { */
/*   return _quick_malloc(nmemb * size, size); */
/* } */

/*   quick_calloc(); */

/* quick_memcpy(); */

/* quick_memmove(); */

/* quick_memset(); */


char *get_md5sum(const char *filename) {
  /// for future use
  char **array;
  char *md5;
  char *com = lives_strdup_printf("%s \"%s\"", EXEC_MD5SUM, filename);
  mainw->com_failed = FALSE;
  lives_popen(com, TRUE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);
  if (mainw->com_failed) {
    mainw->com_failed = FALSE;
    return NULL;
  }
  array = lives_strsplit(mainw->msg, " ", 2);
  md5 = lives_strdup(array[0]);
  lives_strfreev(array);
  return md5;
}


char *lives_format_storage_space_string(uint64_t space) {
  char *fmt;

  if (space > lives_10pow(18)) {
    // TRANSLATORS: Exabytes
    fmt = lives_strdup_printf(_("%.2f EB"), (double)space / (double)lives_10pow(18));
  } else if (space > lives_10pow(15)) {
    // TRANSLATORS: Petabytes
    fmt = lives_strdup_printf(_("%.2f PB"), (double)space / (double)lives_10pow(15));
  } else if (space > lives_10pow(12)) {
    // TRANSLATORS: Terabytes
    fmt = lives_strdup_printf(_("%.2f TB"), (double)space / (double)lives_10pow(12));
  } else if (space > lives_10pow(9)) {
    // TRANSLATORS: Gigabytes
    fmt = lives_strdup_printf(_("%.2f GB"), (double)space / (double)lives_10pow(9));
  } else if (space > lives_10pow(6)) {
    // TRANSLATORS: Megabytes
    fmt = lives_strdup_printf(_("%.2f MB"), (double)space / (double)lives_10pow(6));
  } else if (space > 1024) {
    // TRANSLATORS: Kilobytes (1024 bytes)
    fmt = lives_strdup_printf(_("%.2f KiB"), (double)space / 1024.);
  } else {
    fmt = lives_strdup_printf(_("%d bytes"), space);
  }

  return fmt;
}


lives_storage_status_t get_storage_status(const char *dir, uint64_t warn_level, uint64_t *dsval) {
  // WARNING: this will actually create the directory (since we dont know if its parents are needed)
  uint64_t ds;
  if (!is_writeable_dir(dir)) return LIVES_STORAGE_STATUS_UNKNOWN;
  ds = get_fs_free(dir);
  if (dsval != NULL) *dsval = ds;
  if (ds < prefs->ds_crit_level) return LIVES_STORAGE_STATUS_CRITICAL;
  if (ds < warn_level) return LIVES_STORAGE_STATUS_WARNING;
  return LIVES_STORAGE_STATUS_NORMAL;
}


uint64_t get_fs_free(const char *dir) {
  // get free space in bytes for volume containing directory dir
  // return 0 if we cannot create/write to dir

  // caller should test with is_writeable_dir() first before calling this
  // since 0 is a valid return value

  // dir should be in locale encoding

  // WARNING: this will actually create the directory (since we dont know if its parents are needed)

  struct statvfs sbuf;

  uint64_t bytes = 0;
  boolean must_delete = FALSE;

  if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) must_delete = TRUE;
  if (!is_writeable_dir(dir)) goto getfserr;

  // use statvfs to get fs details
  if (statvfs(dir, &sbuf) == -1) goto getfserr;
  if (sbuf.f_flag & ST_RDONLY) goto getfserr;

  // result is block size * blocks available
  bytes = sbuf.f_bsize * sbuf.f_bavail;

getfserr:
  if (must_delete) lives_rmdir(dir, FALSE);

  return bytes;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_relative_ticks(int64_t origsecs, int64_t origusecs) {
#ifdef USE_MONOTONIC_TIME
  return (lives_get_monotonic_time() - origusecs) * USEC_TO_TICKS;
#else
  gettimeofday(&tv, NULL);
  return TICKS_PER_SECOND * (tv.tv_sec - origsecs) + tv.tv_usec * USEC_TO_TICKS - origusecs * USEC_TO_TICKS;
#endif
}


LIVES_GLOBAL_INLINE ticks_t lives_get_current_ticks(void) {
  //  return current (wallclock) time in ticks (units of 10 nanoseconds)
  return lives_get_relative_ticks(0, 0);
}


char *lives_datetime(struct timeval * tv) {
  char buf[128];
  char *datetime = NULL;
  struct tm *gm = gmtime(&tv->tv_sec);
  ssize_t written;

  if (gm) {
    written = (ssize_t)strftime(buf, 128, "%Y-%m-%d    %H:%M:%S", gm);
    if ((written > 0) && ((size_t)written < 128)) {
      datetime = lives_strdup(buf);
    }
  }
  return datetime;
}


boolean check_dev_busy(char *devstr) {
  int ret;
#ifdef IS_SOLARIS
  struct flock lock;
  lock.l_start = 0;
  lock.l_whence = SEEK_SET;
  lock.l_len = 0;
  lock.l_type = F_WRLCK;
#endif
  int fd = open(devstr, O_RDONLY | O_NONBLOCK);
  if (fd == -1) return FALSE;
#ifdef IS_SOLARIS
  ret = fcntl(fd, F_SETLK, &lock);
#else
  ret = flock(fd, LOCK_EX | LOCK_NB);
#endif
  close(fd);
  if (ret == -1) return FALSE;
  return TRUE;
}


uint64_t get_file_size(int fd) {
  // get the size of file fd
  struct stat filestat;
  fstat(fd, &filestat);
  return (uint64_t)(filestat.st_size);
}


uint64_t sget_file_size(const char *name) {
  // get the size of file fd
  struct stat filestat;
  int fd;

  if ((fd = open(name, O_RDONLY)) == -1) {
    return (uint32_t)0;
  }

  fstat(fd, &filestat);
  close(fd);

  return (uint64_t)(filestat.st_size);
}


void reget_afilesize(int fileno) {
  // re-get the audio file size
  lives_clip_t *sfile = mainw->files[fileno];
  boolean bad_header = FALSE;

  if (mainw->multitrack != NULL) return; // otherwise achans gets set to 0...

  sfile->afilesize = reget_afilesize_inner(fileno);

  if (sfile->afilesize == 0l) {
    if (!sfile->opening && fileno != mainw->ascrap_file && fileno != mainw->scrap_file) {
      if (sfile->arate != 0 || sfile->achans != 0 || sfile->asampsize != 0 || sfile->arps != 0) {
        sfile->arate = sfile->achans = sfile->asampsize = sfile->arps = 0;
        save_clip_value(fileno, CLIP_DETAILS_ACHANS, &sfile->achans);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        save_clip_value(fileno, CLIP_DETAILS_ARATE, &sfile->arps);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        save_clip_value(fileno, CLIP_DETAILS_PB_ARATE, &sfile->arate);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        save_clip_value(fileno, CLIP_DETAILS_ASAMPS, &sfile->asampsize);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        if (bad_header) do_header_write_error(fileno);
      }
    }
  }

  if (mainw->is_ready && fileno > 0 && fileno == mainw->current_file) {
    // force a redraw
    update_play_times();
  }
}


uint64_t reget_afilesize_inner(int fileno) {
  // safe version that just returns the audio file size
  uint64_t filesize;
  char *afile = lives_get_audio_file_name(fileno);
  lives_sync(1);
  filesize = sget_file_size(afile);
  lives_free(afile);
  return filesize;
}


#ifdef PRODUCE_LOG
// disabled by default
void lives_log(const char *what) {
  char *lives_log_file = lives_build_filename(prefs->workdir, LIVES_LOG_FILE, NULL);
  if (mainw->log_fd < 0) mainw->log_fd = open(lives_log_file, O_WRONLY | O_CREAT, DEF_FILE_PERMS);
  if (mainw->log_fd != -1) {
    char *msg = lives_strdup("%s|%d|", what, mainw->current_file);
    write(mainw->log_fd, msg, strlen(msg));
    lives_free(msg);
  }
  lives_free(lives_log_file);
}
#endif


int check_for_bad_ffmpeg(void) {
  int i, fcount;
  char *fname_next;
  boolean maybeok = FALSE;

  fcount = get_frame_count(mainw->current_file, 1);

  for (i = 1; i <= fcount; i++) {
    fname_next = make_image_file_name(cfile, i, get_image_ext_for_type(cfile->img_type));
    if (sget_file_size(fname_next) > 0) {
      lives_free(fname_next);
      maybeok = TRUE;
      break;
    }
    lives_free(fname_next);
  }

  if (!maybeok) {
    do_error_dialog(
      _("Your version of mplayer/ffmpeg may be broken !\nSee http://bugzilla.mplayerhq.hu/show_bug.cgi?id=2071\n\n"
        "You can work around this temporarily by switching to jpeg output in Preferences/Decoding.\n\n"
        "Try running Help/Troubleshoot for more information."));
    return CANCEL_ERROR;
  }
  return CANCEL_NONE;
}


#define hasNulByte(x) ((x - 0x0101010101010101) & ~x & 0x8080808080808080)

#define getnulpos(nulmask) ((nulmask & 0x0000000080808080) ? ((nulmask & 0x0000000000008080) ? \
							      ((nulmask & 0x0000000000000080) ? 0 : 1) : (((nulmask & 0x0000000000800000) ? 2 : 3))) : \
			    ((nulmask & 0x0000808000000000) ? ((nulmask & 0x0000008000000000) ? 4 : 5) : \
			     ((nulmask & 0x0080000000000000) ? 6 : 7)))

LIVES_GLOBAL_INLINE size_t lives_strlen(const char *s) {
  if (!s) return 0;
  else {
    const char *p = s;
    uint64_t *pi = (uint64_t *)p, nulmask;
    while (*p) {
      if ((void *)pi == (void *)p) {
        while (!(nulmask = hasNulByte(*pi))) ++pi;
        if ((void *)pi - (void *)s + getnulpos(nulmask) != strlen(s)) {
          g_print("len of %s (%ld) is of course %ld + %d, i.e. %ld %lx   \n", s, strlen(s),
                  (void *)pi - (void *)s, getnulpos(nulmask), (void *)pi - (void *)s + getnulpos(nulmask), nulmask);
          return (void *)pi - (void *)s + getnulpos(nulmask) ;
        }
      }
      p++;
    }
    return p - s;
  }
}

/// returns FALSE if strings match
LIVES_GLOBAL_INLINE boolean lives_strcmp(const char *st1, const char *st2) {
  if (!st1 || !st2) return (st1 != st2);
  else {
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if ((void *)ip1 == (void *)st1 && (void *)ip2 == (void *)st2) {
        while (1) {
          if ((d1 = *(ip1++)) == (d2 = *(ip2++))) {
            if (hasNulByte(d1)) {
              if (!hasNulByte(d2)) return TRUE;
              break;
            }
          } else {
            if (!hasNulByte(d1) || !(hasNulByte(d2))) return TRUE;
            break;
          }
        }
        st1 = (const char *)(--ip1); st2 = (const char *)(--ip2);
      }
      if (*st1 != *st2 || !(*st1)) break;
      st1++; st2++;
    }
  }
  return (*st2 != 0);
}

LIVES_GLOBAL_INLINE int lives_strcmp_ordered(const char *st1, const char *st2) {
  if (!st1 || !st2) return (st1 != st2);
  else {
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if ((const char *)ip1 == st1 && (const char *)ip2 == st2) {
        do {
          d1 = *(ip1++);
          d2 = *(ip2++);
        } while (d1 == d2 && !hasNulByte(d1));
        st1 = (const char *)(--ip1); st2 = (const char *)(--ip2);
      }
      if (*st1 != *st2 || !(*st1)) break;
      st1++; st2++;
    }
  }
  return (*st1 > *st2) - (*st1 < *st2);
}

/// returns FALSE if strings match
LIVES_GLOBAL_INLINE boolean lives_strncmp(const char *st1, const char *st2, size_t len) {
  if (!st1 || !st2) return (st1 != st2);
  else {
    size_t xlen = len >> 3;
    uint64_t d1, d2, *ip1 = (uint64_t *)st1, *ip2 = (uint64_t *)st2;
    while (1) {
      if (xlen && (const char *)ip1 == st1 && (const char *)ip2 == st2) {
        do {
          d1 = *(ip1++);
          d2 = *(ip2++);
        } while (d1 == d2 && !hasNulByte(d1) && --xlen);
        if (xlen) {
          if (!hasNulByte(d2)) return TRUE;
          ip1--;
          ip2--;
        }
        st1 = (const char *)ip1; st2 = (const char *)ip2;
        len -= ((len >> 3) - xlen) << 3;
      }
      if (!(len--)) return FALSE;
      if (*st1 != *st2 || !(*st1)) break;
      st1++; st2++;
    }
  }
  return (*st1 != *st2);
}


LIVES_GLOBAL_INLINE uint32_t string_hash(const char *string) {
  uint32_t hash = 5381;
  if (string == NULL) return 0;
  for (char c; (c = *(string++)) != 0; hash += (hash << 5) + c);
  return hash;
}

///////// thread pool ////////////////////////

#define MINPOOLTHREADS 4
static int npoolthreads;
static pthread_t **poolthrds;
static pthread_cond_t tcond  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t tcond_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t twork_mutex = PTHREAD_MUTEX_INITIALIZER;
static LiVESList *twork_first, *twork_last; /// FIFO list of tasks
static int ntasks;
static boolean threads_die;

static void *thrdpool(void *arg) {
  LiVESList *list;
  thrd_work_t *mywork;
  int myidx = LIVES_POINTER_TO_INT(arg);
  while (!threads_die) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_wait(&tcond, &tcond_mutex);
    pthread_mutex_unlock(&tcond_mutex);
    if (threads_die) break;
    while (!threads_die) {
      pthread_mutex_lock(&twork_mutex);
      if ((list = twork_last) == NULL) {
        pthread_mutex_unlock(&twork_mutex);
        break;
      }
      if (twork_first == list) twork_first = NULL;
      twork_last = list->prev;
      if (twork_last != NULL) twork_last->next = NULL;
      pthread_mutex_unlock(&twork_mutex);

      list->prev = list->next = NULL;
      mywork = (thrd_work_t *)list->data;
      mywork->busy = myidx + 1;
      (*mywork->func)(mywork->arg);
      pthread_mutex_lock(&twork_mutex);
      ntasks--;
      pthread_mutex_lock(&mywork->cond_mutex);
      pthread_cond_signal(&mywork->cond);
      pthread_mutex_unlock(&mywork->cond_mutex);
      mywork->done = myidx + 1;
      pthread_mutex_unlock(&twork_mutex);
    }
  }
  return NULL;
}


void lives_threadpool_init(void) {
  npoolthreads = MINPOOLTHREADS;
  if (prefs->nfx_threads > npoolthreads) npoolthreads = prefs->nfx_threads;
  poolthrds = (pthread_t **)lives_calloc(npoolthreads, sizeof(pthread_t *));
  threads_die = FALSE;
  twork_first = twork_last = NULL;
  ntasks = 0;
  for (int i = 0; i < npoolthreads; i++) {
    poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
    pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i));
  }
}


void lives_threadpool_finish(void) {
  threads_die = TRUE;
  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_broadcast(&tcond);
  pthread_mutex_unlock(&tcond_mutex);
  for (int i = 0; i < npoolthreads; i++) {
    pthread_mutex_lock(&tcond_mutex);
    pthread_cond_broadcast(&tcond);
    pthread_mutex_unlock(&tcond_mutex);
    pthread_join(*(poolthrds[i]), NULL);
    lives_free(poolthrds[i]);
  }
  lives_free(poolthrds);
  poolthrds = NULL;
  npoolthreads = 0;
  lives_list_free_all((LiVESList **)&twork_first);
  twork_first = twork_last = NULL;
  ntasks = 0;
}


int lives_thread_create(lives_thread_t *thread, void *attr, lives_funcptr_t func, void *arg) {
  LiVESList *list = (LiVESList *)thread;
  thrd_work_t *work;
  list->data = work = (thrd_work_t *)lives_calloc(1, sizeof(thrd_work_t));
  list->prev = NULL;
  work->func = func;
  work->arg = arg;
  pthread_cond_init(&work->cond, NULL);
  pthread_mutex_init(&work->cond_mutex, NULL);

  pthread_mutex_lock(&twork_mutex);
  if (twork_first != NULL) twork_first->prev = list;
  list->next = twork_first;
  twork_first = list;
  if (twork_last == NULL) twork_last = list;
  if (++ntasks > npoolthreads) {
    poolthrds = (pthread_t **)lives_realloc(poolthrds, (npoolthreads + MINPOOLTHREADS) * sizeof(pthread_t *));
    for (int i = npoolthreads; i < npoolthreads + MINPOOLTHREADS; i++) {
      poolthrds[i] = (pthread_t *)lives_malloc(sizeof(pthread_t));
      pthread_create(poolthrds[i], NULL, thrdpool, LIVES_INT_TO_POINTER(i));
      pthread_mutex_lock(&tcond_mutex);
      pthread_cond_signal(&tcond);
      pthread_mutex_unlock(&tcond_mutex);
    }
    npoolthreads += MINPOOLTHREADS;
  }
  pthread_mutex_unlock(&twork_mutex);
  pthread_mutex_lock(&tcond_mutex);
  pthread_cond_signal(&tcond);
  pthread_mutex_unlock(&tcond_mutex);
  return 0;
}


int lives_thread_join(lives_thread_t work, void **retval) {
  struct timespec ts;
  thrd_work_t *task = (thrd_work_t *)work.data;
  while (!task->busy) {
    sched_yield();
    lives_usleep(100);
  }
  while (!task->done) {
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += 100000;
    pthread_mutex_lock(&task->cond_mutex);
    pthread_cond_timedwait(&task->cond, &task->cond_mutex, &ts);
    pthread_mutex_unlock(&task->cond_mutex);
    sched_yield();
  }
  if (retval != NULL) *retval = task->ret;
  pthread_mutex_lock(&twork_mutex);
  lives_free(task);
  pthread_mutex_unlock(&twork_mutex);
  return 0;
}

LIVES_GLOBAL_INLINE void lives_srandom(unsigned int seed) {
  srandom(seed);
}

LIVES_GLOBAL_INLINE uint64_t lives_random(void) {
  return random();
}

LIVES_GLOBAL_INLINE pid_t lives_getpid(void) {
  return getpid();
}

LIVES_GLOBAL_INLINE int lives_getuid(void) {
  return geteuid();
}

LIVES_GLOBAL_INLINE int lives_getgid(void) {
  return getegid();
}

static uint16_t swabtab[65536];
static boolean swabtab_inited = FALSE;

static void init_swabtab(void) {
  for (int i = 0; i < 256; i++) {
    int z = i << 8;
    for (int j = 0; j < 256; j++) {
      swabtab[z++] = (j << 8) + i;
    }
  }
  swabtab_inited = TRUE;
}

union split8 {
  uint64_t u64;
  uint32_t u32[2];
};

union split4 {
  uint32_t u32;
  uint16_t u16[2];
};


// gran(ularity) may be 1, or 2
LIVES_GLOBAL_INLINE void swab2(const void *from, const void *to, size_t gran) {
  uint16_t *s = (uint16_t *)from;
  uint16_t *d = (uint16_t *)to;
  if (gran == 2) {
    uint16_t tmp = *s;
    *s = *d;
    *d = tmp;
    return;
  }
  if (!swabtab_inited) init_swabtab();
  *d = swabtab[*s];
}

// gran(ularity) may be 1, 2 or 4
LIVES_GLOBAL_INLINE void swab4(const void *from, const void *to, size_t gran) {
  union split4 *d = (union split4 *)to, s;
  uint16_t tmp;

  if (gran > 2) {
    lives_memcpy((void *)to, from, gran);
    return;
  }
  s.u32 = *(uint32_t *)from;
  tmp = s.u16[0];
  if (gran == 2) {
    d->u16[0] = s.u16[1];
    d->u16[1] = tmp;
  } else {
    swab2(&s.u16[1], &d->u16[0], 1);
    swab2(&tmp, &d->u16[1], 1);
  }
}


// gran(ularity) may be 1, 2 or 4
LIVES_GLOBAL_INLINE void swab8(const void *from, const void *to, size_t gran) {
  union split8 *d = (union split8 *)to, s;
  uint32_t tmp;
  if (gran > 4) {
    lives_memcpy((void *)to, from, gran);
    return;
  }
  s.u64 = *(uint64_t *)from;
  tmp = s.u32[0];
  if (gran == 4) {
    d->u32[0] = s.u32[1];
    d->u32[1] = tmp;
  } else {
    swab4(&s.u32[1], &d->u32[0], gran);
    swab4(&tmp, &d->u32[1], gran);
  }
}


LIVES_GLOBAL_INLINE void reverse_bytes(char *buff, size_t count, size_t gran) {
  if (count == 2) swab2(buff, buff, 1);
  if (count == 4) swab4(buff, buff, gran);
  else if (count == 8) swab8(buff, buff, gran);
}


boolean reverse_buffer(uint8_t *buff, size_t count, size_t chunk) {
  // reverse chunk sized bytes in buff, count must be a multiple of chunk
  ssize_t start = -1, end;
  size_t ocount = count;

  if (chunk < 8) {
    if ((chunk != 4 && chunk != 2 && chunk != 1) || (count % chunk) != 0) return FALSE;
  } else {
    if ((chunk & 0x01) || (count % chunk) != 0) return FALSE;
    else {
      void *tbuff = lives_malloc(chunk);
      start++;
      end = ocount - 1 - chunk;
      while (start + chunk < end) {
        lives_memcpy(tbuff, &buff[end], chunk);
        lives_memcpy(&buff[end], &buff[start], chunk);
        lives_memcpy(&buff[start], tbuff, chunk);
        start += chunk;
        end -= chunk;
      }
      lives_free(tbuff);
      return TRUE;
    }
  }

  /// halve the number of bytes, since we will work forwards and back to meet in the middle
  count >>= 1;

  if (count >= 8) {
    // start by swapping 8 bytes from each end
    uint64_t *buff8 = (uint64_t *)buff;
    if ((void *)buff8 == (void *)buff) {
      end = ocount  >> 3;
      for (; count >= 8; count -= 8) {
        /// swap 8 bytes at a time from start and end
        uint64_t tmp8 = buff8[--end];
        if (chunk == 8) {
          buff8[end] = buff8[++start];
          buff8[start] = tmp8;
        } else {
          swab8(&buff8[++start], &buff8[end], chunk);
          swab8(&tmp8, &buff8[start], chunk);
        }
      }
      if (count == 0) return TRUE;
      start = (start + 1) << 3;
      start--;
    }
  }

  /// remainder should be only 6, 4, or 2 bytes in the middle

  if (count >= 4) {
    uint32_t *buff4 = (uint32_t *)buff;
    if ((void *)buff4 == (void *)buff) {
      if (start > 0) {
        end = (ocount - start) >> 2;
        start >>= 2;
      } else end = ocount >> 2;
      for (; count >= 4; count -= 4) {
        /// swap 4 bytes at a time from start and end
        uint32_t tmp4 = buff4[--end];
        if (chunk == 4) {
          buff4[end] = buff4[++start];
          buff4[start] = tmp4;
        } else {
          swab4(&buff4[++start], &buff4[end], chunk);
          swab4(&tmp4, &buff4[start], chunk);
        }
      }
      if (count == 0) return TRUE;
      start = (start + 1) << 2;
      start--;
    }
  }

  /// remainder should be only 6 or 2 bytes in the middle, with a chunk size of 4 or 2 or 1
  if (chunk > 4) return FALSE;

  if (count > 0) {
    uint16_t *buff2 = (uint16_t *)buff;
    if ((void *)buff2 == (void *)buff) {
      if (start > 0) {
        end = (ocount - start) >> 1;
        start >>= 1;
      } else end = ocount >> 1;
      for (; count > 0; count -= 2) {
        /// swap 2 bytes at a time from start and end
        uint16_t tmp2 = buff2[--end];
        if (chunk == 2) {
          buff2[end] = buff2[++start];
          buff2[start] = tmp2;
        }
        /// swap single bytes
        else {
          swab2(&buff2[++start], &buff2[end], 1);
          swab2(&tmp2, &buff2[start], 1);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

  if (count == 0) return TRUE;
  return FALSE;
}


