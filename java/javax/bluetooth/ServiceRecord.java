package javax.bluetooth;

/** JSR-82 service-record contract; no service records are fabricated by this runtime. */
public interface ServiceRecord {
    int NOAUTHENTICATE_NOENCRYPT=0, AUTHENTICATE_NOENCRYPT=1, AUTHENTICATE_ENCRYPT=2;
    DataElement getAttributeValue(int attrID);
    int[] getAttributeIDs();
    RemoteDevice getHostDevice();
    boolean populateRecord(int[] attrIDs) throws java.io.IOException;
    String getConnectionURL(int requiredSecurity,boolean mustBeMaster);
    void setDeviceServiceClasses(int classes);
    boolean setAttributeValue(int attrID,DataElement attrValue);
}
