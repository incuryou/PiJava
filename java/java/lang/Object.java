package java.lang;
public class Object {
    protected native Object clone() throws CloneNotSupportedException;
    public boolean equals(Object other) { return this == other; }
    public String toString() { return getClass().getName(); }
    public native Class getClass();
    public int hashCode() { return System.identityHashCode(this); }
    public final void wait() throws InterruptedException { wait(0); }
    public final native void wait(long timeout) throws InterruptedException;
    public final void wait(long timeout, int nanos) throws InterruptedException {
        if (timeout < 0 || nanos < 0 || nanos > 999999)
            throw new IllegalArgumentException();
        if (nanos > 0 && timeout < 9223372036854775807L) timeout++;
        wait(timeout);
    }
    public final native void notify();
    public final native void notifyAll();
}
