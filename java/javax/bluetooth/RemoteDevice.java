package javax.bluetooth;
public class RemoteDevice {
 private String address;
 protected RemoteDevice(String a){if(a==null)throw new NullPointerException();if(a.length()!=12)throw new IllegalArgumentException();address=a;}
 public String getBluetoothAddress(){return address;}
 public String getFriendlyName(boolean ask) throws java.io.IOException {throw new java.io.IOException("No Bluetooth transport");}
 public boolean isTrustedDevice(){return false;}
 public boolean isAuthenticated(){return false;}
 public boolean isEncrypted(){return false;}
 public boolean authenticate() throws java.io.IOException {throw new java.io.IOException("No Bluetooth transport");}
}
