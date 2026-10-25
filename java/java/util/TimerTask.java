package java.util;
/** Scheduling state is ordinary Java state, included in instant snapshots. */
public abstract class TimerTask implements Runnable {
    boolean cancelled;
    int state; // 0 virgin, 1 scheduled, 2 executed, 3 cancelled
    long nextTime;
    long period;
    long scheduledTime;
    protected TimerTask() {}
    public abstract void run();
    public synchronized boolean cancel() {
        boolean prevented = state == 1;
        cancelled = true; state = 3;
        return prevented;
    }
    public synchronized long scheduledExecutionTime() { return scheduledTime; }
}
