package java.lang;
public final class Class {
    public native String getName();
    public native java.io.InputStream getResourceAsStream(String name);
    public static native Class forName(String name) throws ClassNotFoundException;
    public native Object newInstance() throws InstantiationException, IllegalAccessException;
    public native boolean isInstance(Object obj);
    public native boolean isAssignableFrom(Class cls);
    public native boolean isArray();
    public native boolean isInterface();
}
