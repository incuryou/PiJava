package com.samsung.util;
/** 0.2.107: Samsung SMS is always reported sent (SMSSKIP log line); nothing is transmitted. */
public final class SMS {
 private SMS(){}
 public static void send(SM message) throws java.io.IOException {
  if(message==null)throw new NullPointerException();
  String address=message.getDestAddress();
  String data=message.getData();
  if(address!=null&&!address.startsWith("sms://"))address="sms://"+address;
  javax.microedition.io.Connector.__vmSmsSkip("samsung",address,data==null?0:data.length(),false);
 }
}
