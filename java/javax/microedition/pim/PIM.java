package javax.microedition.pim;

import java.io.InputStream;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;

/** JSR-75 entry point. Access is unavailable; callers may use their own fallback. */
public abstract class PIM {
    public static final int CONTACT_LIST = 1;
    public static final int EVENT_LIST = 2;
    public static final int TODO_LIST = 3;
    public static final int READ_ONLY = 1;
    public static final int WRITE_ONLY = 2;
    public static final int READ_WRITE = 3;

    protected PIM() { }

    public static PIM getInstance() {
        throw new SecurityException("PIM access is unavailable");
    }

    public abstract PIMList openPIMList(int pimListType, int mode) throws PIMException;
    public abstract PIMList openPIMList(int pimListType, int mode, String name) throws PIMException;
    public abstract String[] listPIMLists(int pimListType);
    public abstract PIMItem[] fromSerialFormat(InputStream is, String enc)
            throws PIMException, UnsupportedEncodingException;
    public abstract void toSerialFormat(PIMItem item, OutputStream os, String enc, String dataFormat)
            throws PIMException, UnsupportedEncodingException;
    public abstract String[] supportedSerialFormats(int pimListType);
}
