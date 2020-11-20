// Utilities: Used by many

//#ifndef UTILS_INCLUDED

//#define UTILS_INCLUDED

//#define  GENERIC_CLIENT

#define LOGTAG "hu_uti"
#include "hu_uti.h"
#include "hu.pb.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <ctype.h>
#include <openssl/ssl.h>


#include <string.h>
#include <signal.h>

#include <pthread.h>

#include <errno.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <dirent.h>                                                   // For opendir (), readdir (), closedir (), DIR, struct dirent.
#include <sys/utsname.h>
#include <libusb.h>
#include <execinfo.h>
#include <dlfcn.h>    // for dladdr
#include <cxxabi.h>   // for __cxa_demangle
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <ucontext.h>

int gen_server_loop_func (unsigned char * cmd_buf, int cmd_len, unsigned char * res_buf, int res_max);
int gen_server_poll_func (int poll_ms);

  // Log stuff:

int ena_log_extra   = 0;
int ena_log_verbo   = 0;
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


const  char * prio_get (int prio) {
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


  va_list ap;
  va_start (ap, fmt);
#ifdef __ANDROID_API__
  char tag_str [512] = {0};
  snprintf (tag_str, sizeof (tag_str), "%32.32s", func);
  __android_log_vprint (prio, tag_str, fmt, ap);
#else
  va_list aq;
  va_start (aq, fmt);

  char log_line [4096] = {0};
  int len = vsnprintf (log_line, sizeof (log_line), fmt, aq);

  //Time doesn't work on CMU anyway, always says 1970
  time_t timestamp;
  time(&timestamp);
  printf ("%d %s: %s: %s : %s\n", timestamp, prio_get (prio), tag, func, log_line);

  va_end(aq);
#endif
  va_end(ap);

  // if (prio == hu_LOG_ERR)
  // {
  //   raise(SIGTRAP);
  // }

  return (0);
}

unsigned long ms_sleep (unsigned long ms) {
  usleep (ms * 1000L);
  return (ms);
}


#define HD_MW   256
void hex_dump (const char * prefix, int width, unsigned char * buf, int len) {
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
    strlcat (line, prefix, sizeof(line));

  snprintf (tmp, sizeof (tmp), " %8.8x ", 0);
  strlcat (line, tmp, sizeof(line) - strlen(line));

  for (i = 0, n = 1; i < len; i ++, n ++) {                           // i keeps incrementing, n gets reset to 0 each line

    snprintf (tmp, sizeof (tmp), "%2.2x ", buf [i]);
    strlcat (line, tmp, sizeof(line) - strlen(line));                 // Append 2 bytes hex and space to line

    if (n == width) {                                                 // If at specified line width
      n = 0;                                                          // Reset position in line counter
      logd (line);                                                    // Log line

      line [0] = 0;
      if (prefix)
        //strlcpy (line, prefix, sizeof (line));
        strlcat (line, prefix, sizeof(line) - strlen(line));

      //snprintf (tmp, sizeof (tmp), " %8.8x ", i + 1);
      snprintf (tmp, sizeof (tmp), "     %4.4x ", i + 1);
      strlcat (line, tmp, sizeof(line) - strlen(line));
    }
    else if (i == len - 1)                                            // Else if at last byte
      logd (line);                                                    // Log line
  }
}

void hu_log_library_versions()
{
  utsname sysinfo;
  if (uname(&sysinfo) < 0)
  {
    printf("uname failed\n");
  }
  else
  {
    printf("uname:\n sysname: %s\n release: %s\n version: %s\n machine: %s\n", sysinfo.sysname, sysinfo.release, sysinfo.version, sysinfo.machine);
  }
  printf("libprotoversion: %s\n",google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION).c_str());

  const libusb_version* usbversion = libusb_get_version();
  printf("libusb_get_version:\n");
  printf(" version: %u.%u.%u.%u\n", (unsigned int)usbversion->major, (unsigned int)usbversion->minor, (unsigned int)usbversion->micro, (unsigned int)usbversion->nano);
  printf(" rc: %s\n describe: %s\n", usbversion->rc, usbversion->describe);

  SSL_library_init();
  printf("openssl version: %s (%#010lx)\n", SSLeay_version(SSLEAY_VERSION), SSLeay());
}

