package javax.microedition.pki;

/** MIDP certificate metadata contract; does not create synthetic certificates. */
public interface Certificate {
    String getSubject();
    String getIssuer();
    String getType();
    String getVersion();
    String getSigAlgName();
    long getNotBefore();
    long getNotAfter();
    String getSerialNumber();
}
