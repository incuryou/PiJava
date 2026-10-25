/** Deterministic cooperative-thread regressions; returns the assertion count. */
public class ThreadChecks {
    private static final long GUARD = 3000;
    private static int assertions;

    private static void check(boolean condition, String reason) {
        assertions++;
        if (!condition) throw new RuntimeException(reason);
    }

    private static abstract class Worker extends Thread {
        volatile int phase;
        Throwable failure;
        Thread identity;
        public final void run() {
            identity = Thread.currentThread();
            try { work(); } catch (Throwable problem) { failure = problem; }
        }
        abstract void work() throws Exception;
    }

    private static void ready(Worker worker) throws Exception {
        long limit = System.currentTimeMillis() + GUARD;
        while (worker.phase == 0 && worker.isAlive() && System.currentTimeMillis() < limit)
            Thread.sleep(1);
        check(worker.phase != 0, "worker reached rendezvous");
        check(worker.failure == null, "worker rendezvous succeeded");
    }

    private static void finish(Worker worker) throws Exception {
        worker.join(GUARD);
        check(!worker.isAlive(), "worker completed bounded join");
        if (worker.failure != null) throw new RuntimeException(worker.failure.toString());
        check(worker.identity == worker, "subclass currentThread identity");
    }

    private static class GateWorker extends Worker {
        volatile boolean release;
        void work() throws Exception {
            phase = 1;
            long limit = System.currentTimeMillis() + GUARD;
            while (!release && System.currentTimeMillis() < limit) Thread.yield();
            check(release, "gate released before guard");
            phase = 2;
        }
    }

    private static class Task implements Runnable {
        volatile boolean release;
        volatile int phase;
        Thread identity;
        Throwable failure;
        public void run() {
            try {
                identity = Thread.currentThread();
                phase = 1;
                long limit = System.currentTimeMillis() + GUARD;
                while (!release && System.currentTimeMillis() < limit) Thread.yield();
                check(release, "Runnable gate released");
                phase = 2;
            } catch (Throwable problem) { failure = problem; }
        }
    }

    private static void lifecycle() throws Exception {
        Thread main = Thread.currentThread();
        check(main != null && main == Thread.currentThread(), "stable main identity");
        check(main.isAlive(), "main is alive");
        GateWorker first = new GateWorker();
        GateWorker second = new GateWorker();
        Task task = new Task();
        Thread runnable = new Thread(task);
        check(!first.isAlive() && !second.isAlive() && !runnable.isAlive(), "new threads not alive");
        first.start();
        second.start();
        runnable.start();
        check(first.isAlive() && second.isAlive() && runnable.isAlive(), "started threads alive");
        boolean rejected = false;
        try { first.start(); } catch (IllegalThreadStateException expected) { rejected = true; }
        check(rejected, "duplicate start rejected");
        ready(first);
        ready(second);
        long limit = System.currentTimeMillis() + GUARD;
        while (task.phase == 0 && System.currentTimeMillis() < limit) Thread.sleep(1);
        check(task.phase == 1, "Runnable entered run");
        check(first.identity == first && second.identity == second && task.identity == runnable,
                "distinct subclass and Runnable identities");
        check(first.identity != second.identity && first.identity != main && task.identity != main,
                "workers differ from each other and main");
        first.join(2);
        check(first.isAlive() && first.phase == 1, "timed join leaves running target alive");
        first.release = true;
        second.release = true;
        task.release = true;
        finish(first);
        finish(second);
        runnable.join(GUARD);
        check(!runnable.isAlive() && task.phase == 2 && task.failure == null,
                "Runnable completed and joined");
        check(first.phase == 2 && second.phase == 2, "multiple subclasses completed");
        rejected = false;
        try { first.start(); } catch (IllegalThreadStateException expected) { rejected = true; }
        check(rejected, "terminated thread restart rejected");
        first.join();
        check(!first.isAlive(), "joining terminated thread returns");
        new Thread().join();
        check(Thread.currentThread() == main, "main identity survives nested scheduler");
    }

    private static class Counter {
        int active;
        int value;
        synchronized void increment() {
            active++;
            try {
                check(active == 1, "instance synchronized exclusion");
                int previous = value;
                Thread.yield();
                nested();
                value = previous + 1;
            } finally { active--; }
        }
        synchronized void nested() {
            check(Thread.holdsLock(this), "reentrant synchronized method holds instance");
            synchronized (this) { Thread.yield(); }
        }
        synchronized void explode() {
            synchronized (this) { throw new RuntimeException("expected monitor unwind"); }
        }
        synchronized void afterException() { value++; }
    }

