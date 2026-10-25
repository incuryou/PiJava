import java.io.ByteArrayOutputStream;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;

/** Throwable formatting, saved Java frames, VM exceptions, and Java stream dispatch. */
public class ThrowableChecks {
    private static int assertions;
    private static void check(boolean condition, String reason) {
        assertions++;
        if (!condition) throw new RuntimeException(reason);
    }
    private static String trace(Throwable throwable) throws Exception {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        PrintStream stream = new PrintStream(bytes, false, "UTF-8");
        throwable.printStackTrace(stream);
        stream.flush();
        check(!stream.checkError(), "trace stream has no write error");
        return bytes.toString("UTF-8");
    }
    private static Exception createLeaf() { return new Exception("saved 中文 creation"); }
    private static Exception createMiddle() { return createLeaf(); }
    private static void rethrowFromHere(Exception throwable) throws Exception { throw throwable; }
    private static Throwable refreshHere(Throwable throwable) { return throwable.fillInStackTrace(); }

    private static class LocalizedException extends Exception {
        LocalizedException() { super("original"); }
        public String getLocalizedMessage() { return "翻译"; }
    }
    private static void messages() throws Exception {
        Throwable empty = new Throwable();
        check(empty.getMessage() == null && empty.getLocalizedMessage() == null,
                "default Throwable has null message");
        check(empty.toString().equals("java.lang.Throwable"), "default Throwable formats type only");
        Exception nullMessage = new Exception((String) null);
        check(nullMessage.getMessage() == null && nullMessage.getLocalizedMessage() == null,
                "Exception inherits null message accessors");
        check(nullMessage.toString().equals("java.lang.Exception"), "null message formats type only");
        Exception blank = new Exception("");
        check(blank.getMessage().equals("") && blank.getLocalizedMessage().equals(""),
                "empty message remains empty");
        check(blank.toString().equals("java.lang.Exception: "), "empty message includes separator");
        RuntimeException text = new RuntimeException("中文诊断");
        check(text.getLocalizedMessage().equals("中文诊断"), "localized message defaults to original");
        check(text.toString().equals("java.lang.RuntimeException: 中文诊断"),
                "nonempty message formats runtime type and Unicode message");
        LocalizedException localized = new LocalizedException();
        check(localized.getMessage().equals("original") && localized.getLocalizedMessage().equals("翻译"),
                "localized override preserves original message");
        check(localized.toString().equals("ThrowableChecks$LocalizedException: 翻译"),
                "toString dispatches localized message override");
        String printed = trace(text);
        check(printed.indexOf("java.lang.RuntimeException: 中文诊断") >= 0,
                "UTF-8 trace preserves type and Chinese message");
    }
    private static void savedFrames() throws Exception {
        Exception throwable = createMiddle();
        String original = trace(throwable);
        check(original.indexOf("ThrowableChecks.createLeaf") >= 0,
                "creation trace contains leaf class and method");
        check(original.indexOf("ThrowableChecks.createMiddle") >= 0,
                "creation trace contains calling helper");
        check(original.indexOf("saved 中文 creation") >= 0, "creation trace preserves Unicode header");
        boolean caught = false;
        try { rethrowFromHere(throwable); }
        catch (Exception expected) {
            caught = true;
            check(expected == throwable, "rethrow preserves Throwable identity");
        }
        check(caught, "saved Throwable was rethrown and caught");
        System.gc();
        String afterThrow = trace(throwable);
        check(afterThrow.equals(original), "rethrow and collection preserve original captured trace");
        check(afterThrow.indexOf("rethrowFromHere") < 0, "rethrow does not replace creation frames");
        check(refreshHere(throwable) == throwable, "fillInStackTrace returns same Throwable");
        String refreshed = trace(throwable);
        check(refreshed.indexOf("ThrowableChecks.refreshHere") >= 0,
                "fillInStackTrace captures refreshing helper");
        check(refreshed.indexOf("ThrowableChecks.createLeaf") < 0,
                "fillInStackTrace replaces original creation chain");
        check(!refreshed.equals(original), "refreshed trace differs from original");
    }
    private static int divideHere(int zero) { return 17 / zero; }
    private static int boundsHere(int index) { return (new int[1])[index]; }
    private static int nullHere(Object object) { return object.hashCode(); }
    private static char nativeBoundsHere(String text) { return text.charAt(-1); }
    private static void vmExceptions() throws Exception {
        boolean caught = false;
        try { divideHere(0); }
        catch (ArithmeticException expected) {
            caught = true;
            String text = trace(expected);
            check(text.indexOf("java.lang.ArithmeticException") >= 0,
                    "division VM exception trace has exception type");
            check(text.indexOf("ThrowableChecks.divideHere") >= 0,
                    "division VM exception captures throwing Java frame");
        }
        check(caught, "division by zero exception caught");
        caught = false;
        try { boundsHere(3); }
        catch (ArrayIndexOutOfBoundsException expected) {
            caught = true;
            check(trace(expected).indexOf("ThrowableChecks.boundsHere") >= 0,
                    "array VM exception captures throwing Java frame");
        }
        check(caught, "array bounds exception caught");
        caught = false;
        try { nullHere(null); }
        catch (NullPointerException expected) {
            caught = true;
            check(trace(expected).indexOf("ThrowableChecks.nullHere") >= 0,
                    "null-reference VM exception captures throwing Java frame");
        }
        check(caught, "null-reference exception caught");
        caught = false;
        try { nativeBoundsHere("x"); }
        catch (StringIndexOutOfBoundsException expected) {
            caught = true;
            check(trace(expected).indexOf("ThrowableChecks.nativeBoundsHere") >= 0,
                    "native VM exception captures calling Java frame");
        }
        check(caught, "native string bounds exception caught");
        caught = false;
        try { new Exception("null stream").printStackTrace((PrintStream) null); }
        catch (NullPointerException expected) { caught = true; }
        check(caught, "null explicit trace stream is rejected");
    }

