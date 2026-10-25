package javax.microedition.m3g;
import java.util.Vector;
/** CLDC collection adapter for the upstream M3G implementation. */
final class M3GVector<E> {
 private Vector elements;
 M3GVector() { elements = new Vector(); }
 M3GVector(int capacity) { elements = new Vector(capacity); }
 void add(int index,E value) { elements.insertElementAt(value,index); }
 void add(E value) { elements.addElement(value); }
 void addElement(E value) { elements.addElement(value); }
 E get(int index) { return (E) elements.elementAt(index); }
 E elementAt(int index) { return get(index); }
 E set(int index, E value) { E old = get(index); elements.setElementAt(value,index); return old; }
 E remove(int index) { E old=get(index); elements.removeElementAt(index); return old; }
 boolean remove(Object value) { return elements.removeElement(value); }
 boolean removeElement(Object value) { return elements.removeElement(value); }
 void removeElementAt(int index) { elements.removeElementAt(index); }
 void removeAllElements() { elements.removeAllElements(); }
 void clear() { elements.removeAllElements(); }
 boolean contains(Object value) { return elements.contains(value); }
 int indexOf(Object value) { return elements.indexOf(value); }
 int size() { return elements.size(); }
 boolean isEmpty() { return elements.isEmpty(); }
 public Object clone() { M3GVector<E> copy = new M3GVector<E>(size()); for(int i=0;i<size();i++) copy.add(get(i)); return copy; }
}
