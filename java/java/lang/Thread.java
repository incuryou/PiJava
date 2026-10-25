package java.lang;

public class Thread implements Runnable {
    public static final int MIN_PRIORITY = 1;
    public static final int NORM_PRIORITY = 5;
    public static final int MAX_PRIORITY = 10;

    // Runtime state: 0 = not started, 1 = alive, 2 = terminated.
    private int vmState;
    private boolean vmInterrupted;
    // Retained as a hint; the cooperative scheduler uses round robin.
    private int vmPriority = NORM_PRIORITY;
    private Runnable target;
    private String name;
    private static int nextThreadNumber;

    public Thread() { this((Runnable) null); }
    public Thread(Runnable target) {
        this(target, "Thread-" + nextThreadNumber++);
    }
    public Thread(String name) { this(null, name); }
    public Thread(Runnable target, String name) {
        if (name == null) throw new NullPointerException();
        this.target = target;
        this.name = name;
    }

    public void run() { if (target != null) target.run(); }
    public native void start();
    public static native void sleep(long millis) throws InterruptedException;

    public static void sleep(long millis, int nanos) throws InterruptedException {
        sleep(roundedMillis(millis, nanos));
    }

    public static native Thread currentThread();
    public final native boolean isAlive();
    public native void interrupt();
    public static native boolean interrupted();
    public native boolean isInterrupted();
    public final native void join(long millis) throws InterruptedException;

    public final void join() throws InterruptedException { join(0); }
    public final void join(long millis, int nanos) throws InterruptedException {
        join(roundedMillis(millis, nanos));
    }

    public static native void yield();
    public static native int activeCount();
    public static native boolean holdsLock(Object object);

    public final String getName() { return name; }
    public final void setName(String name) {
        if (name == null) throw new NullPointerException();
        this.name = name;
    }
    public final int getPriority() { return vmPriority; }
    public final void setPriority(int priority) {
        if (priority < MIN_PRIORITY || priority > MAX_PRIORITY)
            throw new IllegalArgumentException();
        vmPriority = priority;
    }

    public String toString() { return "Thread[" + name + "," + vmPriority + "]"; }

    private static long roundedMillis(long millis, int nanos) {
        if (millis < 0 || nanos < 0 || nanos > 999999)
            throw new IllegalArgumentException();
        if (nanos > 0 && millis < 9223372036854775807L) millis++;
        return millis;
    }
}
