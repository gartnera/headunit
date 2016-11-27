#define LOGTAG "hu_uti"

#include "hu_uti.h"

#ifndef NDEBUG
  char * state_get (int state) {
    switch (state) {
      case hu_STATE_INITIAL:                                           // 0
        return ("hu_STATE_INITIAL");
      case hu_STATE_STARTIN:                                           // 1
        return ("hu_STATE_STARTIN");
      case hu_STATE_STARTED:                                           // 2
        return ("hu_STATE_STARTED");
      case hu_STATE_STOPPIN:                                           // 3
        return ("hu_STATE_STOPPIN");
      case hu_STATE_STOPPED:                                           // 4
        return ("hu_STATE_STOPPED");
    }
    return ("hu_STATE Unknown error");
  }
#endif

  #include <stdio.h>
  #include <stdlib.h>
  #include <stdarg.h>
  #include <stdint.h>
  #include <unistd.h>
  #include <time.h>

  #include <string.h>
  #include <signal.h>

  #include <pthread.h>

  #include <errno.h>

  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/stat.h>

  #include <dirent.h>                                                   // For opendir (), readdir (), closedir (), DIR, struct dirent.

  //#define   S2D_POLL_MS     100
  //#define   NET_PORT_S2D    2102
  //#define   NET_PORT_HCI    2112

  //#include "man_ver.h"


  int gen_server_loop_func (unsigned char * cmd_buf, int cmd_len, unsigned char * res_buf, int res_max);
  int gen_server_poll_func (int poll_ms);

    // Log stuff:

  int ena_log_extra   = 0;//1;//0;
  int ena_log_verbo   = 0;//1;
  int ena_log_debug   = 1;
  int ena_log_warni   = 1;
  int ena_log_error   = 1;

  int ena_log_aap_send = 0;

  // Enables for hex_dump:
  int ena_hd_hu_aad_dmp = 1;        // Higher level
  int ena_hd_tra_send   = 0;        // Lower  level
  int ena_hd_tra_recv   = 0;

  int ena_log_hexdu = 1;//1;    // Hex dump master enable
  int max_hex_dump  = 64;//32;

  #ifdef  LOG_FILE
  int logfd = -1;
  void logfile (char * log_line) {
    if (logfd < 0)
      logfd = open ("/sdcard/hulog", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    int written = -77;
    if (logfd >= 0)
      written = write (logfd, log_line, strlen (log_line));
  }
  #endif

  char * prio_get (int prio) {
    switch (prio) {
      case hu_LOG_EXT: return ("X");
      case hu_LOG_VER: return ("V");
      case hu_LOG_DEB: return ("D");
      case hu_LOG_WAR: return ("W");
      case hu_LOG_ERR: return ("E");
    }
    return ("?");
  }

  int hu_log (int prio, const char * tag, const char * func, const char * fmt, ...) {

    if (! ena_log_extra && prio == hu_LOG_EXT)
      return -1;
    if (! ena_log_verbo && prio == hu_LOG_VER)
      return -1;
    if (! ena_log_debug && prio == hu_LOG_DEB)
      return -1;
    if (! ena_log_warni && prio == hu_LOG_WAR)
      return -1;
    if (! ena_log_error && prio == hu_LOG_ERR)
      return -1;

    char tag_str [DEF_BUF] = {0};
    snprintf (tag_str, sizeof (tag_str), "%32.32s", func);

    va_list ap;
    va_start (ap, fmt); 
  #ifdef __ANDROID_API__
    __android_log_vprint (prio, tag_str, fmt, ap);
  #else
    char log_line [4096] = {0};
    va_list aq;
    va_start (aq, fmt); 
    int len = vsnprintf (log_line, sizeof (log_line), fmt, aq);
    time_t timet = time (NULL);
    const time_t * timep = & timet;
    char asc_time [DEF_BUF] = "";
    ctime_r (timep, asc_time);
    int len_time = strlen (asc_time);
    asc_time [len_time - 1] = 0;        // Remove trailing \n
    printf ("%s %s: %s:: %s\n", & asc_time [11], prio_get (prio), tag, log_line);
  #endif
    
  #ifdef  LOG_FILE
    char log_line [4096] = {0};
    va_list aq;
    va_start (aq, fmt); 
    int len = vsnprintf (log_line, sizeof (log_line), fmt, aq);
    strlcat (log_line, "\n", sizeof (log_line));
    logfile (log_line);
  #endif
    return (0);
  }

/*
  int loge (const char * fmt, ...) {
    printf ("E: ");
    va_list ap;
    va_start (ap, fmt); 
    vprintf (fmt, ap);
    printf ("\n");
    return (0);
  }
*/


    //

  #define MAX_ITOA_SIZE 32      // Int 2^32 need max 10 characters, 2^64 need 21

  char * itoa (int val, char * ret, int radix) {
    if (radix == 10)
      snprintf (ret, MAX_ITOA_SIZE - 1, "%d", val);
    else if (radix == 16)
      snprintf (ret, MAX_ITOA_SIZE - 1, "%x", val);
    else
      loge ("radix != 10 && != 16: %d", radix);
    return (ret);
  }

/*int noblock_set (int fd) {
    //#define IOCTL_METH
    #ifdef  IOCTL_METH
    int nbio = 1;
    errno = 0;
    int ret = ioctl (fd, FIONBIO, & nbio);
    if (ret == -1)
      loge ("noblock_set ioctl errno: %d (%s)", errno, strerror (errno));
    else
      logd ("noblock_set ioctl ret: %d", ret);
    #else
    errno = 0;
    int flags = fcntl (fd, F_GETFL);
    if (flags == -1) {
      loge ("noblock_set fcntl get errno: %d (%s)", errno, strerror (errno));
      flags = 0;
    }
    else
      logd ("noblock_set fcntl get flags: %d  nonblock flags: %d", flags, flags & O_NONBLOCK);
    errno = 0;
    int ret = fcntl (fd, F_SETFL, flags | O_NONBLOCK);
    if (ret == -1)
      loge ("noblock_set fcntl set errno: %d (%s)", errno, strerror (errno));
    else
      logd ("noblock_set fcntl set ret: %d", ret);
    errno = 0;
    flags = fcntl (fd, F_GETFL);
    if (flags == -1)
      loge ("noblock_set fcntl result get errno: %d (%s)", errno, strerror (errno));
    else
      logd ("noblock_set fcntl result get flags: %d  nonblock flags: %d", flags, flags & O_NONBLOCK);
    #endif
    return (0);
  }*/

/*
  int alt_usleep (long us) {    // usleep can't be used in some cases because it uses SIGALRM
    struct timespec delay;
    int err;
    delay.tv_sec = us / 1000000;
    delay.tv_nsec = 1000 * 1000 * (us % 1000000);
    errno = 0;
    do {
      err = nanosleep (& delay, & delay);
    } while (err < 0 && errno == EINTR);
  }

*/

  unsigned long us_get () {                                                      // !!!! Affected by time jumps ?
    struct timespec tspec = {0, 0};
    int res = clock_gettime (CLOCK_MONOTONIC, & tspec);
    //logd ("sec: %ld  nsec: %ld", tspec.tv_sec, tspec.tv_nsec);

    unsigned long microsecs = (tspec.tv_nsec / 1000L);
    microsecs += (tspec.tv_sec * 1000000L);

    return (microsecs);
  }

  unsigned long ms_get () {                                                      // !!!! Affected by time jumps ?
    struct timespec tspec = {0, 0};
    int res = clock_gettime (CLOCK_MONOTONIC, & tspec);
    //logd ("sec: %ld  nsec: %ld", tspec.tv_sec, tspec.tv_nsec);

    unsigned long millisecs = (tspec.tv_nsec / 1000000L);
    millisecs += (tspec.tv_sec * 1000L);       // remaining 22 bits good for monotonic time up to 4 million seconds =~ 46 days. (?? - 10 bits for 1024 ??)

    return (millisecs);
  }



  #define quiet_ms_sleep  ms_sleep

  unsigned long ms_sleep (unsigned long ms) {
/*
    usleep (ms * 1000L);
    return (0);
//*/

    struct timespec tm;
    tm.tv_sec = 0;
    tm.tv_sec = ms / 1000L;
	  tm.tv_nsec = (ms % 1000L) *  1000000L;
    //printf ("tm.tv_sec: %ld  tm.tv_nsec: %ld\n", tm.tv_sec, tm.tv_nsec);
    //logd ("tm.tv_sec: %ld  tm.tv_nsec: %ld", tm.tv_sec, tm.tv_nsec);

//    nanosleep (& tm, NULL);

    unsigned long ms_end = ms_get () + ms;
    unsigned long ctr = 0;
    while (ms_get () < ms_end) {
      usleep (32000L);
      //tm.tv_nsec = 1000000L;
      //nanosleep (& tm, NULL); 

      ctr ++;

      if (ctr > 25){//100L) {
        ctr = 0L;
//        printf (".\b");
      }
    }
    return (ms);

//*/

    return (ms);
  }


  static void * busy_thread (void * arg) {
    logd ("busy_thread start");

    while (1) {
    unsigned long ms = 1L;
//      usleep (ms * 1000L);
    struct timespec tm;
    tm.tv_sec = 0;
    tm.tv_sec = ms / 1000L;
	  tm.tv_nsec = (ms % 1000L) *  1000000L;
    //printf ("tm.tv_sec: %ld  tm.tv_nsec: %ld\n", tm.tv_sec, tm.tv_nsec);
    //logd ("tm.tv_sec: %ld  tm.tv_nsec: %ld", tm.tv_sec, tm.tv_nsec);
//    nanosleep (& tm, NULL);

      int ctr = 0;
      int max_cnt = 32;//512;   1024 too much on N9, 512 OK
      for (ctr = 0; ctr < max_cnt ; ctr ++) {
        ms = ms_get ();
      }

      printf (".");
    }

    pthread_exit (NULL);
    loge ("busy_thread exit 2");

    return (NULL);                                                      // Compiler friendly ; never reach
  }




  char user_dev [DEF_BUF] = "";
  
  char * user_char_dev_get (char * dir_or_dev, int user) {
    DIR  * dp;
    struct dirent * dirp;
    struct stat sb;
    int ret = 0;
    logd ("user_char_dev_get: %s  %d", dir_or_dev, user);
  
    errno = 0;
    ret = stat (dir_or_dev, & sb);                                        // Get file/dir status.
    if (ret == -1) {
      loge ("user_char_dev_get: dir_or_dev stat errno: %d (%s)", errno, strerror (errno));
      return (NULL);
    }
  
    if (S_ISCHR (sb.st_mode)) {                                           // If this is a character device...
      if (sb.st_uid == user) {                                            // If user match...
        //strlcpy (user_dev, dir_or_dev, sizeof (user_dev));
        //return (user_dev);                                              // Device found
        return (dir_or_dev);
      }
      return (NULL);
    }
  
    errno = 0;
    if ((dp = opendir (dir_or_dev)) == NULL) {                            // Open the directory. If error...
      loge ("user_char_dev_get: can't open dir_or_dev: %s  errno: %d (%s)", dir_or_dev, errno, strerror (errno));
      return (NULL);                                                      // Done w/ no result
    }
    //logd ("user_char_dev_get opened directory %s", dir_or_dev);
  
    while ((dirp = readdir (dp)) != NULL) {                               // For all files/dirs in this directory... (Could terminate with errno set !!)
      //logd ("user_char_dev_get: readdir returned file/dir %s", dirp->d_name);
  
      char filename [DEF_BUF] = {0};
      strlcpy (filename, dir_or_dev, sizeof (filename));
      strlcat (filename, "/", sizeof (filename));
      strlcat (filename, dirp->d_name, sizeof (filename));                // Set fully qualified filename
  
      errno = 0;
      ret = stat (filename, & sb);                                        // Get file/dir status.
      if (ret == -1) {
        loge ("user_char_dev_get: file stat errno: %d (%s)", errno, strerror (errno));
        continue;                                                         // Ignore/Next if can't get status
      }
  
      if (S_ISCHR (sb.st_mode)) {                                         // If this is a character device...
        //logd ("user_char_dev_get: dir %d", sb.st_mode);
        if (sb.st_uid == user) {                                          // If user match...
          closedir (dp);                                                  // Close the directory.
          strlcpy (user_dev, filename, sizeof (user_dev));
          return (user_dev);                                              // Device found
        }
      }
    }
    closedir (dp);                                                        // Close the directory.
    return (NULL);
  }


  char * upper_set (char * data) {
    int ctr = 0;
    int len = strlen (data);
    for (ctr = 0; ctr < len; ctr ++)
      data [ctr] = toupper (data [ctr]);
    return (data);
  }

  char * lower_set (char * data) {
    int ctr = 0;
    int len = strlen (data);
    for (ctr = 0; ctr < len; ctr ++)
      data [ctr] = tolower (data [ctr]);
    return (data);
  }


    // Find first file under subdir dir, with pattern pat. Put results in path_buf of size path_len:

  int file_find (char * dir, char * pat, char * path_buf, int path_len) {
    static int nest = 0;
    path_buf [0] = 0;
    if (nest == 0)
      logd ("file_find: %d %s %s", nest, dir, pat);
  
    nest ++;
    if (nest > 16) {                                                    // Routine is recursive; if more than 16 subdirectories nested... (/system/xbin/bb -> /system/xbin)
      logd ("file_find maximum nests: %d  dir: %s  path: %s", nest, dir, pat);
      nest --;
      return (0);                                                       // Done w/ no result
    }
  
    DIR  * dp;
    struct dirent * dirp;
    struct stat sb;
    int ret = 0;
  
    errno = 0;
    if ((dp = opendir (dir)) == NULL) {                                 // Open the directory. If error...
      if (errno == EACCES)                                              // EACCESS is a common annoyance, Even w/ SU presumably due to SELinux
        logd ("file_find: can't open directory %s  errno: %d (%s) (EACCES Permission denied)", dir, errno, strerror (errno));
      else
        logd ("file_find: can't open directory %s  errno: %d (%s)", dir, errno, strerror (errno));
      nest --;
      return (0);                                                       // Done w/ no result for this directory
    }
    //logd ("file_find opened directory %s", dir);
  
    while ((dirp = readdir (dp)) != NULL) {                             // For all files/dirs in this directory... (Could terminate with errno set !!)
      //logd ("file_find: readdir returned file/dir %s", dirp->d_name);
  
      if (strlen (dirp->d_name) == 1)
        if (dirp->d_name[0] == '.')
          continue;                                                     // Ignore/Next if "." current dir
  
      if (strlen (dirp->d_name) == 2)
        if (dirp->d_name[0] == '.' && dirp->d_name[1] == '.')
          continue;                                                     // Ignore/Next if ".." parent dir
  
      char filename[DEF_BUF] = {0};
      strlcpy (filename, dir, sizeof (filename));
      strlcat (filename, "/", sizeof (filename));
      strlcat (filename, dirp->d_name, sizeof (filename));              // Set fully qualified filename
  
      errno = 0;
      ret = stat (filename, &sb);                                       // Get file/dir status.
      if (ret == -1) {
        logd ("file_find: stat errno: %d (%s)", errno, strerror (errno));
        continue;                                                       // Ignore/Next if can't get status
      }
  
      if (S_ISDIR (sb.st_mode)) {                                       // If this is a directory...
        //logd ("file_find: dir %d", sb.st_mode);
        if (file_find (filename, pat, path_buf, path_len)) {            // Recursively call self: Go deeper to find the file, If found...
          closedir (dp);                                                // Close the directory.
          nest --;
          return (1);                                                   // File found
        }
      }
  
      else if (S_ISREG (sb.st_mode)) {                                  // If this is a regular file...
        //logd ("file_find: reg %d", sb.st_mode);
        int pat_len = strlen (pat);
        int filename_len = strlen (filename);
        if (filename_len >= pat_len) {
                                                                        // !! Case insensitive
          if (! strncasecmp (pat, & filename [filename_len - pat_len], pat_len)) {
            logd ("file_find pattern: %s  filename: %s", pat, filename);
            strlcpy (path_buf,filename, path_len);
            closedir (dp);                                              // Close the directory.
            nest --;
            return (1);                                                 // File found
          }
        }
      }
      else {
        logd ("file_find: unk %d", sb.st_mode);
      }
    }
    closedir (dp);                                                      // Close the directory.
    nest --;
    return (0);                                                         // Done w/ no result
  }



  int flags_file_get (const char * filename, int flags) {               // Return 1 if file, or directory, or device node etc. exists and we can open it

    int ret = 0;                                                        // 0 = File does not exist or is not accessible
    if ( file_get (filename)) {                                         // If file exists...
      errno = 0;
      int fd = open (filename, flags);
      if (fd < 0)
        loge ("flags_file_get open fd: %d  errno: %d (%s)", fd, errno, strerror (errno));
      else
        logd ("flags_file_get open fd: %d", fd);
      if (fd >= 0) {                                                    // If open success...
        ret = 1;                                                        // 1 = File exists and is accessible
        close (fd);
      }
    }
    logd ("flags_file_get ret: %d  filename: %s", ret, filename);
    return (ret);                                                       // 0 = File does not exist or is not accessible         // 1 = File exists and is accessible
  }

  int file_get (const char * filename) {                                // Return 1 if file, or directory, or device node etc. exists
    struct stat sb = {0};
    int ret = 0;                                                        // 0 = No file
    errno = 0;
    if (stat (filename, & sb) == 0) {                                   // If file exists...
      ret = 1;                                                          // 1 = File exists
//      logd ("file_get ret: %d  filename: %s", ret, filename);
    }
    else {
//      if (errno == ENOENT)                                              // 2
//        logd ("file_get ret: %d  filename: %s  errno ENOENT = No File/Dir", ret, filename);
//      else
//        loge ("file_get ret: %d  filename: %s  errno: %d (%s)", ret, filename, errno, strerror (errno));
    }
    return (ret);
  }

  int file_delete (const char * filename) {
    errno = 0;
    int ret = unlink (filename);
    if (ret)
      loge ("file_delete ret: %d  filename: %s  errno: %d (%s)", ret, filename, errno, strerror (errno));
    else
      logd ("file_delete ret: %d  filename: %s", ret, filename);
    return (ret);
  }
  int file_create (const char * filename) {
    int ret = flags_file_get (filename, O_CREAT);
    logd ("file_create flags_file_get ret: %d  filename: %s");
    return (ret);
  }


/*
  int chip_lock_val = 0;
  char * curr_lock_cmd = "none";
  int chip_lock_get (char * cmd) {
    return (0);
    int retries = 0;
    int max_msecs = 3030;
    int sleep_ms = 101; // 10
    while (retries ++ < max_msecs / sleep_ms) {
      chip_lock_val ++;
      if (chip_lock_val == 1) {
        curr_lock_cmd = cmd;
        return (0);
      }
      chip_lock_val --;
      if (chip_lock_val < 0)
        chip_lock_val = 0;
      loge ("sleep_ms: %d  retries: %d  cmd: %s  curr_lock_cmd: %s", sleep_ms, retries, cmd, curr_lock_cmd);
      ms_sleep (sleep_ms);
    }
    loge ("chip_lock_get retries exhausted");
    return (-1);
  }
  int chip_lock_ret () {
    if (chip_lock_val > 0)
      chip_lock_val --;
    if (chip_lock_val < 0)
      chip_lock_val = 0;
    return (0);
  }
*/


  #define HD_MW   256
  void hex_dump (char * prefix, int width, unsigned char * buf, int len) {
    if (0)//! strncmp (prefix, "AUDIO: ", strlen ("AUDIO: ")))
      len = len;
    else if (! ena_log_hexdu)
      return;
    //loge ("hex_dump prefix: \"%s\"  width: %d   buf: %p  len: %d", prefix, width, buf, len);

    if (buf == NULL || len <= 0)
      return;

    if (len > max_hex_dump)
      len = max_hex_dump;

    char tmp  [3 * HD_MW + 8] = "";                                     // Handle line widths up to HD_MW
    char line [3 * HD_MW + 8] = "";
    if (width > HD_MW)
      width = HD_MW;
    int i, n;
    line [0] = 0;

    if (prefix)
      //strlcpy (line, prefix, sizeof (line));
      strlcat (line, prefix, sizeof (line));

    snprintf (tmp, sizeof (tmp), " %8.8x ", 0);
    strlcat (line, tmp, sizeof (line));

    for (i = 0, n = 1; i < len; i ++, n ++) {                           // i keeps incrementing, n gets reset to 0 each line

      snprintf (tmp, sizeof (tmp), "%2.2x ", buf [i]);
      strlcat (line, tmp, sizeof (line));                               // Append 2 bytes hex and space to line

      if (n == width) {                                                 // If at specified line width
        n = 0;                                                          // Reset position in line counter
        logd (line);                                                    // Log line

        line [0] = 0;
        if (prefix)
          //strlcpy (line, prefix, sizeof (line));
          strlcat (line, prefix, sizeof (line));

        //snprintf (tmp, sizeof (tmp), " %8.8x ", i + 1);
        snprintf (tmp, sizeof (tmp), "     %4.4x ", i + 1);
        strlcat (line, tmp, sizeof (line));
      }
      else if (i == len - 1)                                            // Else if at last byte
        logd (line);                                                    // Log line
    }
  }

/*
  static char sys_cmd [32768] = {0};
  static int sys_commit () {
    int ret = sys_run (sys_cmd);                                        // Run
    sys_cmd [0] = 0;                                                    // Done, so zero
    return (ret);
  }
  static int cached_sys_run (char * new_cmd) {                          // Additive single string w/ commit version
    char cmd [512] = {0};
    if (strlen (sys_cmd) == 0)                                          // If first command since commit
      snprintf (cmd, sizeof (cmd), "%s", new_cmd);
    else
      snprintf (cmd, sizeof (cmd), " ; %s", new_cmd);
    strlcat (sys_cmd, cmd, sizeof (sys_cmd));
    int ret = sys_commit ();                                            // Commit every command now, due to GS3/Note problems
    return (ret);
  }
*/

/*
  int sys_run (char * cmd) {
    int ret = system (cmd);                                             // !! Binaries like ssd that write to stdout cause C system() to crash !
    logd ("sys_run ret: %d  cmd: \"%s\"", ret, cmd);
    return (ret);
  }
  int insmod_shell = 1;
*/
  int util_insmod (char * module) {    // ("/system/lib/modules/radio-iris-transport.ko");
    int ret = 0;
/*
    if (insmod_shell) {
      char cmd [DEF_BUF] = "insmod ";
      strlcat (cmd, module, sizeof (cmd));
      strlcat (cmd, " >/dev/null 2>/dev/null", sizeof (cmd));
      ret = sys_run (cmd);
      loge ("util_insmod module: \"%s\"  ret: %d  cmd: \"%s\"", module, ret, cmd);
    }
    else {
*/
      ret = insmod_internal (module);
      if (ret)
        loge ("util_insmod module: \"%s\"  ret: %d", module, ret);
      else
        logd ("util_insmod module: \"%s\"  success ret: %d", module, ret);
/*
    }
*/
    return (ret);
  }

  int file_write_many (const char * filename, int * pfd, char * data, int len, int flags) {
    logd ("file_write_many filename: %d  * pfd: %d  len: %d  flags: %d", filename, * pfd, len, flags);
    if (len > 0 && len == strlen (data))
      logd ("file_write_many data: \"%s\"", data);                      // All we use this for is ascii strings, so OK to display, else hexdump
    else if (len > 0)
      loge ("file_write_many NEED HEXDUMP !!");
    else
      logd ("file_write_many no data");
    int ret = 0;
    errno = 0;
    if (* pfd < 0) {
      if (flags | O_CREAT)
        * pfd = open (filename, flags, S_IRWXU | S_IRWXG | S_IRWXO);        //O_WRONLY);                                   // or O_RDWR    (But at least one codec_reg is write only, so leave!)
      else
        * pfd = open (filename, flags);
    }

    if (* pfd < 0) {
      loge ("file_write_many open * pfd: %d  flags: %d  errno: %d (%s)  len: %d  filename: %s", * pfd, flags, errno, strerror (errno), len, filename);
      return (0);
    }

    logd ("file_write_many open * pfd: %d", * pfd);
    int written = 0;
    errno = 0;
    if (len > 0)
      written = write (* pfd, data, len);
    if (written != len)
      loge ("file_write_many written: %d  flags: %d  errno: %d (%s)  len: %d  filename: %s", written, flags, errno, strerror (errno), len, filename);
    else
      logd ("file_write_many written: %d  flags: %d  len: %d  filename: %s", written, flags, len, filename);
    return (written);
  }

  int file_write (const char * filename, char * data, int len, int flags) {
    int ret = 0;
    int fd = -1;

    ret = file_write_many (filename, & fd, data, len, flags);
    logd ("file_write -> file_write_many ret: %d  flags: %d  len: %d  filename: %s", ret, flags, len, filename);

    errno = 0;
    ret = -1;
    if (fd >= 0)
      ret = close (fd);
    if (ret < 0)
      loge ("file_write close ret: %d  errno: %d (%s)", ret, errno, strerror (errno));
    else
      logd ("file_write close ret: %d", ret);
    return (ret);
  }

  void * file_read (const char * filename, ssize_t * size_ret) {
    int ret, fd;
    struct stat sb;
    ssize_t size;
    void * buffer = NULL;

    /* open the file */
    fd = open (filename, O_RDONLY);
    if (fd < 0)
      return (NULL);

    /* find out how big it is */
    if (fstat (fd, & sb) < 0)
      goto bail;
    size = sb.st_size;

    /* allocate memory for it to be read into */
    buffer = malloc (size);
    if (! buffer)
      goto bail;

    /* slurp it into our buffer */
    ret = read (fd, buffer, size);
    if (ret != size)
      goto bail;

    /* let the caller know how big it is */
    * size_ret = size;

  bail:
    close (fd);
    return (buffer);
  }

  extern int init_module (void *, unsigned long, const char *);

  int insmod_internal (const char * filename) {
    void * file;
    ssize_t size = 0;
    char opts [1024];
    int ret;

    file = file_read (filename, & size); // read the file into memory
    if (! file) {
      loge ("insmod_internal can't open \"%s\"", filename);
      return (-1);
    }

    opts [0] = '\0';
/*
    if (argc > 2) {
      int i, len;
      char *end = opts + sizeof(opts) - 1;
      char *ptr = opts;

      for (i = 2; (i < argc) && (ptr < end); i++) {
          len = MIN(strlen(argv[i]), end - ptr);
          memcpy(ptr, argv[i], len);
          ptr += len;
          *ptr++ = ' ';
      }
      *(ptr - 1) = '\0';
    }
*/

    errno = 0;
    ret = init_module (file, size, opts);   // pass it to the kernel
    if (ret != 0) {
      if (errno == EEXIST) { // 17
        ret = 0;
        logd ("insmod_internal: init_module '%s' failed EEXIST because already loaded", filename);
      }
      else
        loge ("insmod_internal: init_module '%s' failed errno: %d (%s)", filename, errno, strerror (errno));
    }

    free (file);    // free the file buffer
    return ret;
 }


/*char * holder_id = "None";
  int lock_open (const char * id, volatile int * lock, int tmo) {
    int attempts = 0;
    volatile int lock_val = * lock;
  
    //logd ("lock_open %s  lock_val: %d  lock: %d  tmo: %d", id, lock_val, * lock, tmo);
    int start_time   = ms_get ();                                         // Set start time
    int timeout_time = start_time + tmo;                                  // Set end/timeout time
    int elapsed_time = ms_get () - start_time;
    long alt_sleep_ctr = 0;
  
    while (ms_get () < timeout_time) {                                    // Until timeout
      if (* lock < 0) {                                                   // If negative...
        * lock = 0;                                                       // Fix
        loge ("!!!!!!!!! lock_open clear negative lock id: %s  holder_id: %s", id, holder_id, attempts);
      }
  
      while ((* lock) && ms_get () < timeout_time) {                      // While the lock is NOT acquired and we have not timed out...
        if (elapsed_time > 100)
          loge ("lock_open sleep 10 ms id: %s  holder_id: %s  attempts: %d  lock_val: %d  lock: %d  elapsed_time: %d", id, holder_id, attempts, lock_val, * lock, elapsed_time);// Else we lost attempt
        if (attempts) {                                                   // If not 1st try...
          alt_sleep_ctr = 0;
          while (alt_sleep_ctr ++ < 10000);       // !!!! Because ms_sleep was crashing ?
        }
        else
          ms_sleep (10);                                                  // Sleep a while then try again
      }
      elapsed_time = ms_get () - start_time;
  
      (* lock) ++;                                                        // Attempt to acquire lock (1st or again)
      lock_val = (* lock);
      if (lock_val == 1) {                                                // If success...
        if (attempts)                                                     // If not 1st try...
          loge ("lock_open %s success id: %s  holder_id: %s  attempts: %d  lock_val: %d  lock: %d  elapsed_time: %d", id, holder_id, attempts, lock_val, * lock, elapsed_time);
        holder_id = (char *) id;                                          // We are last holder (though done now)
        return (0);                                                       // Lock acquired
      }
      else {
        (* lock) --;                                                      // Release lock attempt
        loge ("lock_open lost id: %s  holder_id: %s  attempts: %d  lock_val: %d  lock: %d  elapsed_time: %d", id, holder_id, attempts, lock_val, * lock, elapsed_time);// Else we lost attempt
        attempts ++;
      }
    }
    lock_val = (* lock);
    loge ("lock_open timeout id: %s  holder_id: %s  attempts: %d  lock_val: %d  lock: %d  elapsed_time: %d", id, holder_id, attempts, lock_val, * lock, elapsed_time);
    return (-1);                                                          // Error, no lock
  }
  
  int lock_close (const char * id, volatile int * lock) {
    //logd ("lock_close %s  lock: %d", id, (* lock));
    (* lock) --;
    //logd ("lock_close %s 2  lock: %d", id, (* lock));
    return (0);
  }*/


  #define ERROR_CODE_NUM 56
  static const char * error_code_str [ERROR_CODE_NUM + 1] = {
    "Success",
    "Unknown HCI Command",
    "Unknown Connection Identifier",
    "Hardware Failure",
    "Page Timeout",
    "Authentication Failure",
    "PIN or Key Missing",
    "Memory Capacity Exceeded",
    "Connection Timeout",
    "Connection Limit Exceeded",
    "Synchronous Connection to a Device Exceeded",
    "ACL Connection Already Exists",
    "Command Disallowed",
    "Connection Rejected due to Limited Resources",
    "Connection Rejected due to Security Reasons",
    "Connection Rejected due to Unacceptable BD_ADDR",
    "Connection Accept Timeout Exceeded",
    "Unsupported Feature or Parameter Value",
    "Invalid HCI Command Parameters",
    "Remote User Terminated Connection",
    "Remote Device Terminated Connection due to Low Resources",
    "Remote Device Terminated Connection due to Power Off",
    "Connection Terminated by Local Host",
    "Repeated Attempts",
    "Pairing Not Allowed",
    "Unknown LMP PDU",
    "Unsupported Remote Feature / Unsupported LMP Feature",
    "SCO Offset Rejected",
    "SCO Interval Rejected",
    "SCO Air Mode Rejected",
    "Invalid LMP Parameters",
    "Unspecified Error",
    "Unsupported LMP Parameter Value",
    "Role Change Not Allowed",
    "LMP Response Timeout",
    "LMP Error Transaction Collision",
    "LMP PDU Not Allowed",
    "Encryption Mode Not Acceptable",
    "Link Key Can Not be Changed",
    "Requested QoS Not Supported",
    "Instant Passed",
    "Pairing with Unit Key Not Supported",
    "Different Transaction Collision",
    "Reserved",
    "QoS Unacceptable Parameter",
    "QoS Rejected",
    "Channel Classification Not Supported",
    "Insufficient Security",
    "Parameter out of Mandatory Range",
    "Reserved",
    "Role Switch Pending",
    "Reserved",
    "Reserved Slot Violation",
    "Role Switch Failed",
    "Extended Inquiry Response Too Large",
    "Simple Pairing Not Supported by Host",
    "Host Busy - Pairing",
  };

  const char * hci_err_get (uint8_t status) {
    const char * str;
    if (status <= ERROR_CODE_NUM)
      str = error_code_str [status];
    else
      str = "Unknown HCI Error";
    return (str);
  }



  #include <netinet/in.h>
  #include <netdb.h> 

//For REUSEADDR only
//#define SK_NO_REUSE     0
//#define SK_CAN_REUSE    1


  #ifndef SK_FORCE_REUSE
    #define SK_FORCE_REUSE  2
  #endif

  #ifndef SO_REUSEPORT
    #define SO_REUSEPORT 15
  #endif

    // ?? Blocked in Android ?          sock_tmo_set setsockopt SO_REUSEPORT errno: 92 (Protocol not available)

  int sock_reuse_set (int fd) {
    errno = 0;
    int val = SK_FORCE_REUSE;//SK_CAN_REUSE;
    int ret = setsockopt (fd, SOL_SOCKET, SO_REUSEADDR, & val, sizeof (val));
    if (ret != 0)
      loge ("sock_tmo_set setsockopt SO_REUSEADDR errno: %d (%s)", errno, strerror (errno));
    else
      logd ("sock_tmo_set setsockopt SO_REUSEADDR Success");
    return (0);
  }

  int sock_tmo_set (int fd, int tmo) {                                 // tmo = timeout in milliseconds
    struct timeval tv = {0, 0};
    tv.tv_sec = tmo / 1000;                                               // Timeout in seconds
    tv.tv_usec = (tmo % 1000) * 1000;
    errno = 0;
    int ret = setsockopt (fd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) & tv, sizeof (struct timeval));
    if (ret != 0)
      loge ("sock_tmo_set setsockopt SO_RCVTIMEO errno: %d (%s)", errno, strerror (errno));
    else
      logv ("sock_tmo_set setsockopt SO_RCVTIMEO Success");
    //errno = 0;
    //ret = setsockopt (fd, SOL_SOCKET, SO_SNDTIMEO, (struct timeval *) & tv, sizeof (struct timeval));
    //if (ret != 0) {
    //  loge ("timeout_set setsockopt SO_SNDTIMEO errno: %d (%s)", errno, strerror (errno));
    //}
    return (0);
  }
  

  int pid_get (char * cmd, int start_pid) {
    DIR  * dp;
    struct dirent * dirp;
    FILE * fdc;
    struct stat sb;
    int pid = 0;
    int ret = 0;
    logd ("pid_get: %s  start_pid: %d", cmd, start_pid);

    errno = 0;
    if ((dp = opendir ("/proc")) == NULL) {                             // Open the /proc directory. If error...
      loge ("pid_get: opendir errno: %d (%s)", errno, strerror (errno));
      return (0);                                                       // Done w/ no process found
    }
    while ((dirp = readdir (dp)) != NULL) {                             // For all files/dirs in this directory... (Could terminate with errno set !!)
      //logd ("pid_get: readdir: %s", dirp->d_name);
      errno = 0;
      pid = atoi (dirp->d_name);                                        // pid = directory name string to integer
      if (pid <= 0) {                                                   // Ignore non-numeric directories
        //loge ("pid_get: not numeric ret: %d  errno: %d (%s)", pid, errno, strerror (errno));
        continue;
      }
      if (pid < start_pid) {                                            // Ignore PIDs we have already checked. Depends on directories in PID order which seems to always be true
        //loge ("pid_get: pid < start_pid");
        continue;
      }

      //logd ("pid_get: test pid: %d", pid);
    
      char fcmdline [DEF_BUF] = "/proc/";
      strlcat (fcmdline, dirp->d_name, sizeof (fcmdline));
      errno = 0;
      ret = stat (fcmdline, & sb);                                      // Get file/dir status.
      if (ret == -1) {
        logd ("pid_get: stat errno: %d (%s)", errno, strerror (errno)); // Common: pid_get: stat errno: 2 = ENOENT
        continue;
      }
      if (S_ISDIR (sb.st_mode)) {                                       // If this is a directory...
        //logd ("pid_get: dir %d", sb.st_mode);
        char cmdline [DEF_BUF] = {0};
        strlcat (fcmdline, "/cmdline", sizeof (fcmdline));
        errno = 0;
        if ((fdc = fopen (fcmdline, "r")) == NULL) {                    // Open /proc/???/cmdline file read-only, If error...
          loge ("pid_get: fopen errno: %d (%s)", errno, strerror (errno));
          continue;
        }
        errno = 0;
        ret = fread (cmdline, sizeof (char), sizeof (cmdline) - 1, fdc);// Read
        if (ret < 0 || ret > sizeof (cmdline) - 1) {                    // If error...
          loge ("pid_get fread ret: %d  errno: %d (%s)", ret, errno, strerror (errno));
          fclose (fdc);
          continue;
        }
        cmdline [ret] = 0;
        fclose (fdc);
        int cmd_len = strlen (cmd);
        ret = strlen (cmdline);                                         // The buffer includes a trailing 0, so adjust ret to actual string length (in case not always true) (Opts after !)
        //logd ("pid_get: cmdline bytes: %d  cmdline: %s", ret, cmdline);

        if (ret >= cmd_len) {                                           // Eg: ret = strlen ("/bin/a") = 6, cmd_len = strlen ("a") = 1, compare 1 at cmd_line[5]

          if (! strcmp (cmd, & cmdline [ret - cmd_len])) {              // If a matching process name
            logd ("pid_get: got pid: %d for cmdline: %s  start_pid: %d", pid, cmdline, start_pid);
            closedir (dp);                                              // Close the directory.
            return (pid);                                               // SUCCESS: Done w/ pid
          }
        }
      }
      else if (S_ISREG (sb.st_mode)) {                                  // If this is a regular file...
        loge ("pid_get: reg %d", sb.st_mode);
      }
      else {
        loge ("pid_get: unk %d", sb.st_mode);
      }
    }
    closedir (dp);                                                      // Close the directory.
    return (0);                                                         // Done w/ no PID found
  }

  int kill_gentle_first = 1;
  int pid_kill (int pid, int brutal, char * cmd_to_verify) {
    logd ("pid_kill pid: %d  brutal: %d", pid, brutal);
    int ret = 0;
    int sig = SIGTERM;
    if (brutal) {
      if (kill_gentle_first) {
        errno = 0;
        ret = kill (pid, sig);
        if (ret) {
          loge ("pid_kill kill_gentle_first kill() errno: %d (%s)", errno, strerror (errno));
        }
        else {
          logd ("pid_kill kill_gentle_first kill() success");
          errno = 0;
          int new_pid_check1 = pid_get (cmd_to_verify, pid);
          if (new_pid_check1 == pid) {
            loge ("pid_kill kill() success detected but same new_pid_check: %d  errno: %d (%s)", new_pid_check1, errno, strerror (errno));  // Fall through to brutal kill
          }
          else {
            logd ("Full Success pid != new_pid_check1: %d  errno: %d (%s)", new_pid_check1, errno, strerror (errno));
            return (ret);
          }
        }
      }
      sig = SIGKILL;
    }
    errno = 0;
    ret = kill (pid, sig);
    if (ret) {
      loge ("pid_kill kill() errno: %d (%s)", errno, strerror (errno));
    }
    else {
      logd ("pid_kill kill() success");
      errno = 0;
      int new_pid_check2 = pid_get (cmd_to_verify, pid);
      if (new_pid_check2 == pid)
        loge ("pid_kill kill() success detected but same new_pid_check2: %d", new_pid_check2);
      else
        logd ("pid != new_pid_check: %d  errno: %d (%s)", new_pid_check2, errno, strerror (errno));
    }
    return (ret);
  }

  int killall (char * cmd, int brutal) {                                // Kill all OTHER instances of named process, except our own, if the same
    int ret = 0;
    int pid = 0;
    int our_pid = getpid ();
    logd ("killall cmd: %s  brutal: %d  our_pid: %d", cmd, brutal, our_pid);
    int idx = 0;
    int num_kill_attempts = 0;
    int max_kill_attempts = 16;                                         // Max of 16 kills (was to prevent blocking if can't kill, now just to limit kills)

    for (idx = 0; idx < max_kill_attempts; idx ++) {                    // For maximum kills...

      pid = pid_get (cmd, pid + 1);                                     // Get PID starting at last found PID + 1

      if (pid == our_pid) {
        logd ("pid == our_pid");                                        // If us, just log
      }
      else if (pid > 0) {
        ret = pid_kill (pid, brutal, cmd);                              // Else if valid external PID, kill it
        num_kill_attempts ++;
      }
      else {
        break;                                                          // Else if end of PID search, terminate loop
      }
    }
    logd ("killall num_kill_attempts: %d", num_kill_attempts);
    return (num_kill_attempts);
  }





    // Buffers: Audio, Video, identical code, should generalize

  #define aud_buf_BUFS_SIZE    65536 * 4      // Up to 256 Kbytes
  int aud_buf_bufs_size = aud_buf_BUFS_SIZE;

  #define   NUM_aud_buf_BUFS   16            // Maximum of NUM_aud_buf_BUFS - 1 in progress; 1 is never used
  int num_aud_buf_bufs = NUM_aud_buf_BUFS;

  char aud_buf_bufs [NUM_aud_buf_BUFS] [aud_buf_BUFS_SIZE];

  int aud_buf_lens [NUM_aud_buf_BUFS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  int aud_buf_buf_tail = 0;    // Tail is next index for writer to write to.   If head = tail, there is no info.
  int aud_buf_buf_head = 0;    // Head is next index for reader to read from.

  int aud_buf_errs = 0;
  int aud_max_bufs = 0;
  int aud_sem_tail = 0;
  int aud_sem_head = 0;

  char * aud_write_tail_buf_get (int len) {                          // Get tail buffer to write to

    if (len > aud_buf_BUFS_SIZE) {
      loge ("!!!!!!!!!! aud_write_tail_buf_get too big len: %d", len);   // E/aud_write_tail_buf_get(10699): !!!!!!!!!! aud_write_tail_buf_get too big len: 66338
      return (NULL);
    }

    int bufs = aud_buf_buf_tail - aud_buf_buf_head;
    if (bufs < 0)                                                       // If underflowed...
      bufs += num_aud_buf_bufs;                                         // Wrap
    //logd ("aud_write_tail_buf_get start bufs: %d  head: %d  tail: %d", bufs, aud_buf_buf_head, aud_buf_buf_tail);

    if (bufs > aud_max_bufs)                                            // If new maximum buffers in progress...
      aud_max_bufs = bufs;                                              // Save new max
    if (bufs >= num_aud_buf_bufs - 1) {                                 // If room for another (max = NUM_aud_buf_BUFS - 1)
      loge ("aud_write_tail_buf_get out of aud_buf_bufs");
      aud_buf_errs ++;
      //aud_buf_buf_tail = aud_buf_buf_head = 0;                        // Drop all buffers
      return (NULL);
    }

    int max_retries = 4;
    int retries = 0;
    for (retries = 0; retries < max_retries; retries ++) {
      aud_sem_tail ++;
      if (aud_sem_tail == 1)
        break;
      aud_sem_tail --;
      loge ("aud_sem_tail wait");
      ms_sleep (10);
    }
    if (retries >= max_retries) {
      loge ("aud_sem_tail could not be acquired");
      return (NULL);
    }

    if (aud_buf_buf_tail < 0 || aud_buf_buf_tail > num_aud_buf_bufs - 1)   // Protect
      aud_buf_buf_tail &= num_aud_buf_bufs - 1;

    aud_buf_buf_tail ++;

    if (aud_buf_buf_tail < 0 || aud_buf_buf_tail > num_aud_buf_bufs - 1)
      aud_buf_buf_tail &= num_aud_buf_bufs - 1;

    char * ret = aud_buf_bufs [aud_buf_buf_tail];
    aud_buf_lens [aud_buf_buf_tail] = len;

    //logd ("aud_write_tail_buf_get done  ret: %p  bufs: %d  tail len: %d  head: %d  tail: %d", ret, bufs, len, aud_buf_buf_head, aud_buf_buf_tail);

    aud_sem_tail --;

    return (ret);
  }

  char * aud_read_head_buf_get (int * len) {                              // Get head buffer to read from

    if (len == NULL) {
      loge ("!!!!!!!!!! aud_read_head_buf_get");
      return (NULL);
    }
    * len = 0;

    int bufs = aud_buf_buf_tail - aud_buf_buf_head;
    if (bufs < 0)                                                       // If underflowed...
      bufs += num_aud_buf_bufs;                                          // Wrap
    //logd ("aud_read_head_buf_get start bufs: %d  head: %d  tail: %d", bufs, aud_buf_buf_head, aud_buf_buf_tail);

    if (bufs <= 0) {                                                    // If no buffers are ready...
      if (ena_log_extra)
        logd ("aud_read_head_buf_get no aud_buf_bufs");
      //aud_buf_errs ++;  // Not an error; just no data
      //aud_buf_buf_tail = aud_buf_buf_head = 0;                          // Drop all buffers
      return (NULL);
    }

    int max_retries = 4;
    int retries = 0;
    for (retries = 0; retries < max_retries; retries ++) {
      aud_sem_head ++;
      if (aud_sem_head == 1)
        break;
      aud_sem_head --;
      loge ("aud_sem_head wait");
      ms_sleep (10);
    }
    if (retries >= max_retries) {
      loge ("aud_sem_head could not be acquired");
      return (NULL);
    }

    if (aud_buf_buf_head < 0 || aud_buf_buf_head > num_aud_buf_bufs - 1)   // Protect
      aud_buf_buf_head &= num_aud_buf_bufs - 1;

    aud_buf_buf_head ++;

    if (aud_buf_buf_head < 0 || aud_buf_buf_head > num_aud_buf_bufs - 1)
      aud_buf_buf_head &= num_aud_buf_bufs - 1;

    char * ret = aud_buf_bufs [aud_buf_buf_head];
    * len = aud_buf_lens [aud_buf_buf_head];

    //logd ("aud_read_head_buf_get done  ret: %p  bufs: %d  head len: %d  head: %d  tail: %d", ret, bufs, * len, aud_buf_buf_head, aud_buf_buf_tail);

    aud_sem_head --;

    return (ret);
  }



  #define vid_buf_BUFS_SIZE    65536 * 4      // Up to 256 Kbytes
  int vid_buf_bufs_size = vid_buf_BUFS_SIZE;

  #define   NUM_vid_buf_BUFS   16            // Maximum of NUM_vid_buf_BUFS - 1 in progress; 1 is never used
  int num_vid_buf_bufs = NUM_vid_buf_BUFS;

  char vid_buf_bufs [NUM_vid_buf_BUFS] [vid_buf_BUFS_SIZE];

  int vid_buf_lens [NUM_vid_buf_BUFS] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  int vid_buf_buf_tail = 0;    // Tail is next index for writer to write to.   If head = tail, there is no info.
  int vid_buf_buf_head = 0;    // Head is next index for reader to read from.

  int vid_buf_errs = 0;
  int vid_max_bufs = 0;
  int vid_sem_tail = 0;
  int vid_sem_head = 0;

  char * vid_write_tail_buf_get (int len) {                          // Get tail buffer to write to

    if (len > vid_buf_BUFS_SIZE) {
      loge ("!!!!!!!!!! vid_write_tail_buf_get too big len: %d", len);   // E/vid_write_tail_buf_get(10699): !!!!!!!!!! vid_write_tail_buf_get too big len: 66338
      return (NULL);
    }

    int bufs = vid_buf_buf_tail - vid_buf_buf_head;
    if (bufs < 0)                                                       // If underflowed...
      bufs += num_vid_buf_bufs;                                         // Wrap
    //logd ("vid_write_tail_buf_get start bufs: %d  head: %d  tail: %d", bufs, vid_buf_buf_head, vid_buf_buf_tail);

    if (bufs > vid_max_bufs)                                            // If new maximum buffers in progress...
      vid_max_bufs = bufs;                                              // Save new max
    if (bufs >= num_vid_buf_bufs - 1) {                                 // If room for another (max = NUM_vid_buf_BUFS - 1)
      loge ("vid_write_tail_buf_get out of vid_buf_bufs");
      vid_buf_errs ++;
      //vid_buf_buf_tail = vid_buf_buf_head = 0;                        // Drop all buffers
      return (NULL);
    }

    int max_retries = 4;
    int retries = 0;
    for (retries = 0; retries < max_retries; retries ++) {
      vid_sem_tail ++;
      if (vid_sem_tail == 1)
        break;
      vid_sem_tail --;
      loge ("vid_sem_tail wait");
      ms_sleep (10);
    }
    if (retries >= max_retries) {
      loge ("vid_sem_tail could not be acquired");
      return (NULL);
    }

    if (vid_buf_buf_tail < 0 || vid_buf_buf_tail > num_vid_buf_bufs - 1)   // Protect
      vid_buf_buf_tail &= num_vid_buf_bufs - 1;

    vid_buf_buf_tail ++;

    if (vid_buf_buf_tail < 0 || vid_buf_buf_tail > num_vid_buf_bufs - 1)
      vid_buf_buf_tail &= num_vid_buf_bufs - 1;

    char * ret = vid_buf_bufs [vid_buf_buf_tail];
    vid_buf_lens [vid_buf_buf_tail] = len;

    //logd ("vid_write_tail_buf_get done  ret: %p  bufs: %d  tail len: %d  head: %d  tail: %d", ret, bufs, len, vid_buf_buf_head, vid_buf_buf_tail);

    vid_sem_tail --;

    return (ret);
  }

  char * vid_read_head_buf_get (int * len) {                              // Get head buffer to read from

    if (len == NULL) {
      loge ("!!!!!!!!!! vid_read_head_buf_get");
      return (NULL);
    }
    * len = 0;

    int bufs = vid_buf_buf_tail - vid_buf_buf_head;
    if (bufs < 0)                                                       // If underflowed...
      bufs += num_vid_buf_bufs;                                          // Wrap
    //logd ("vid_read_head_buf_get start bufs: %d  head: %d  tail: %d", bufs, vid_buf_buf_head, vid_buf_buf_tail);

    if (bufs <= 0) {                                                    // If no buffers are ready...
      if (ena_log_extra)
        logd ("vid_read_head_buf_get no vid_buf_bufs");
      //vid_buf_errs ++;  // Not an error; just no data
      //vid_buf_buf_tail = vid_buf_buf_head = 0;                          // Drop all buffers
      return (NULL);
    }

    int max_retries = 4;
    int retries = 0;
    for (retries = 0; retries < max_retries; retries ++) {
      vid_sem_head ++;
      if (vid_sem_head == 1)
        break;
      vid_sem_head --;
      loge ("vid_sem_head wait");
      ms_sleep (10);
    }
    if (retries >= max_retries) {
      loge ("vid_sem_head could not be acquired");
      return (NULL);
    }

    if (vid_buf_buf_head < 0 || vid_buf_buf_head > num_vid_buf_bufs - 1)   // Protect
      vid_buf_buf_head &= num_vid_buf_bufs - 1;

    vid_buf_buf_head ++;

    if (vid_buf_buf_head < 0 || vid_buf_buf_head > num_vid_buf_bufs - 1)
      vid_buf_buf_head &= num_vid_buf_bufs - 1;

    char * ret = vid_buf_bufs [vid_buf_buf_head];
    * len = vid_buf_lens [vid_buf_buf_head];

    //logd ("vid_read_head_buf_get done  ret: %p  bufs: %d  head len: %d  head: %d  tail: %d", ret, bufs, * len, vid_buf_buf_head, vid_buf_buf_tail);

    vid_sem_head --;

    return (ret);
  }


//#endif  //#ifndef UTILS_INCLUDED


    // Client/Server:

  //#ifdef  CS_AF_UNIX                                                      // For Address Family UNIX sockets
  //#include <sys/un.h>
  //#else                                                                   // For Address Family NETWORK sockets

    // Unix datagrams requires other write permission for /dev/socket, or somewhere else (ext, not FAT) writable.

  //#define CS_AF_UNIX        // Use network sockets to avoid filesystem permission issues w/ Unix Domain Address Family sockets
  #define CS_DGRAM            // Use datagrams, not streams/sessions

  #ifdef  CS_AF_UNIX                                                      // For Address Family UNIX sockets
  //#include <sys/un.h>
  #define DEF_API_SRVSOCK    "/dev/socket/srv_spirit"
  #define DEF_API_CLISOCK    "/dev/socket/cli_spirit"
  char api_srvsock [DEF_BUF] = DEF_API_SRVSOCK;
  char api_clisock [DEF_BUF] = DEF_API_CLISOCK;
  #define CS_FAM   AF_UNIX

  #else                                                                   // For Address Family NETWORK sockets
  //#include <netinet/in.h>
  //#include <netdb.h> 
  #define CS_FAM   AF_INET
  #endif

  #ifdef  CS_DGRAM
  #define CS_SOCK_TYPE    SOCK_DGRAM
  #else
  #define CS_SOCK_TYPE    SOCK_STREAM
  #endif

  #define   RES_DATA_MAX  1280


#ifdef  GENERIC_CLIENT
#ifndef  GENERIC_CLIENT_INCLUDED
#define  GENERIC_CLIENT_INCLUDED

    // Generic IPC API:

  int gen_client_cmd (unsigned char * cmd_buf, int cmd_len, unsigned char * res_buf, int res_max, int net_port, int rx_tmo) {
    logv ("net_port: %d  cmd_buf: \"%s\"  cmd_len: %d", net_port, cmd_buf, cmd_len);
    static int sockfd = -1;
    int res_len = 0, written = 0, ctr = 0;
    static socklen_t srv_len = 0;
  #ifdef  CS_AF_UNIX
    static struct sockaddr_un  srv_addr;
    #ifdef  CS_DGRAM
    #define   CS_DGRAM_UNIX
      struct sockaddr_un  cli_addr;                                     // Unix datagram sockets must be bound; no ephemeral sockets.
      socklen_t cli_len = 0;
    #endif
  #else
    //struct hostent *hp;
    struct sockaddr_in  srv_addr, cli_addr;
    socklen_t cli_len = 0;
  #endif
  
    if (sockfd < 0) {
      errno = 0;
      if ((sockfd = socket (CS_FAM, CS_SOCK_TYPE, 0)) < 0) {            // Get an ephemeral, unbound socket
        loge ("gen_client_cmd: socket errno: %d (%s)", errno, strerror (errno));
        return (0);
      }
    #ifdef  CS_DGRAM_UNIX                                               // Unix datagram sockets must be bound; no ephemeral sockets.
      strlcpy (api_clisock, DEF_API_CLISOCK, sizeof (api_clisock));
      char itoa_ret [MAX_ITOA_SIZE] = {0};
      strlcat (api_clisock, itoa (net_port, itoa_ret, 10), sizeof (api_clisock));
      unlink (api_clisock);                                             // Remove any lingering client socket
      memset ((char *) & cli_addr, sizeof (cli_addr), 0);
      cli_addr.sun_family = AF_UNIX;
      strlcpy (cli_addr.sun_path, api_clisock, sizeof (cli_addr.sun_path));
      cli_len = strlen (cli_addr.sun_path) + sizeof (cli_addr.sun_family);
  
      errno = 0;
      if (bind (sockfd, (struct sockaddr *) & cli_addr, cli_len) < 0) {
        loge ("gen_client_cmd: bind errno: %d (%s)", errno, strerror (errno));
        close (sockfd);
        sockfd = -1;
        return (0);                                                     // OK to continue w/ Internet Stream but since this is Unix Datagram and we ran unlink (), let's fail
      }
    #endif
    }
  //!! Can move inside above
  // Setup server address
    memset ((char *) & srv_addr, sizeof (srv_addr), 0);
  #ifdef  CS_AF_UNIX
    strlcpy (api_srvsock, DEF_API_SRVSOCK, sizeof (api_srvsock));
    char itoa_ret [MAX_ITOA_SIZE] = {0};
    strlcat (api_srvsock, itoa (net_port, itoa_ret, 10), sizeof (api_srvsock));
    srv_addr.sun_family = AF_UNIX;
    strlcpy (srv_addr.sun_path, api_srvsock, sizeof (srv_addr.sun_path));
    srv_len = strlen (srv_addr.sun_path) + sizeof (srv_addr.sun_family);
  #else
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
    //errno = 0;
    //hp = gethostbyname ("localhost");
    //if (hp == 0) {
    //  loge ("gen_client_cmd: Error gethostbyname  errno: %d (%s)", errno, strerror (errno));
    //  return (0);
    //}
    //bcopy ((char *) hp->h_addr, (char *) & srv_addr.sin_addr, hp->h_length);
    srv_addr.sin_port = htons (net_port);
    srv_len = sizeof (struct sockaddr_in);
  #endif
  
  
  // Send cmd_buf and get res_buf
  #ifdef CS_DGRAM
    errno = 0;
    written = sendto (sockfd, cmd_buf, cmd_len, 0, (const struct sockaddr *) & srv_addr, srv_len);
    if (written != cmd_len) {                                           // Dgram buffers should not be segmented
      loge ("gen_client_cmd: sendto errno: %d (%s)", errno, strerror (errno));
    #ifdef  CS_DGRAM_UNIX
      unlink (api_clisock);
    #endif
      close (sockfd);
      sockfd = -1;
      return (0);
    }
  
    sock_tmo_set (sockfd, rx_tmo);

    res_len = -1;
    ctr = 0;
    while (res_len < 0 && ctr < 2) {
      errno = 0;
      res_len = recvfrom (sockfd, res_buf, res_max, 0, (struct sockaddr *) & srv_addr, & srv_len);
      ctr ++;
      if (res_len < 0 && ctr < 2) {
        if (errno == EAGAIN)
          logw ("gen_client_cmd: recvfrom errno: %d (%s)", errno, strerror (errno));           // Occasionally get EAGAIN here
        else
          loge ("gen_client_cmd: recvfrom errno: %d (%s)", errno, strerror (errno));
      }
    }
    if (res_len <= 0) {
      loge ("gen_client_cmd: recvfrom errno: %d (%s)", errno, strerror (errno));
    #ifdef  CS_DGRAM_UNIX
      unlink (api_clisock);
    #endif
      close (sockfd);
      sockfd = -1;
      return (0);
    }
    #ifndef CS_AF_UNIX
  // !!   ?? Don't need this ?? If srv_addr still set from sendto, should restrict recvfrom to localhost anyway ?
    if ( srv_addr.sin_addr.s_addr != htonl (INADDR_LOOPBACK) ) {
      loge ("gen_client_cmd: Unexpected suspicious packet from host");// %s", inet_ntop(srv_addr.sin_addr.s_addr)); //inet_ntoa(srv_addr.sin_addr.s_addr));
    }
    #endif
  #else
    errno = 0;
    if (connect (sockfd, (struct sockaddr *) & srv_addr, srv_len) < 0) {
      loge ("gen_client_cmd: connect errno: %d (%s)", errno, strerror (errno));
      close (sockfd);
      sockfd = -1;
      return (0);
    }
    errno = 0;
    written = write (sockfd, cmd_buf, cmd_len);                           // Write the command packet
    if (written != cmd_len) {                                             // Small buffers under 256 bytes should not be segmented ?
      loge ("gen_client_cmd: write errno: %d (%s)", errno, strerror (errno));
      close (sockfd);
      sockfd = -1;
      return (0);
    }
  
    sock_tmo_set (sockfd, rx_tmo);
  
    errno = 0;
    res_len = read (sockfd, res_buf, res_max)); // Read response
    if (res_len <= 0) {
      loge ("gen_client_cmd: read errno: %d (%s)", errno, strerror (errno));
      close (sockfd);
      sockfd = -1;
      return (0);
    }
  #endif
    //hex_dump ("", 32, res_buf, n);
  #ifdef  CS_DGRAM_UNIX
      unlink (api_clisock);
  #endif
    //close (sockfd);
    return (res_len);
  }

