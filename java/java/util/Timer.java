package java.util;
/** One cooperative Java thread per timer, with an ordered task queue. */
public class Timer {
    private TimerQueue queue;
    public Timer() {
        queue = new TimerQueue();
        new Thread(queue, "Timer").start();
    }
    public void schedule(TimerTask task, long delay) { enqueue(task, after(delay), 0); }
    public void schedule(TimerTask task, Date time) {
        if (time == null) throw new NullPointerException();
        enqueue(task, time.getTime(), 0);
    }
    public void schedule(TimerTask task, long delay, long period) {
        if (period <= 0) throw new IllegalArgumentException("period");
        enqueue(task, after(delay), -boundedPeriod(period));
    }
    public void schedule(TimerTask task, Date time, long period) {
        if (time == null) throw new NullPointerException();
        if (period <= 0) throw new IllegalArgumentException("period");
        enqueue(task, time.getTime(), -boundedPeriod(period));
    }
    public void scheduleAtFixedRate(TimerTask task, long delay, long period) {
        if (period <= 0) throw new IllegalArgumentException("period");
        enqueue(task, after(delay), boundedPeriod(period));
    }
    public void scheduleAtFixedRate(TimerTask task, Date time, long period) {
        if (time == null) throw new NullPointerException();
        if (period <= 0) throw new IllegalArgumentException("period");
        enqueue(task, time.getTime(), boundedPeriod(period));
    }
    private static long boundedPeriod(long period) {
        return period > 4611686018427387903L ? 4611686018427387903L : period;
    }
    private static long after(long delay) {
        if (delay < 0) throw new IllegalArgumentException("delay");
        long now = System.currentTimeMillis();
        if (now > 9223372036854775807L - delay) throw new IllegalArgumentException("time overflow");
        return now + delay;
    }
    private void enqueue(TimerTask task, long time, long period) {
        if (task == null) throw new NullPointerException();
        if (time < 0) throw new IllegalArgumentException("time");
        synchronized (queue) {
            if (queue.closed) throw new IllegalStateException("Timer cancelled");
            synchronized (task) {
                if (task.state != 0 || task.cancelled) throw new IllegalStateException("Task already used");
                task.nextTime = time;
                task.period = period;
                task.state = 1;
                queue.add(task);
            }
            queue.notifyAll();
        }
    }
    public void cancel() {
        synchronized (queue) {
            queue.closed = true;
            queue.clear();
            queue.notifyAll();
        }
    }
    /** Optional Java-compatible maintenance method; never changes task execution. */
    public int purge() {
        synchronized (queue) {
            int removed = 0;
            TimerTask[] old = queue.tasks;
            int oldCount = queue.count;
            queue.tasks = new TimerTask[old.length];
            queue.count = 0;
            for (int i = 0; i < oldCount; i++) {
                if (old[i].cancelled) removed++;
                else queue.add(old[i]);
            }
            if (removed != 0) queue.notifyAll();
            return removed;
        }
    }
}
final class TimerQueue implements Runnable {
    TimerTask[] tasks = new TimerTask[8];
    int count;
    boolean closed;
    void add(TimerTask task) {
        if (count == tasks.length) {
            TimerTask[] next = new TimerTask[tasks.length * 2];
            System.arraycopy(tasks, 0, next, 0, count);
            tasks = next;
        }
        int i = count++;
        while (i > 0) {
            int p = (i - 1) / 2;
            if (tasks[p].nextTime <= task.nextTime) break;
            tasks[i] = tasks[p]; i = p;
        }
        tasks[i] = task;
    }
    void removeAt(int at) {
        TimerTask last = tasks[--count]; tasks[count] = null;
        if (at == count) return;
        int i = at;
        while (i > 0) {
            int p = (i - 1) / 2;
            if (tasks[p].nextTime <= last.nextTime) break;
            tasks[i] = tasks[p]; i = p;
        }
        if (i == at) {
            while (2 * i + 1 < count) {
                int c = 2 * i + 1;
                if (c + 1 < count && tasks[c + 1].nextTime < tasks[c].nextTime) c++;
                if (tasks[c].nextTime >= last.nextTime) break;
                tasks[i] = tasks[c]; i = c;
            }
        }
        tasks[i] = last;
    }
    void clear() { while (count > 0) tasks[--count] = null; }
    public void run() {
        try {
            while (true) {
                TimerTask ready = null;
                synchronized (this) {
                    while (!closed && count == 0) {
                        try { wait(); } catch (InterruptedException ignored) {}
                    }
                    if (closed) return;
                    TimerTask task = tasks[0];
                    synchronized (task) {
                        if (task.cancelled) { removeAt(0); continue; }
                        long now = System.currentTimeMillis();
                        if (task.nextTime <= now) {
                            ready = task;
                            task.scheduledTime = task.nextTime;
                            removeAt(0);
                            if (task.period == 0) task.state = 2;
                            else {
                                long base = task.period < 0 ? now : task.nextTime;
                                long step = task.period < 0 ? -task.period : task.period;
                                if (base > 9223372036854775807L - step) task.state = 2;
                                else { task.nextTime = base + step; add(task); }
                            }
                        }
                    }
                    if (ready == null) {
                        long remaining = task.nextTime - System.currentTimeMillis();
                        if (remaining > 0) {
                            try { wait(remaining); } catch (InterruptedException ignored) {}
                        }
                    }
                }
                // User code must not run while the queue lock is held.
                if (ready != null) ready.run();
            }
        } finally {
            synchronized (this) { closed = true; clear(); notifyAll(); }
        }
    }
}
