package javax.bluetooth;
public class UUID {
 private String value;
 public UUID(long uuid){if(uuid<0||uuid>0xffffffffL)throw new IllegalArgumentException();value=hex32(uuid)+"00001000800000805f9b34fb";}
 public UUID(String uuid,boolean shortUUID){
  if(uuid==null)throw new NullPointerException();
  int n=uuid.length();if(n==0||n>(shortUUID?8:32))throw new IllegalArgumentException();
  uuid=uuid.toLowerCase();for(int i=0;i<n;i++){char c=uuid.charAt(i);if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))throw new IllegalArgumentException();}
  value=shortUUID?pad(uuid,8)+"00001000800000805f9b34fb":pad(uuid,32);
 }
 private static String hex32(long n){String s="";for(int i=0;i<8;i++){s="0123456789abcdef".charAt((int)(n&15))+s;n>>>=4;}return s;}
 private static String pad(String s,int n){while(s.length()<n)s="0"+s;return s;}
 public boolean equals(Object o){return o instanceof UUID&&value.equals(((UUID)o).value);}
 public int hashCode(){return value.hashCode();}
 public String toString(){int i=0;while(i+1<value.length()&&value.charAt(i)=='0')i++;return value.substring(i);}
}
