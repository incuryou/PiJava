/** Scheduling across native-to-Java callbacks, class initialization, and reflection. */
public class ThreadCallbackChecks {
    private static final long GUARD = 3000;
    private static int assertions;
    private static volatile boolean callbackEntered;
    private static volatile boolean callbackFlag;
    private static int initPhase;
    private static int literalInitialized;

    private static void check(boolean condition, String reason) {
        assertions++;
        if (!condition) throw new RuntimeException(reason);
    }
    private static abstract class Worker extends Thread {
        Throwable failure;
        public final void run() {
            try { work(); } catch (Throwable problem) { failure = problem; }
        }
        abstract void work() throws Exception;
    }
    private static void finish(Worker worker) throws Exception {
        worker.join(GUARD);
        check(!worker.isAlive(), "callback worker completed bounded join");
        if (worker.failure != null) throw new RuntimeException(worker.failure.toString());
    }

    private static class SleepingText {
        int active;
        int calls;
        public synchronized String toString() {
            active++;
            try {
                check(active == 1, "callback synchronized exclusion");
                check(Thread.holdsLock(this), "callback owns synchronized monitor");
                Thread.sleep(2);
                calls++;
                return "callback";
            } catch (InterruptedException unexpected) {
                throw new RuntimeException("unexpected callback interruption");
            } finally { active--; }
        }
    }
    private static void sleepingCallbacks() throws Exception {
        final SleepingText object = new SleepingText();
        Worker first = new Worker() {
            void work() {
                check(new StringBuilder().append(object).toString().equals("callback"),
                        "first append resumes after sleeping toString");
            }
        };
        Worker second = new Worker() {
            void work() {
                check(new StringBuilder().append(object).toString().equals("callback"),
                        "second append resumes after sleeping toString");
            }
        };
        first.start();
        second.start();
        finish(first);
        finish(second);
        check(object.calls == 2 && object.active == 0, "both callback invocations completed");
    }

    private static class BusyText {
        int iterations;
        int checksum;
        public String toString() {
            int value = 1 * 1664525 + 1013904223;
            int count = 1;
            callbackEntered = true;
            while (!callbackFlag && count < 2000000) {
                value = value * 1664525 + 1013904223;
                count++;
            }
            iterations = count;
            checksum = value;
            check(callbackFlag, "instruction slice permits peer inside toString callback");
            return "busy";
        }
    }
    private static void busyCallbacks() throws Exception {
        callbackEntered = false;
        callbackFlag = false;
        final BusyText object = new BusyText();
        Worker callback = new Worker() {
            void work() {
                check(new StringBuilder().append(object).toString().equals("busy"),
                        "busy callback returns through append");
            }
        };
        Worker setter = new Worker() {
            void work() {
                int count = 0;
                while (!callbackEntered && count < 2000000) count++;
                check(callbackEntered, "peer observed callback entry without explicit yield");
                callbackFlag = true;
            }
        };
        callback.start();
        setter.start();
        finish(callback);
        finish(setter);
        check(object.iterations > 0 && object.iterations < 2000000,
                "callback retained local loop state across instruction slice");
        check(object.checksum != 1, "callback retained computed local value");
    }

    public static class SlowInit {
        public static int value;
        static {
            initPhase = 1;
            try { Thread.sleep(10); }
            catch (InterruptedException unexpected) {
                throw new RuntimeException("unexpected initializer interruption");
            }
            value = 42;
            initPhase = 2;
        }
    }
    public static class LiteralOnly {
        static { literalInitialized++; }
        public LiteralOnly() { }
    }
    public static class SleepingConstructor {
        public int value;
        public SleepingConstructor() {
            try { Thread.sleep(2); }
            catch (InterruptedException unexpected) {
                throw new RuntimeException("unexpected constructor interruption");
            }
            value = 73;
        }
    }

    private static void initialization() throws Exception {
        Class literal = SlowInit.class;
        check(initPhase == 0, "class literal does not trigger static initialization");
        final int[] values = new int[2];
        Worker first = new Worker() { void work() { values[0] = SlowInit.value; } };
        Worker second = new Worker() { void work() { values[1] = SlowInit.value; } };
        first.start();
        second.start();
        finish(first);
        finish(second);
        check(values[0] == 42 && values[1] == 42,
                "concurrent active use waits for full static initialization");
        check(initPhase == 2, "sleeping initializer completed once");
        check(Class.forName("ThreadCallbackChecks$SlowInit") == literal,
                "initialized class has canonical forName and literal identity");
        check(new SlowInit().getClass() == literal, "instance getClass uses canonical identity");

        Class other = LiteralOnly.class;
        check(literalInitialized == 0, "unused class literal leaves initializer untouched");
        check(Class.forName("ThreadCallbackChecks$LiteralOnly") == other,
                "forName returns literal canonical class");
        check(literalInitialized == 1, "forName actively initializes class once");
        check(new LiteralOnly().getClass() == other && literalInitialized == 1,
                "getClass identity preserves completed initializer");
    }

    private static void reflection() throws Exception {
        final Class constructorClass = Class.forName("ThreadCallbackChecks$SleepingConstructor");
        Worker constructor = new Worker() {
            void work() throws Exception {
                Object object = constructorClass.newInstance();
                check(object.getClass() == constructorClass,
                        "reflective constructor returns canonical class instance");
                check(((SleepingConstructor) object).value == 73,
                        "reflective constructor resumes after native-trampoline sleep");
            }
        };
        constructor.start();
        finish(constructor);
    }

    public static int run() throws Exception {
        assertions = 0;
        sleepingCallbacks();
        busyCallbacks();
        initialization();
        reflection();
        return assertions;
    }
}
