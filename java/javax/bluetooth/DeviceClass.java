package javax.bluetooth;
public class DeviceClass {
 private int record;
 public DeviceClass(int value){if((value&0xff000000)!=0)throw new IllegalArgumentException();record=value;}
 public int getMajorDeviceClass(){return record&0x1f00;}
 public int getMinorDeviceClass(){return record&0xfc;}
 public int getServiceClasses(){return record&0xffe000;}
}
