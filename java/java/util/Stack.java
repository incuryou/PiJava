package java.util;
public class Stack extends Vector {
    public Stack() {}
    public Object push(Object obj) { addElement(obj); return obj; }
    public Object pop() { Object o = lastElement(); removeElementAt(size() - 1); return o; }
    public Object peek() { return lastElement(); }
    public boolean empty() { return size() == 0; }
}
