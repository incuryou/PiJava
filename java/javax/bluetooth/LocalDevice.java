package javax.bluetooth;
/** API-linkage facade. No radio is advertised until a real transport exists. */
public class LocalDevice {
 private LocalDevice(){}
 public static LocalDevice getLocalDevice() throws BluetoothStateException {throw new BluetoothStateException("Bluetooth transport unavailable on this runtime");}
 public static boolean isPowerOn(){return false;}
 public static String getProperty(String property){if(property==null)throw new NullPointerException();return null;}
 public DiscoveryAgent getDiscoveryAgent(){throw new IllegalStateException("No Bluetooth transport");}
 public DeviceClass getDeviceClass(){throw new IllegalStateException("No Bluetooth transport");}
 public boolean setDiscoverable(int mode) throws BluetoothStateException {if(mode!=DiscoveryAgent.NOT_DISCOVERABLE&&(mode<0x9e8b00||mode>0x9e8b3f))throw new IllegalArgumentException();throw new BluetoothStateException("No Bluetooth transport");}
 public int getDiscoverable(){return DiscoveryAgent.NOT_DISCOVERABLE;}
 public String getFriendlyName(){return null;}
 public String getBluetoothAddress(){throw new IllegalStateException("No Bluetooth address");}
 public ServiceRecord getRecord(javax.microedition.io.Connection notifier){
  if(notifier==null)throw new NullPointerException();
  // No runtime-owned Bluetooth notifier can exist without a transport.
  throw new IllegalArgumentException("Not a Bluetooth notifier owned by this runtime");
 }
 public void updateRecord(ServiceRecord record) throws ServiceRegistrationException {
  if(record==null)throw new NullPointerException();
  throw new IllegalArgumentException("Not a local Bluetooth service record");
 }
}
