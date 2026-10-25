/** Native-harness fixtures for parked roots, interpreter time slicing, and worker failure. */
public class ThreadGcChecks {
    public static int parked;
    public static int completed;
    public static int failures;
    public static int cpuDone;
    public static int survived;
    private static final Object throwingLock = new Object();

    private static synchronized void parkedOne() { parked++; }
    private static synchronized void finished(boolean valid) {
        if (!valid) failures++;
        completed++;
    }
    private static synchronized void failed() { failures++; }
    private static synchronized void cpuFinished(int checksum) {
        if (checksum != 25493856) failures++;
        cpuDone++;
    }

    /** The live Thread objects and worker payloads are retained only by VM contexts. */
    public static int setup() {
        parked = 0;
        completed = 0;
        failures = 0;
        for (int i = 0; i < 3; i++) {
            final int tag = i;
            new Thread(new Runnable() {
                public void run() {
                    try {
                        byte[] payload = new byte[65536];
                        payload[0] = (byte) (17 + tag);
                        payload[payload.length - 1] = (byte) (83 + tag);
                        Object lock = new Object();
                        Object identity = lock;
                        synchronized (lock) {
                            parkedOne();
                            boolean interrupted = false;
                            try { lock.wait(60000); }
                            catch (InterruptedException expected) { interrupted = true; }
                            finished(interrupted
                                    && payload.length == 65536
                                    && payload[0] == (byte) (17 + tag)
                                    && payload[payload.length - 1] == (byte) (83 + tag)
                                    && lock == identity
                                    && Thread.holdsLock(lock)
                                    && !Thread.currentThread().isInterrupted());
                        }
                    } catch (Throwable problem) { failed(); }
                }
            }).start();
        }
        return 3;
    }

    /** Neither worker explicitly yields: scheduler instruction cuts must retain frames. */
    public static int setupCpu() {
        cpuDone = 0;
        for (int worker = 0; worker < 2; worker++) {
            new Thread(new Runnable() {
                public void run() {
                    int checksum = 0;
                    for (int i = 0; i < 200000; i++) checksum += i & 255;
                    cpuFinished(checksum);
                }
            }).start();
        }
        return 2;
    }

    /** The uncaught worker exception must unwind its lock without killing its peer. */
    public static int setupThrowing() {
        survived = 0;
        new Thread(new Runnable() {
            public void run() {
                synchronized (throwingLock) {
                    throw new RuntimeException("expected uncaught worker exception");
                }
            }
        }).start();
        new Thread(new Runnable() {
            public void run() {
                synchronized (throwingLock) { survived = 1; }
            }
        }).start();
        return 2;
    }
}
