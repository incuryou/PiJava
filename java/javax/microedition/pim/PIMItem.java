/** Standard JSR-75 contact API contract; this runtime has no PIM backend. */
package javax.microedition.pim;

public interface PIMItem {
    int BINARY = 0;
    int BOOLEAN = 1;
    int DATE = 2;
    int INT = 3;
    int STRING = 4;
    int STRING_ARRAY = 5;
    int ATTR_NONE = 0;
    int EXTENDED_FIELD_MIN_VALUE = 16777216;
    int EXTENDED_ATTRIBUTE_MIN_VALUE = 16777216;

    PIMList getPIMList();

    void commit() throws PIMException;

    boolean isModified();

    int[] getFields();

    byte[] getBinary(int field, int index);

    void addBinary(int field, int attributes, byte[] value, int offset, int length);

    void setBinary(int field, int index, int attributes, byte[] value, int offset, int length);

    long getDate(int field, int index);

    void addDate(int field, int attributes, long value);

    void setDate(int field, int index, int attributes, long value);

    int getInt(int field, int index);

    void addInt(int field, int attributes, int value);

    void setInt(int field, int index, int attributes, int value);

    String getString(int field, int index);

    void addString(int field, int attributes, String value);

    void setString(int field, int index, int attributes, String value);

    boolean getBoolean(int field, int index);

    void addBoolean(int field, int attributes, boolean value);

    void setBoolean(int field, int index, int attributes, boolean value);

    String[] getStringArray(int field, int index);

    void addStringArray(int field, int attributes, String[] value);

    void setStringArray(int field, int index, int attributes, String[] value);

    int countValues(int field);

    void removeValue(int field, int index);

    int getAttributes(int field, int index);

    void addToCategory(String category) throws PIMException;

    void removeFromCategory(String category);

    String[] getCategories();

    int maxCategories();
}
