import ctypes
import mmap
import os
import signal
import subprocess
import threading
import time
import errno

_libc = ctypes.CDLL(None, use_errno=True)

TOTAL_TAKEOFFS = 20
STRIPS = 5
SHM_NAME = b"/air_control_memory"   # must be bytes for libc calls
SHM_INTS = 3

# size for 3 ints
SHM_LENGTH = ctypes.sizeof(ctypes.c_int) * SHM_INTS

# Global variables and locks
planes = 0          # planes waiting
takeoffs = 0        # local block counter (shared across threads per spec)
total_takeoffs = 0  # total takeoffs performed

# Locks
runway1_lock = threading.Lock()
runway2_lock = threading.Lock()
state_lock = threading.Lock()

# We'll keep references to shared memory objects returned by create_shared_memory
_shm_fd = None
_shm_mmap = None
_shm_arr = None  # ctypes array view of shared memory


# ----- libc prototypes -----
_libc.shm_open.restype = ctypes.c_int
_libc.shm_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]

_libc.ftruncate.restype = ctypes.c_int
_libc.ftruncate.argtypes = [ctypes.c_int, ctypes.c_size_t]

_libc.shm_unlink.restype = ctypes.c_int
_libc.shm_unlink.argtypes = [ctypes.c_char_p]


def create_shared_memory():
    """
    Create shared memory and return (fd, mmap_obj, ctypes_array_of_ints)
    """
    global _shm_fd, _shm_mmap, _shm_arr

    # Temporarily loosen umask so created shm has correct permissions
    old_umask = os.umask(0)
    try:
        # O_CREAT | O_RDWR
        flags = os.O_CREAT | os.O_RDWR
        fd = _libc.shm_open(SHM_NAME, flags, 0o666)
        if fd < 0:
            err = ctypes.get_errno()
            raise OSError(err, "shm_open failed")
    finally:
        os.umask(old_umask)

    # resize
    if _libc.ftruncate(fd, SHM_LENGTH) != 0:
        err = ctypes.get_errno()
        os.close(fd)
        raise OSError(err, "ftruncate failed")

    # mmap the region
    # Python's mmap requires a file descriptor and length
    mm = mmap.mmap(fd, SHM_LENGTH, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ | mmap.PROT_WRITE)

    # Create a ctypes array view pointing to the mmap buffer
    # from_buffer requires a writable buffer (mmap is writable)
    IntArray = ctypes.c_int * SHM_INTS
    arr = IntArray.from_buffer(mm)

    # Save globals
    _shm_fd = fd
    _shm_mmap = mm
    _shm_arr = arr

    return fd, mm, arr


def HandleUSR2(signum, frame):
    """Handle SIGUSR2: arrival of 5 new planes."""
    global planes
    # increment planes by 5 (signal handler runs in main thread)
    # Use a simple lockless increment — not ideal but acceptable for this assignment pattern
    # We still use state_lock to avoid races with threads updating planes concurrently
    try:
        state_lock.acquire()
        planes += 5
    finally:
        state_lock.release()


def TakeOffFunction(agent_id: int):
    """Thread worker that authorizes takeoffs."""
    global planes, takeoffs, total_takeoffs

    while True:
        # Fast exit if we've already reached the total
        with state_lock:
            if total_takeoffs >= TOTAL_TAKEOFFS:
                break

        # Try to acquire any runway (non-blocking). If none available, sleep shortly and retry.
        runway_acquired = None
        # Try runway1
        if runway1_lock.acquire(blocking=False):
            runway_acquired = runway1_lock
        else:
            # Try runway2
            if runway2_lock.acquire(blocking=False):
                runway_acquired = runway2_lock

        if runway_acquired is None:
            # no runway available; wait a tiny bit and retry
            time.sleep(0.001)
            continue

        # We have exclusive access to one runway now.
        took_off = False
        try:
            # Enter state critical section to check planes and update counters
            with state_lock:
                if total_takeoffs >= TOTAL_TAKEOFFS:
                    # done
                    break

                if planes > 0:
                    # permit takeoff
                    planes -= 1
                    takeoffs += 1
                    total_takeoffs += 1
                    took_off = True

                    # If we've completed a block of 5 takeoffs, signal the radio and reset
                    if takeoffs >= 5:
                        # send SIGUSR1 to radio process, radio pid is stored at shm index 1
                        try:
                            radio_pid = int(_shm_arr[1])
                            if radio_pid > 0:
                                os.kill(radio_pid, signal.SIGUSR1)
                        except Exception:
                            # ignore signalling errors (radio might not be up yet)
                            pass
                        takeoffs = 0

            if took_off:
                # simulate actual takeoff time
                time.sleep(1)
            else:
                # No planes available right now — release runway and try again
                # small sleep to avoid tight loop
                time.sleep(0.001)
        finally:
            # release the runway lock we acquired
            try:
                runway_acquired.release()
            except RuntimeError:
                # if release fails, ignore
                pass

    # If this thread is exiting because total_takeoffs reached target,
    # send SIGTERM to the radio (only one thread needs to do it — but safe to call multiple times)
    try:
        radio_pid = int(_shm_arr[1])
        if radio_pid > 0:
            os.kill(radio_pid, signal.SIGTERM)
    except Exception:
        pass


def launch_radio():
    """
    Launch the radio executable and return the Popen process handle.
    We unblock SIGUSR2 in the child so it can receive that signal.
    """
    def _unblock_sigusr2():
        signal.pthread_sigmask(signal.SIG_UNBLOCK, {signal.SIGUSR2})

    # radio expects the shared memory name as an argument
    proc = subprocess.Popen(["./radio", SHM_NAME.decode("utf-8")], preexec_fn=_unblock_sigusr2)
    return proc


def main():
    global _shm_fd, _shm_mmap, _shm_arr, planes, takeoffs, total_takeoffs

    # Register SIGUSR2 handler
    signal.signal(signal.SIGUSR2, HandleUSR2)

    # Create shared memory and write our PID into position 0
    fd, mm, arr = create_shared_memory()
    # ensure memory initially zeroed
    for i in range(SHM_INTS):
        arr[i] = 0

    arr[0] = os.getpid()

    # Launch radio and store its PID in shared memory (index 1)
    radio_proc = launch_radio()
    # Wait briefly for child to start and have a PID
    time.sleep(0.05)
    arr[1] = radio_proc.pid

    # Create & start controller threads
    threads = []
    for i in range(STRIPS):
        t = threading.Thread(target=TakeOffFunction, args=(i,))
        t.start()
        threads.append(t)

    # Wait for threads to finish
    for t in threads:
        t.join()

    # Once threads done, cleanup: close mmap and unlink shared memory
    try:
        _shm_mmap.close()
    except Exception:
        pass

    try:
        os.close(_shm_fd)
    except Exception:
        pass

    # unlink shared memory so tests can verify it's gone
    _libc.shm_unlink(SHM_NAME)

    # Ensure radio process has exited (it will be signalled by threads)
    try:
        radio_proc.wait(timeout=1)
    except Exception:
        try:
            radio_proc.terminate()
        except Exception:
            pass


if __name__ == "__main__":
    main()
