#if defined(__unix__)
# include <sys/param.h>
#endif

#if !defined(NDEBUG) && defined(SP_USE_VALGRIND)
# define SP_VALGRIND 1
# include <valgrind/valgrind.h>
#endif

#if defined(BSD)
# define SP_STAT_CLOCK 1
# define SP_POSIX_CLOCK 1
# ifdef CLOCK_REALTIME_FAST
#  define SP_CLOCK_REALIME CLOCK_REALTIME_FAST
# else
#  define SP_CLOCK_REALIME CLOCK_REALTIME
# endif
# ifdef CLOCK_MONOTONIC_PRECISE
#  define SP_CLOCK_MONOTONIC CLOCK_MONOTONIC_PRECISE
# else
#  define SP_CLOCK_MONOTONIC CLOCK_MONOTONIC
# endif

#elif defined(__linux__)
# define SP_POSIX_CLOCK 1
# ifdef CLOCK_REALTIME_COARSE
#  define SP_CLOCK_REALIME CLOCK_REALTIME_COARSE
# else
#  define SP_CLOCK_REALIME CLOCK_REALTIME
# endif
# ifdef CLOCK_MONOTONIC_RAW
#  define SP_CLOCK_MONOTONIC CLOCK_MONOTONIC_RAW
# else
#  define SP_CLOCK_MONOTONIC CLOCK_MONOTONIC
# endif

#elif defined(__APPLE__)
# define SP_STAT_CLOCK 1
# define SP_MACH_CLOCK 1
# include <mach/clock.h>
# include <mach/mach_host.h>
# include <mach/mach_port.h>
# include <mach/mach_time.h>

#else
# error unsupported platform
#endif