    private static int staticActive;
    private static int staticValue;
    private static synchronized void incrementStatic() {
        staticActive++;
        try {
            check(staticActive == 1, "static synchronized exclusion");
            check(Thread.holdsLock(ThreadChecks.class), "static method locks canonical class");
            int previous = staticValue;
            Thread.yield();
            staticValue = previous + 1;
        } finally { staticActive--; }
    }

    private static void monitors() throws Exception {
        final Counter counter = new Counter();
        Worker a = new Worker() {
            void work() { for (int i = 0; i < 4; i++) { counter.increment(); incrementStatic(); } }
        };
        Worker b = new Worker() {
            void work() { for (int i = 0; i < 4; i++) { counter.increment(); incrementStatic(); } }
        };
        staticActive = 0;
        staticValue = 0;
        a.start();
        b.start();
        finish(a);
        finish(b);
        check(counter.value == 8 && counter.active == 0, "instance synchronized increments preserved");
        check(staticValue == 8 && staticActive == 0, "static synchronized increments preserved");

        final Object other = new Object();
        Worker independent = new Worker() {
            void work() {
                synchronized (other) {
                    check(Thread.holdsLock(other), "different monitor acquired");
                    phase = 1;
                }
            }
        };
        synchronized (counter) {
            synchronized (counter) { check(Thread.holdsLock(counter), "block monitor reentrancy"); }
            check(Thread.holdsLock(counter), "outer recursive monitor remains owned");
            independent.start();
            finish(independent);
        }
        check(!Thread.holdsLock(counter), "outer monitor exit releases ownership");

        Worker canonical = new Worker() {
            void work() { phase = 1; incrementStatic(); phase = 2; }
        };
        synchronized (ThreadChecks.class) {
            canonical.start();
            for (int i = 0; i < 8; i++) Thread.yield();
            check(staticValue == 8 && canonical.phase != 2,
                    "class literal excludes static synchronized entry");
        }
        finish(canonical);
        check(staticValue == 9, "static method resumes after class monitor exit");

        Worker throwing = new Worker() { void work() { counter.explode(); } };
        throwing.start();
        throwing.join(GUARD);
        check(!throwing.isAlive() && throwing.failure instanceof RuntimeException,
                "exception escaped synchronized method");
        Worker successor = new Worker() { void work() { counter.afterException(); } };
        successor.start();
        finish(successor);
        check(counter.value == 9, "exception releases recursive method monitor");
    }

    private static class WaitWorker extends Worker {
        final Object lock;
        volatile boolean signal;
        boolean interrupted;
        WaitWorker(Object lock) { this.lock = lock; }
        void work() throws Exception {
            synchronized (lock) {
                synchronized (lock) {
                    phase = 1;
                    long limit = System.currentTimeMillis() + GUARD;
                    try {
                        while (!signal && System.currentTimeMillis() < limit) lock.wait(GUARD);
                        check(signal, "wait notified before guard");
                    } catch (InterruptedException expected) {
                        interrupted = true;
                        check(!Thread.currentThread().isInterrupted(), "wait interrupt flag cleared");
                        check(!Thread.interrupted(), "wait clearing visible to interrupted");
                    }
                    check(Thread.holdsLock(lock), "wait reacquires recursive monitor");
                    lock.notify();
                    phase = 2;
                }
                check(Thread.holdsLock(lock), "wait restores full recursion depth");
                phase = 3;
            }
        }
    }

    private static void waitNotify() throws Exception {
        final Object lock = new Object();
        WaitWorker waiter = new WaitWorker(lock);
        waiter.start();
        ready(waiter);
        synchronized (lock) {
            check(waiter.phase == 1, "wait releases all recursive ownership");
            waiter.signal = true;
            lock.notify();
            for (int i = 0; i < 8; i++) Thread.yield();
            check(waiter.phase == 1, "notify retains notifying monitor");
        }
        finish(waiter);
        check(waiter.phase == 3 && !waiter.interrupted, "notified waiter completed");

        WaitWorker first = new WaitWorker(lock);
        WaitWorker second = new WaitWorker(lock);
        first.start();
        second.start();
        ready(first);
        ready(second);
        synchronized (lock) {
            first.signal = true;
            second.signal = true;
            lock.notifyAll();
        }
        finish(first);
        finish(second);
        check(first.phase == 3 && second.phase == 3, "notifyAll wakes multiple waiters");
        synchronized (lock) {
            lock.wait(2);
            check(Thread.holdsLock(lock), "timed wait returns owning monitor");
        }
    }

