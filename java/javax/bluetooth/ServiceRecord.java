package javax.bluetooth;
public interface ServiceRecord {
 int NOAUTHENTICATE_NOENCRYPT=0,AUTHENTICATE_NOENCRYPT=1,AUTHENTICATE_ENCRYPT=2;
 String getConnectionURL(int security,boolean master);
 RemoteDevice getHostDevice();
}