static void print_backtrace(ucontext_t* ucontext)
{
  unw_cursor_t cursor;
  unw_context_t context;

  if (ucontext)
  {
    #ifdef __arm__
    //convert ucontext_t
    context.regs[UNW_ARM_R0] = ucontext->uc_mcontext.arm_r0;
    context.regs[UNW_ARM_R1] = ucontext->uc_mcontext.arm_r1;
    context.regs[UNW_ARM_R2] = ucontext->uc_mcontext.arm_r2;
    context.regs[UNW_ARM_R3] = ucontext->uc_mcontext.arm_r3;
    context.regs[UNW_ARM_R4] = ucontext->uc_mcontext.arm_r4;
    context.regs[UNW_ARM_R5] = ucontext->uc_mcontext.arm_r5;
    context.regs[UNW_ARM_R6] = ucontext->uc_mcontext.arm_r6;
    context.regs[UNW_ARM_R7] = ucontext->uc_mcontext.arm_r7;
    context.regs[UNW_ARM_R8] = ucontext->uc_mcontext.arm_r8;
    context.regs[UNW_ARM_R9] = ucontext->uc_mcontext.arm_r9;
    context.regs[UNW_ARM_R10] = ucontext->uc_mcontext.arm_r10;
    context.regs[UNW_ARM_R11] = ucontext->uc_mcontext.arm_fp;
    context.regs[UNW_ARM_R12] = ucontext->uc_mcontext.arm_ip;
    context.regs[UNW_ARM_R13] = ucontext->uc_mcontext.arm_sp;
    context.regs[UNW_ARM_R14] = ucontext->uc_mcontext.arm_lr;
    context.regs[UNW_ARM_R15] = ucontext->uc_mcontext.arm_pc;
    #else
    //they are the same
    context = *ucontext;
    #endif
  }
  else
  {
      // Initialize cursor to current frame for local unwinding.
      unw_getcontext(&context);
  }
  unw_init_local(&cursor, &context);

  int skip_count = 0; //show everything for now
  int cur_level = 0;

  // Unwind frames one by one, going up the frame stack.
  while (unw_step(&cursor) > 0)
  {
    unw_word_t offset, pc;
    unw_get_reg(&cursor, UNW_REG_IP, &pc);
    if (pc == 0)
    {
      break;
    }
    cur_level++;
    if (cur_level <= skip_count)
    {
      continue;
    }

    printf("(%i) 0x%lx:", cur_level - skip_count - 1, pc);


    char sym[4096];
    if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0)
    {
      const char* sym_name = sym;
      char* demanged_name = nullptr;
      if (sym[0] == '_')
      {
        int status = 1;
        demanged_name = abi::__cxa_demangle(sym, NULL, 0, &status);
        if (status == 0)
        {
          sym_name = demanged_name;
        }
      }

      printf(" (%s+0x%lx)\n", sym_name, offset);
      free(demanged_name);
    }
    else
    {
      printf(" ???\n");
    }
  }
}

static void crash_handler(int sig, siginfo_t * info, void * ucontext)
{
  ucontext_t* context = reinterpret_cast<ucontext_t*>(ucontext);
  // print out all the frames to stderr
  printf("Error: signal %s context %p :\n", strsignal(sig), context);

  print_backtrace(context);
#if CMU
  //JS code looks for this to flush the log
  printf("END \n");
#endif
  abort();
}

static void crash_handler_terminate()
{
    // print out all the frames to stderr
  printf("Error: c++ exception\n");

  print_backtrace(nullptr);
#if CMU
  //JS code looks for this to flush the log
  printf("END \n");
#endif
  abort();
}

void hu_install_crash_handler()
{
  struct sigaction sigact;
  memset(&sigact, 0, sizeof(sigact));

  sigact.sa_sigaction = &crash_handler;
  sigact.sa_flags = SA_SIGINFO;

  sigaction(SIGSEGV, &sigact, nullptr);
  sigaction(SIGILL, &sigact, nullptr);
  sigaction(SIGFPE, &sigact, nullptr);
  sigaction(SIGBUS, &sigact, nullptr);
  sigaction(SIGSYS, &sigact, nullptr);
  sigaction(SIGXCPU, &sigact, nullptr);
  sigaction(SIGXFSZ, &sigact, nullptr);
  std::set_terminate (crash_handler_terminate);
}
int wait_for_device_connection(){
        int ret;

        struct udev *udev = udev_new();
        struct udev_device *dev;
        struct udev_monitor *mon;
        int fd;
	/* Set up a monitor to monitor USB devices */
	mon = udev_monitor_new_from_netlink(udev, "udev");
        if(mon == NULL) {
                loge("udev_monitor_new_from_netlink returned NULL\n");
                return -2;
        }

        ret = udev_monitor_filter_add_match_subsystem_devtype(mon, "usb", "usb_device");
	if(ret != 0) {
                loge("udev_monitor_filter_add_match_subsystem_devtype error : %d \n",ret);
                return -2;
        }

	ret = udev_monitor_enable_receiving(mon);
        if(ret != 0){
                loge("udev_monitor_enable_receiving error : %d \n",ret);
                return -2;
        }

	fd = udev_monitor_get_fd(mon);

	while (1) {
		fd_set fds;
		struct timeval tv;
		int ret;

		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		ret = select(fd+1, &fds, NULL, NULL, &tv);

		/* Check if our file descriptor has received data. */
		if (ret > 0 && FD_ISSET(fd, &fds)) {
			logv("\nselect() says there should be data\n");

			/* Make the call to receive the device.
			   select() ensured that this will not block. */
			dev = udev_monitor_receive_device(mon);
			if (dev) {
				logw("udev device %sed | node:%s, subsystem:%s, devtype:%s\n",
                                        udev_device_get_action(dev),
                                        udev_device_get_devnode(dev),
                                        udev_device_get_subsystem(dev),
                                        udev_device_get_devtype(dev));

                                if(strcmp(udev_device_get_action(dev),"add") == 0){
                                        udev_device_unref(dev);
                                        return 0;
                                }
				udev_device_unref(dev);
			}
			else {
				loge("udev_monitor_receive_device error: no new device\n");
			}
		}
		usleep(25*1000);
	}
}
