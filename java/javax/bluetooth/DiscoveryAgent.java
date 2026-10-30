package javax.bluetooth;
public class DiscoveryAgent {
 public static final int NOT_DISCOVERABLE=0,GIAC=0x9e8b33,LIAC=0x9e8b00,CACHED=0,PREKNOWN=1;
 public boolean cancelInquiry(DiscoveryListener listener){if(listener==null)throw new NullPointerException();return false;}
 public boolean cancelServiceSearch(int id){return false;}
 public boolean startInquiry(int access,DiscoveryListener listener) throws BluetoothStateException {if(listener==null)throw new NullPointerException();if(access<0x9e8b00||access>0x9e8b3f)throw new IllegalArgumentException();throw new BluetoothStateException("No Bluetooth transport");}
 public int searchServices(int[] attrs,UUID[] uuids,RemoteDevice device,DiscoveryListener listener) throws BluetoothStateException {if(uuids==null||device==null||listener==null)throw new NullPointerException();
  if(uuids.length==0)throw new IllegalArgumentException();
  for(int i=0;i<uuids.length;i++){
   if(uuids[i]==null)throw new NullPointerException();
   for(int j=0;j<i;j++)if(uuids[i].equals(uuids[j]))throw new IllegalArgumentException();
  }
  if(attrs!=null){
   if(attrs.length==0)throw new IllegalArgumentException();
   for(int i=0;i<attrs.length;i++){
    if(attrs[i]<0||attrs[i]>0xffff)throw new IllegalArgumentException();
    for(int j=0;j<i;j++)if(attrs[i]==attrs[j])throw new IllegalArgumentException();
   }
  }
  throw new BluetoothStateException("No Bluetooth transport");}
 public String selectService(UUID uuid,int security,boolean master) throws BluetoothStateException {if(uuid==null)throw new NullPointerException();if(security<0||security>2)throw new IllegalArgumentException();throw new BluetoothStateException("No Bluetooth transport");}
 public RemoteDevice[] retrieveDevices(int option){if(option!=CACHED&&option!=PREKNOWN)throw new IllegalArgumentException();return null;}
}
