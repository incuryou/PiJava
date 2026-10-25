package javax.wireless.messaging;
import java.util.Date;
public interface Message {
    String getAddress();
    void setAddress(String addr);
    Date getTimestamp();
}
