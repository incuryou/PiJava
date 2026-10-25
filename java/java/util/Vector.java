package java.util;
public class Vector {
    protected Object[] elementData;
    protected int elementCount;
    protected int capacityIncrement;
    public Vector() { this(10); }
    public Vector(int cap) { this(cap, 0); }
    public Vector(int cap, int increment) {
        if (cap < 0) throw new IllegalArgumentException("negative capacity");
        elementData = new Object[cap];
        capacityIncrement = increment;
    }
    private void checkIndex(int index) {
        if (index < 0 || index >= elementCount) throw new ArrayIndexOutOfBoundsException(Integer.toString(index));
    }
    public synchronized void addElement(Object obj) {
        ensureCapacity(elementCount + 1);
        elementData[elementCount++] = obj;
    }
    public synchronized Object elementAt(int index) { checkIndex(index); return elementData[index]; }
    public synchronized void setElementAt(Object obj, int index) { checkIndex(index); elementData[index] = obj; }
    public synchronized int size() { return elementCount; }
    public synchronized boolean isEmpty() { return elementCount == 0; }
    public synchronized Object firstElement() {
        if (elementCount == 0) throw new NoSuchElementException();
        return elementData[0];
    }
    public synchronized Object lastElement() {
        if (elementCount == 0) throw new NoSuchElementException();
        return elementData[elementCount - 1];
    }
    public synchronized void removeAllElements() {
        while (elementCount > 0) elementData[--elementCount] = null;
    }
    public int indexOf(Object obj) { return indexOf(obj, 0); }
    public synchronized int indexOf(Object obj, int index) {
        if (index < 0) throw new ArrayIndexOutOfBoundsException(Integer.toString(index));
        while (index < elementCount) {
            Object e = elementData[index];
            if (obj == null ? e == null : obj.equals(e)) return index;
            index++;
        }
        return -1;
    }
    public synchronized int lastIndexOf(Object obj) { return lastIndexOf(obj, elementCount - 1); }
    public synchronized int lastIndexOf(Object obj, int index) {
        if (index >= elementCount) throw new ArrayIndexOutOfBoundsException(Integer.toString(index));
        while (index >= 0) {
            Object e = elementData[index];
            if (obj == null ? e == null : obj.equals(e)) return index;
            index--;
        }
        return -1;
    }
    public boolean contains(Object obj) { return indexOf(obj) >= 0; }
    public synchronized void removeElementAt(int index) {
        checkIndex(index);
        int j = index;
        while (j + 1 < elementCount) { elementData[j] = elementData[j + 1]; j++; }
        elementData[--elementCount] = null;
    }
    public synchronized boolean removeElement(Object obj) {
        int i = indexOf(obj);
        if (i < 0) return false;
        removeElementAt(i);
        return true;
    }
    public synchronized void insertElementAt(Object obj, int index) {
        if (index < 0 || index > elementCount) throw new ArrayIndexOutOfBoundsException(Integer.toString(index));
        ensureCapacity(elementCount + 1);
        int j = elementCount;
        while (j > index) { elementData[j] = elementData[j - 1]; j--; }
        elementData[index] = obj;
        elementCount++;
    }
    public synchronized void copyInto(Object[] dest) {
        System.arraycopy(elementData, 0, dest, 0, elementCount);
    }
    public synchronized void ensureCapacity(int min) {
        if (min <= elementData.length) return;
        long cap = (long) elementData.length + (capacityIncrement > 0 ? capacityIncrement : elementData.length);
        if (cap < min) cap = min;
        if (cap > 2147483647L) cap = 2147483647L;
        Object[] next = new Object[(int) cap];
        System.arraycopy(elementData, 0, next, 0, elementCount);
        elementData = next;
    }
    public synchronized void trimToSize() {
        if (elementData.length == elementCount) return;
        Object[] next = new Object[elementCount];
        System.arraycopy(elementData, 0, next, 0, elementCount);
        elementData = next;
    }
    public synchronized int capacity() { return elementData.length; }
    public synchronized void setSize(int n) {
        if (n < 0) throw new ArrayIndexOutOfBoundsException(Integer.toString(n));
        ensureCapacity(n);
        while (elementCount > n) elementData[--elementCount] = null;
        while (elementCount < n) elementData[elementCount++] = null;
    }
    public synchronized Object[] toArray() {
        Object[] out = new Object[elementCount];
        copyInto(out);
        return out;
    }
    public Enumeration elements() { return new VecEnum(this); }
}
class VecEnum implements Enumeration {
    Vector vector;
    int index;
    VecEnum(Vector vector) { this.vector = vector; }
    public boolean hasMoreElements() { return index < vector.size(); }
    public Object nextElement() {
        synchronized (vector) {
            if (index >= vector.size()) throw new NoSuchElementException();
            return vector.elementAt(index++);
        }
    }
}
