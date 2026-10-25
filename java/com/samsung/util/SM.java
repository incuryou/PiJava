package com.samsung.util;
public class SM {
 private String data,address;
 public SM(){}
 public void setData(String value){if(value==null)throw new NullPointerException();data=value;}
 public String getData(){return data;}
 public void setDestAddress(String value){if(value==null)throw new NullPointerException();address=value;}
 public String getDestAddress(){return address;}
}
