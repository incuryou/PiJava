package javax.bluetooth;
/** API-linkage facade. No radio is advertised until a real transport exists. */
public class LocalDevice {
 private LocalDevice(){}
 public static LocalDevice getLocalDevice() throws BluetoothStateException {throw new BluetoothStateException("Bluetooth transport unavailable on this runtime");}
 public static boolean isPowerOn(){return false;}
 public static String getProperty(String property){if(property==null)throw new NullPointerException();return null;}
 public DiscoveryAgent getDiscoveryAgent(){throw new IllegalStateException("No Bluetooth transport");}
 public DeviceClass getDeviceClass(){throw new IllegalStateException("No Bluetooth transport");}
 public boolean setDiscoverable(int mode) throws BluetoothStateException {throw new BluetoothStateException("No Bluetooth transport");}
 public int getDiscoverable(){return DiscoveryAgent.NOT_DISCOVERABLE;}
 public String getFriendlyName(){return null;}
 public String getBluetoothAddress(){throw new IllegalStateException("No Bluetooth address");}
}
