package java.util;
public class Stack extends Vector {
    public Stack() {}
    public Object push(Object obj) { addElement(obj); return obj; }
    public Object pop() { Object o = lastElement(); removeElementAt(size() - 1); return o; }
    public Object peek() { return lastElement(); }
    public synchronized int search(Object obj) {
        int index = lastIndexOf(obj);
        return index < 0 ? -1 : size() - index;
    }
    public boolean empty() { return size() == 0; }
}