    private static class SchedulingStream extends PrintStream {
        int calls;
        Thread owner;
        SchedulingStream(ByteArrayOutputStream bytes) throws UnsupportedEncodingException {
            super(bytes, false, "UTF-8");
        }
        public void print(String text) {
            calls++;
            if (Thread.currentThread() != owner) throw new RuntimeException("trace stream caller identity");
            try { Thread.sleep(1); }
            catch (InterruptedException unexpected) { throw new RuntimeException("trace stream interrupted"); }
            super.print(text);
        }
    }
    private static class TraceWorker extends Thread {
        final Exception throwable;
        final ByteArrayOutputStream bytes;
        final SchedulingStream stream;
        Throwable failure;
        String rendered;
        TraceWorker(Exception throwable) throws Exception {
            this.throwable = throwable;
            bytes = new ByteArrayOutputStream();
            stream = new SchedulingStream(bytes);
        }
        public void run() {
            try {
                stream.owner = Thread.currentThread();
                throwable.printStackTrace(stream);
                stream.flush();
                check(!stream.checkError(), "scheduled trace stream has no error");
                rendered = bytes.toString("UTF-8");
            } catch (Throwable problem) { failure = problem; }
        }
    }
    private static void streamDispatchAndThreads() throws Exception {
        Exception throwable = createMiddle();
        TraceWorker worker = new TraceWorker(throwable);
        System.gc();
        worker.start();
        worker.join(3000);
        check(!worker.isAlive(), "printing worker completes bounded join");
        if (worker.failure != null) throw new RuntimeException(worker.failure.toString());
        check(worker.stream.calls > 0, "printStackTrace dispatches Java PrintStream override");
        check(worker.rendered.indexOf("ThrowableChecks.createLeaf") >= 0,
                "other thread prints original creation frame");
        check(worker.rendered.indexOf("ThrowableChecks$TraceWorker.run") < 0,
                "other thread does not overwrite saved creation stack");
        System.gc();
        check(trace(throwable).indexOf("ThrowableChecks.createMiddle") >= 0,
                "saved creation frames survive cross-thread printing and collection");
    }
    private static void systemStreams() {
        Exception marker = new Exception("标准输出中文标记");
        marker.printStackTrace(System.out);
        check(!System.out.checkError(), "System.out Unicode trace prints without error");
        new Exception("默认错误中文标记").printStackTrace();
        check(!System.err.checkError(), "default System.err Unicode trace prints without error");
    }
    public static int run() throws Exception {
        assertions = 0;
        messages();
        savedFrames();
        vmExceptions();
        streamDispatchAndThreads();
        systemStreams();
        return assertions;
    }
}
