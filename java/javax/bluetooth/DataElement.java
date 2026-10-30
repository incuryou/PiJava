package javax.bluetooth;

import java.util.Vector;

/** Portable SDP attribute values. This class does not access a Bluetooth radio. */
public class DataElement {
    public static final int NULL=0, U_INT_1=8, U_INT_2=9, U_INT_4=10,
        U_INT_8=11, U_INT_16=12, INT_1=16, INT_2=17, INT_4=18,
        INT_8=19, INT_16=20, UUID=24, STRING=32, BOOL=40,
        DATSEQ=48, DATALT=56, URL=64;
    private final int type;
    private long number;
    private boolean bool;
    private Object value;

    public DataElement(int valueType) {
        if (valueType!=NULL && valueType!=DATSEQ && valueType!=DATALT)
            throw new IllegalArgumentException("Not an empty SDP type");
        type=valueType;
        if (type!=NULL) value=new Vector();
    }
    public DataElement(boolean bool) { type=BOOL; this.bool=bool; }
    public DataElement(int valueType,long value) {
        long low,high;
        switch(valueType) {
            case U_INT_1: low=0; high=255; break;
            case U_INT_2: low=0; high=65535; break;
            case U_INT_4: low=0; high=4294967295L; break;
            case INT_1: low=-128; high=127; break;
            case INT_2: low=-32768; high=32767; break;
            case INT_4: low=-2147483648L; high=2147483647L; break;
            case INT_8: low=Long.MIN_VALUE; high=Long.MAX_VALUE; break;
            default: throw new IllegalArgumentException("Not a long SDP type");
        }
        if(value<low || value>high) throw new IllegalArgumentException("SDP integer out of range");
        type=valueType; number=value;
    }
    public DataElement(int valueType,Object value) {
        if(value==null) throw new IllegalArgumentException("Null SDP object");
        switch(valueType) {
            case STRING: case URL:
                if(!(value instanceof String)) throw new IllegalArgumentException("SDP string required");
                break;
            case UUID:
                if(!(value instanceof javax.bluetooth.UUID)) throw new IllegalArgumentException("SDP UUID required");
                break;
            case U_INT_8: case U_INT_16: case INT_16:
                if(!(value instanceof byte[]) || ((byte[])value).length!=(valueType==U_INT_8?8:16))
                    throw new IllegalArgumentException("Invalid SDP integer bytes");
                value=copy((byte[])value);
                break;
            default: throw new IllegalArgumentException("Not an object SDP type");
        }
        type=valueType; this.value=value;
    }
    private static byte[] copy(byte[] source) {
        byte[] result=new byte[source.length];
        System.arraycopy(source,0,result,0,source.length);
        return result;
    }
    private Vector sequence() {
        if(type!=DATSEQ && type!=DATALT) throw new ClassCastException("Not an SDP sequence");
        return (Vector)value;
    }
    public void addElement(DataElement elem) {
        if(elem==null) throw new NullPointerException();
        sequence().addElement(elem);
    }
    public void insertElementAt(DataElement elem,int index) {
        if(elem==null) throw new NullPointerException();
        Vector elements=sequence();
        if(index<0 || index>elements.size()) throw new IndexOutOfBoundsException();
        elements.insertElementAt(elem,index);
    }
    public boolean removeElement(DataElement elem) {
        if(elem==null) throw new NullPointerException();
        return sequence().removeElement(elem);
    }
    public int getSize() { return sequence().size(); }
    public int getDataType() { return type; }
    public long getLong() {
        switch(type) {
            case U_INT_1: case U_INT_2: case U_INT_4:
            case INT_1: case INT_2: case INT_4: case INT_8: return number;
            default: throw new ClassCastException("Not a long SDP value");
        }
    }
    public boolean getBoolean() {
        if(type!=BOOL) throw new ClassCastException("Not an SDP boolean");
        return bool;
    }
    public Object getValue() {
        switch(type) {
            case STRING: case URL: return value;
            case UUID: return new javax.bluetooth.UUID(value.toString(),false);
            case U_INT_8: case U_INT_16: case INT_16: return copy((byte[])value);
            case DATSEQ: case DATALT: return sequence().elements();
            default: throw new ClassCastException("Not an object SDP value");
        }
    }
}