#endif      //#ifndef GENERIC_CLIENT_INCLUDED
#endif      //#ifdef  GENERIC_CLIENT


#ifdef  GENERIC_SERVER
#ifndef  GENERIC_SERVER_INCLUDED
#define  GENERIC_SERVER_INCLUDED

  int gen_server_exiting = 0;

  int gen_server_loop (int net_port, int poll_ms) {                     // Run until gen_server_exiting != 0, passing incoming commands to gen_server_loop_func() and responding with the results
    int sockfd = -1, newsockfd = -1, cmd_len = 0, ctr = 0;
    socklen_t cli_len = 0, srv_len = 0;
  #ifdef  CS_AF_UNIX
    struct sockaddr_un  cli_addr = {0}, srv_addr = {0};
    srv_len = strlen (srv_addr.sun_path) + sizeof (srv_addr.sun_family);
  #else
    struct sockaddr_in  cli_addr = {0}, srv_addr = {0};
    //struct hostent *hp;
  #endif
    unsigned char cmd_buf [DEF_BUF] ={0};
  
  #ifdef  CS_AF_UNIX
    strlcpy (api_srvsock, DEF_API_SRVSOCK, sizeof (api_srvsock));
    char itoa_ret [MAX_ITOA_SIZE] = {0};
    strlcat (api_srvsock, itoa (net_port, itoa_ret, 10), sizeof (api_srvsock));
    unlink (api_srvsock);
  #endif
    errno = 0;
    if ((sockfd = socket (CS_FAM, CS_SOCK_TYPE, 0)) < 0) {              // Create socket
      loge ("gen_server_loop socket  errno: %d (%s)", errno, strerror (errno));
      return (-1);
    }

    sock_reuse_set (sockfd);

    if (poll_ms != 0)
      sock_tmo_set (sockfd, poll_ms);                                   // If polling mode, set socket timeout for polling every poll_ms milliseconds

    memset ((char *) & srv_addr, sizeof (srv_addr), 0);
  #ifdef  CS_AF_UNIX
    srv_addr.sun_family = AF_UNIX;
    strlcpy (srv_addr.sun_path, api_srvsock, sizeof (srv_addr.sun_path));
    srv_len = strlen (srv_addr.sun_path) + sizeof (srv_addr.sun_family);
  #else
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);                 // Will bind to loopback instead of common INADDR_ANY. Packets should only be received by loopback and never Internet.
                                                                        // For 2nd line of defence see: loge ("Unexpected suspicious packet from host");
    //errno = 0;
    //hp = gethostbyname ("localhost");
    //if (hp == 0) {
    //  loge ("Error gethostbyname  errno: %d (%s)", errno, strerror (errno));
    //  return (-2);
    //}
    //bcopy ((char *) hp->h_addr, (char *) & srv_addr.sin_addr, hp->h_length);
    srv_addr.sin_port = htons (net_port);
    srv_len = sizeof (struct sockaddr_in);
  #endif
  
  #ifdef  CS_AF_UNIX
  logd ("srv_len: %d  fam: %d  path: %s", srv_len, srv_addr.sun_family, srv_addr.sun_path);
  #else
  logd ("srv_len: %d  fam: %d  addr: 0x%x  port: %d", srv_len, srv_addr.sin_family, ntohl (srv_addr.sin_addr.s_addr), ntohs (srv_addr.sin_port));
  #endif
    errno = 0;
    if (bind (sockfd, (struct sockaddr *) & srv_addr, srv_len) < 0) {   // Bind socket to server address
      loge ("Error bind  errno: %d (%s)", errno, strerror (errno));
  #ifdef  CS_AF_UNIX
      return (-3);
  #endif
  #ifdef CS_DGRAM
      return (-3);
  #endif
      loge ("Inet stream continuing despite bind error");               // OK to continue w/ Internet Stream
    }

    // Done after socket() and before bind() so don't repeat it here ?
    //if (poll_ms != 0)
    //  sock_tmo_set (sockfd, poll_ms);                                   // If polling mode, set socket timeout for polling every poll_ms milliseconds

  // Get command from client
  #ifndef CS_DGRAM
    errno = 0;
    if (listen (sockfd, 5)) {                                           // Backlog= 5; likely don't need this
      loge ("Error listen  errno: %d (%s)", errno, strerror (errno));
      return (-4);
    }
  #endif
  
    logd ("gen_server_loop Ready");
  
    while (! gen_server_exiting) {
      memset ((char *) & cli_addr, sizeof (cli_addr), 0);               // ?? Don't need this ?
      //cli_addr.sun_family = CS_FAM;                                   // ""
      cli_len = sizeof (cli_addr);
  
      //logd ("ms_get: %d",ms_get ());
  #ifdef  CS_DGRAM
      errno = 0;
      cmd_len = recvfrom (sockfd, cmd_buf, sizeof (cmd_buf), 0, (struct sockaddr *) & cli_addr, & cli_len);
      if (cmd_len <= 0) {
        if (errno == EAGAIN) {
          if (poll_ms != 0)                                             // If timeout polling is enabled...
            gen_server_poll_func (poll_ms);                             // Do the polling work
          else
            loge ("gen_server_loop EAGAIN !!!");                        // Else EGAIN is an unexpected error for blocking mode
        }
        else {                                                          // Else if some other error, sleep it off for 100 ms
          if (errno == EINTR)
            logw ("Error recvfrom errno: %d (%s)", errno, strerror (errno));
          else
            loge ("Error recvfrom errno: %d (%s)", errno, strerror (errno));
          quiet_ms_sleep (101);
        }
        continue;
      }
    #ifndef CS_AF_UNIX
  // !! 
      if ( cli_addr.sin_addr.s_addr != htonl (INADDR_LOOPBACK) ) {
        //loge ("Unexpected suspicious packet from host %s", inet_ntop (cli_addr.sin_addr.s_addr));
        loge ("Unexpected suspicious packet from host");// %s", inet_ntoa (cli_addr.sin_addr.s_addr));
      }
    #endif
  #else
      errno = 0;
      newsockfd = accept (sockfd, (struct sockaddr *) & cli_addr, & cli_len);
      if (newsockfd < 0) {
        loge ("Error accept  errno: %d (%s)", errno, strerror (errno));
        ms_sleep (101);                                                 // Sleep 0.1 second to try to clear errors
        continue;
      }
    #ifndef  CS_AF_UNIX
  // !! 
      if ( cli_addr.sin_addr.s_addr != htonl (INADDR_LOOPBACK) ) {
        //loge ("Unexpected suspicious packet from host %s", inet_ntop (cli_addr.sin_addr.s_addr));
        loge ("Unexpected suspicious packet from host");// %s", inet_ntoa (cli_addr.sin_addr.s_addr));
      }
    #endif
      errno = 0;
      cmd_len = read (newsockfd, cmd_buf, sizeof (cmd_buf));
      if (cmd_len <= 0) {
        loge ("Error read  errno: %d (%s)", errno, strerror (errno));
        ms_sleep (101);                                                 // Sleep 0.1 second to try to clear errors
        close (newsockfd);
        ms_sleep (101);                                                 // Sleep 0.1 second to try to clear errors
        continue;
      }
  #endif
  
  #ifdef  CS_AF_UNIX
      //logd ("cli_len: %d  fam: %d  path: %s",cli_len,cli_addr.sun_family,cli_addr.sun_path);
  #else
      //logd ("cli_len: %d  fam: %d  addr: 0x%x  port: %d",cli_len,cli_addr.sin_family, ntohl (cli_addr.sin_addr.s_addr), ntohs (cli_addr.sin_port));
  #endif
      //hex_dump ("", 32, cmd_buf, n);
  
      unsigned char res_buf [RES_DATA_MAX] = {0};
      int res_len = 0;

      cmd_buf [cmd_len] = 0;                                            // Null terminate for string usage
                                                                        // Do server command function and provide response
      res_len = gen_server_loop_func ( cmd_buf, cmd_len, res_buf, sizeof (res_buf));

      if (ena_log_verb)
        logd ("gen_server_loop gen_server_loop_func res_len: %d", res_len);

      if (res_len < 0) {                                                // If error
        res_len = 2;
        res_buf [0] = 0xff;                                             // '?';   ?? 0xff for HCI ?
        res_buf [1] = 0xff;                                             // '\n';
        res_buf [2] = 0;
      }
      //hex_dump ("", 32, res_buf, res_len);
  
  
  // Send response
  #ifdef  CS_DGRAM
      errno = 0;
      if (sendto (sockfd, res_buf, res_len, 0, (struct sockaddr *) & cli_addr, cli_len) != res_len) {
        loge ("Error sendto  errno: %d (%s)  res_len: %d", errno, strerror (errno), res_len);
        ms_sleep (101);                                                 // Sleep 0.1 second to try to clear errors
      }
  #else
      errno = 0;
      if (write (newsockfd, res_buf, res_len) != res_len) {             // Write, if can't write full buffer...
        loge ("Error write  errno: %d (%s)", errno, strerror (errno));
        ms_sleep (101);                                                 // Sleep 0.1 second to try to clear errors
      }
      close (newsockfd);
  #endif
    }
    close (sockfd);
  #ifdef  CS_AF_UNIX
    unlink (api_srvsock);
  #endif
  
    return (0);
  }

#endif      //#ifndef GENERIC_SERVER_INCLUDED
#endif      //#ifdef  GENERIC_SERVER

#ifndef NDEBUG
  char * usb_vid_get (int vid) {
    switch (vid) {
      case USB_VID_GOO:   return ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Google");
      case USB_VID_HTC:   return ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! HTC");
      case USB_VID_MOT:   return ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Motorola");
      case USB_VID_SAM:   return ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Samsung");
      case USB_VID_O1A:   return ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Other/Samsung?");    // 0xfff6 / Samsung ?
      case USB_VID_SON:   return ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! Sony");
      case USB_VID_LGE:   return ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! LGE");
      case USB_VID_LIN:   return ("Linux");
      case USB_VID_QUA:   return ("Qualcomm");
      case USB_VID_COM:   return ("Comneon");
      case USB_VID_ASE:   return ("Action Star");
    }
    return ("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  Unknown VID");//: %d", vid);
  }
#endif

