/** Standard JSR-75 contact API contract; this runtime has no PIM backend. */
package javax.microedition.pim;

import java.util.Enumeration;

public interface PIMList {
    String UNCATEGORIZED = null;

    String getName();

    void close() throws PIMException;

    Enumeration items() throws PIMException;

    Enumeration items(PIMItem matchingItem) throws PIMException;

    Enumeration items(String matchingValue) throws PIMException;

    Enumeration itemsByCategory(String category) throws PIMException;

    String[] getCategories() throws PIMException;

    boolean isCategory(String category) throws PIMException;

    void addCategory(String category) throws PIMException;

    void deleteCategory(String category, boolean deleteUnassignedItems) throws PIMException;

    void renameCategory(String currentCategory, String newCategory) throws PIMException;

    int maxCategories();

    boolean isSupportedField(int field);

    int[] getSupportedFields();

    boolean isSupportedAttribute(int field, int attribute);

    int[] getSupportedAttributes(int field);

    boolean isSupportedArrayElement(int stringArrayField, int arrayElement);

    int[] getSupportedArrayElements(int stringArrayField);

    int getFieldDataType(int field);

    String getFieldLabel(int field);

    String getAttributeLabel(int attribute);

    String getArrayElementLabel(int stringArrayField, int arrayElement);

    int maxValues(int field);

    int stringArraySize(int stringArrayField);
}
