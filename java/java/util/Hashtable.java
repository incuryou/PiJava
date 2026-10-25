package java.util;
public class Hashtable {
    private Object[] keys;
    private Object[] vals;
    private int count;
    public Hashtable() { this(11); }
    public Hashtable(int capacity) {
        if (capacity < 0) throw new IllegalArgumentException("negative capacity");
        if (capacity == 0) capacity = 1;
        keys = new Object[capacity]; vals = new Object[capacity];
    }
    private int find(Object key) {
        if (key == null) throw new NullPointerException();
        int i = 0;
        while (i < count) {
            Object k = keys[i];
            if (k == key || (k != null && k.equals(key))) return i;
            i = i + 1;
        }
        return -1;
    }
    public synchronized Object put(Object key, Object value) {
        if (value == null) throw new NullPointerException();
        int i = find(key);
        if (i >= 0) { Object old = vals[i]; vals[i] = value; return old; }
        if (count >= keys.length) {
            Object[] nk = new Object[keys.length * 2];
            Object[] nv = new Object[vals.length * 2];
            System.arraycopy(keys, 0, nk, 0, count);
            System.arraycopy(vals, 0, nv, 0, count);
            keys = nk; vals = nv;
        }
        keys[count] = key; vals[count] = value; count = count + 1;
        return null;
    }
    public synchronized Object get(Object key) { int i = find(key); return i < 0 ? null : vals[i]; }
    public synchronized Object remove(Object key) {
        int i = find(key);
        if (i < 0) return null;
        Object old = vals[i];
        int j = i;
        while (j + 1 < count) { keys[j] = keys[j + 1]; vals[j] = vals[j + 1]; j = j + 1; }
        count = count - 1; keys[count] = null; vals[count] = null;
        return old;
    }
    public synchronized void clear() {
        int i = 0;
        while (i < keys.length) { keys[i] = null; vals[i] = null; i = i + 1; }
        count = 0;
    }
    public synchronized int size() { return count; }
    public synchronized boolean isEmpty() { return count == 0; }
    public synchronized boolean containsKey(Object key) { return find(key) >= 0; }
    public synchronized boolean contains(Object value) {
        if (value == null) throw new NullPointerException();
        int i = 0;
        while (i < count) {
            Object v = vals[i];
            if (v == value || (v != null && v.equals(value))) return true;
            i = i + 1;
        }
        return false;
    }
    public synchronized Enumeration keys() { return new ArrEnum(keys, count); }
    public synchronized Enumeration elements() { return new ArrEnum(vals, count); }
}
class ArrEnum implements Enumeration {
    Object[] data; int n; int i;
    ArrEnum(Object[] data, int n) {
        // A table removal compacts its backing arrays. Keep this enumeration's
        // references stable while callers remove entries during traversal.
        this.data = new Object[n];
        System.arraycopy(data, 0, this.data, 0, n);
        this.n = n; i = 0;
    }
    public boolean hasMoreElements() { return i < n; }
    public Object nextElement() {
        if (i >= n) throw new NoSuchElementException();
        Object o = data[i]; i = i + 1; return o;
    }
}
