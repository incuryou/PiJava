package javax.microedition.midlet;
public abstract class MIDlet {
    protected MIDlet() {}
    protected abstract void startApp() throws MIDletStateChangeException;
    protected abstract void pauseApp();
    protected abstract void destroyApp(boolean unconditional) throws MIDletStateChangeException;
    public final native void notifyDestroyed();
    public final native void notifyPaused();
    public final native void resumeRequest();
    // Host lifecycle boundary: no exception may escape and skip native cleanup.
    public final int __vmStart() {
        try { startApp(); return 0; }
        catch (MIDletStateChangeException retry) { return 1; }
        catch (Throwable failure) { failure.printStackTrace(); return 2; }
    }
    // VM-owned Java continuation; no C++ stack is serialized or abandoned.
    public final void __vmStartAsync() {
        new Thread(new StartTask(this), "MIDlet-start").start();
    }
    private final native void __vmStartCompleted(int outcome);
    private static final class StartTask implements Runnable {
        private final MIDlet owner;
        StartTask(MIDlet owner) { this.owner = owner; }
        public void run() { owner.__vmStartCompleted(owner.__vmStart()); }
    }
    public final int __vmPause() {
        try { pauseApp(); return 0; }
        catch (Throwable failure) { failure.printStackTrace(); return 2; }
    }
    public final void __vmDestroy() {
        try { destroyApp(true); }
        catch (Throwable failure) { failure.printStackTrace(); }
    }
    // 0.2.107: "sms:" / "smsto:" / "mms:" / "tel:" requests (SMS/call payment) are treated as done and
    // logged (SMSSKIP); other URLs are accepted silently as before, no external app. false = no exit needed.
    public final boolean platformRequest(String url) {
        if (url != null) {
            String lower = url.toLowerCase();
            if (lower.startsWith("sms:") || lower.startsWith("smsto:") || lower.startsWith("mms:") || lower.startsWith("tel:")) {
                int body = url.indexOf("body=");
                javax.microedition.io.Connector.__vmSmsSkip(lower.startsWith("tel:") ? "platformRequest-tel" : "platformRequest", url, body < 0 ? 0 : url.length() - body - 5, false);
            }
        }
        return false;
    }
    public final native String getAppProperty(String key);
}
