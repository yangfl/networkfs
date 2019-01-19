#ifndef SYNCHRONIZED_H
#define SYNCHRONIZED_H

#include <pthread.h>

#include "with.h"


#define synchronized(type, lock, action) with( \
  , while (pthread_ ## type ## _ ## action(lock));, 1, pthread_ ## type ## _unlock(lock))
#define wait_synchronized(type, lock, action) \
  for (int pthread_errno;;) if ((pthread_errno = pthread_ ## type ## _try ## action(lock)) == 0) do_after(pthread_ ## type ## _unlock(lock); break)
#define try_synchronized(type, lock, action) \
  let (int pthread_errno) if ((pthread_errno = pthread_ ## type ## _try ## action(lock)) == 0) do_after(pthread_ ## type ## _unlock(lock))
#define until_synchronized(type, lock, action) \
  while (pthread_ ## type ## _try ## action(lock)); do_after(pthread_ ## type ## _unlock(lock))
#define __generate_ensure_synchronized(type, lock, action, counter) \
  for (int pthread_errno;;) if (1) goto CONCAT(__ensure_synchronized_trylock_, counter); \
  else do_after(if (pthread_errno == 0) pthread_ ## type ## _unlock(lock); break) while (1) \
    if (1) { \
      if (pthread_errno == 0) goto CONCAT(__ensure_synchronized_lock_, counter); \
      CONCAT(__ensure_synchronized_trylock_, counter): \
      pthread_errno = pthread_ ## type ## _try ## action(lock); \
      goto CONCAT(__ensure_synchronized_ensure_, counter); \
    } else CONCAT(__ensure_synchronized_ensure_, counter): \
      if (0) CONCAT(__ensure_synchronized_lock_, counter): break_after
#define ensure_synchronized(type, lock, action) __generate_ensure_synchronized(type, lock, action, __COUNTER__)


#endif /* SYNCHRONIZED_H */
