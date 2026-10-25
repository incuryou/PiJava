/** P2.1-c4 condition / c5: two REAL Java threads yield through the production scheduler.
 *  Each keeps its own deep stack (locals), its own Thread identity and its own exception state across yields. */
public final class ThreadSliceChecks {
    static int[] log = new int[64];
    static int n, errors, doneA, doneB, caughtA, caughtB, resultA, resultB;
    static int deep(int id, int depth, Thread me) {
        if (depth == 0) {
            for (int k = 0; k < 3; k++) {
                log[n++] = id * 10 + k;
                try {
                    if (id == 1) throw new IllegalStateException("A");   // A is mid-exception-handling...
                    else { int z = k - k; int q = 7 / z; log[63] = q; }  // ...B throws ArithmeticException
                } catch (IllegalStateException e) {
                    Thread.yield();                                     // ...and yields INSIDE its catch block
                    if (id != 1) errors++; caughtA++;
                } catch (ArithmeticException e) {
                    Thread.yield();
                    if (id != 2) errors++; caughtB++;
                }
                if (Thread.currentThread() != me) errors += 100;        // running_thread is my own after resume
            }
            return id * 1000;
        }
        int local = id * 100 + depth;                                   // lives in this thread's own frame stack
        int r = deep(id, depth - 1, me);
        if (local != id * 100 + depth) errors += 10;
        return r + depth;
    }
    static final class W implements Runnable {
        final int id; W(int i) { id = i; }
        public void run() {
            int d = id == 1 ? 6 : 3;
            int r = deep(id, d, Thread.currentThread());
            if (id == 1) { resultA = r; doneA = 1; } else { resultB = r; doneB = 1; }
        }
    }
    static int start(int unused) {
        n = 0; errors = 0; doneA = doneB = caughtA = caughtB = 0;
        new Thread(new W(1)).start(); new Thread(new W(2)).start();
        return 1;
    }
}
