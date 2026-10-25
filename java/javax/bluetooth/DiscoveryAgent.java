package javax.bluetooth;
public class DiscoveryAgent {
 public static final int NOT_DISCOVERABLE=0,GIAC=0x9e8b33,LIAC=0x9e8b00,CACHED=0,PREKNOWN=1;
 public boolean cancelInquiry(DiscoveryListener listener){if(listener==null)throw new NullPointerException();return false;}
 public boolean cancelServiceSearch(int id){return false;}
 public boolean startInquiry(int access,DiscoveryListener listener) throws BluetoothStateException {if(listener==null)throw new NullPointerException();throw new BluetoothStateException("No Bluetooth transport");}
 public int searchServices(int[] attrs,UUID[] uuids,RemoteDevice device,DiscoveryListener listener) throws BluetoothStateException {if(uuids==null||device==null||listener==null)throw new NullPointerException();throw new BluetoothStateException("No Bluetooth transport");}
 public String selectService(UUID uuid,int security,boolean master) throws BluetoothStateException {if(uuid==null)throw new NullPointerException();throw new BluetoothStateException("No Bluetooth transport");}
 public RemoteDevice[] retrieveDevices(int option){if(option!=CACHED&&option!=PREKNOWN)throw new IllegalArgumentException();return null;}
}
