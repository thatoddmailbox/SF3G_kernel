/* Automatically created during backport process */
#if 0//ndef CPTCFG_BACKPORT_BUILD_WANT_DEV_COREDUMP
#include_next <linux/devcoredump.h>
#else
#undef dev_coredumpv
#define dev_coredumpv LINUX_BACKPORT(dev_coredumpv)
#undef dev_coredumpm
#define dev_coredumpm LINUX_BACKPORT(dev_coredumpm)
#include <linux/backport-devcoredump.h>
#endif /* CPTCFG_BACKPORT_BUILD_WANT_DEV_COREDUMP */