    private static void interrupts() throws Exception {
        Thread self = Thread.currentThread();
        check(!self.isInterrupted(), "main begins un-interrupted");
        self.interrupt();
        check(self.isInterrupted() && self.isInterrupted(), "isInterrupted preserves flag");
        check(Thread.interrupted(), "interrupted reports self flag");
        check(!Thread.interrupted() && !self.isInterrupted(), "interrupted clears self flag");
        self.interrupt();
        boolean interrupted = false;
        try { Thread.sleep(0); } catch (InterruptedException expected) { interrupted = true; }
        check(interrupted && !self.isInterrupted(), "pre-interrupted sleep clears flag");
        final Object lock = new Object();
        synchronized (lock) {
            self.interrupt();
            interrupted = false;
            try { lock.wait(GUARD); } catch (InterruptedException expected) { interrupted = true; }
            check(interrupted && !self.isInterrupted() && Thread.holdsLock(lock),
                    "pre-interrupted wait retains monitor and clears flag");
        }

        Worker sleeper = new Worker() {
            void work() throws Exception {
                phase = 1;
                boolean caught = false;
                try { Thread.sleep(GUARD); } catch (InterruptedException expected) { caught = true; }
                check(caught && !isInterrupted() && !Thread.interrupted(),
                        "blocked sleep interrupted and flag cleared");
                phase = 2;
            }
        };
        sleeper.start();
        ready(sleeper);
        sleeper.interrupt();
        finish(sleeper);
        check(sleeper.phase == 2, "sleep interrupt resumed worker");

        final GateWorker target = new GateWorker();
        target.start();
        ready(target);
        Worker joiner = new Worker() {
            void work() throws Exception {
                phase = 1;
                boolean caught = false;
                try { target.join(GUARD); } catch (InterruptedException expected) { caught = true; }
                check(caught && !isInterrupted() && !Thread.interrupted(),
                        "blocked join interrupted and flag cleared");
                phase = 2;
            }
        };
        joiner.start();
        ready(joiner);
        joiner.interrupt();
        finish(joiner);
        check(target.isAlive(), "interrupting join does not terminate target");
        target.release = true;
        finish(target);

        WaitWorker waiter = new WaitWorker(lock);
        waiter.start();
        ready(waiter);
        synchronized (lock) {
            waiter.interrupt();
            for (int i = 0; i < 8; i++) Thread.yield();
            check(waiter.phase == 1, "wait interruption deferred until monitor reacquisition");
        }
        finish(waiter);
        check(waiter.interrupted && waiter.phase == 3, "interrupted wait completed after reacquisition");
        check(!self.isInterrupted(), "worker interruptions do not affect main");
    }

    private static void invalidArguments() throws Exception {
        Object lock = new Object();
        boolean rejected = false;
        try { lock.notify(); } catch (IllegalMonitorStateException expected) { rejected = true; }
        check(rejected, "unheld notify rejected");
        rejected = false;
        try { lock.notifyAll(); } catch (IllegalMonitorStateException expected) { rejected = true; }
        check(rejected, "unheld notifyAll rejected");
        rejected = false;
        try { lock.wait(1); } catch (IllegalMonitorStateException expected) { rejected = true; }
        check(rejected, "unheld wait rejected");
        rejected = false;
        try { Thread.sleep(-1); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "negative sleep rejected");
        rejected = false;
        try { new Thread().join(-1); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "negative join rejected even before start");
        synchronized (lock) {
            rejected = false;
            try { lock.wait(-1); } catch (IllegalArgumentException expected) { rejected = true; }
            check(rejected, "negative wait rejected");
            rejected = false;
            try { lock.wait(0, 1000000); } catch (IllegalArgumentException expected) { rejected = true; }
            check(rejected, "invalid wait nanos rejected");
        }
        rejected = false;
        try { Thread.sleep(0, -1); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "negative sleep nanos rejected");
        rejected = false;
        try { new Thread().join(0, 1000000); } catch (IllegalArgumentException expected) { rejected = true; }
        check(rejected, "invalid join nanos rejected");
    }

    public static int run() throws Exception {
        assertions = 0;
        invalidArguments();
        lifecycle();
        monitors();
        waitNotify();
        interrupts();
        return assertions;
    }
}
