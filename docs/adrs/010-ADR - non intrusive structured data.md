### Architecture Decision Record (ADR)

#### **Title:** Support for Non-Intrusive Structured Data in `WordHeap`
- **Status**: Accepted
- **Date**:
- **Authors**:

### **Context**
The `WordHeap` class in the project allocates memory for named "words" and maintains metadata about each allocation. Currently, the memory is treated as raw bytes by default, as required by FORTH semantics. However, there is a growing need to support structured data (e.g., arrays, matrices, or multi-dimensional arrays) while **preserving compatibility** with FORTH conventions.
We need to enhance `WordHeap` to enable non-intrusive structured data support by associating additional metadata with each allocation. This metadata should help interpret raw memory as structured data (arrays, matrices, etc.), but it shouldn't interfere with FORTH's default "bytes as bytes" behavior.
### **Problem**
The current implementation of `WordHeap` lacks sufficient metadata to work efficiently with structured data. Operations like array indexing, slicing, or multi-dimensional interpretation require explicit calculations by the user, leading to repetitive and error-prone code. Additional functionality is needed to define "structured" memory (e.g., arrays or matrices), but:
1. **Compatibility** must be maintained with existing behavior, where all allocated memory is treated as raw bytes.
2. The design should ensure **flexibility**, allowing metadata use only when necessary, without imposing constraints on users who don't require structured data parsing.
3. The system should remain extensible to accommodate future use cases.

### **Decision**
We will extend the `WordAllocation` structure within `WordHeap` to include the following **optional metadata fields**, enabling non-intrusive structured data support while maintaining FORTH compatibility.
#### **Key Additions to `WordAllocation`**
1. **`element_size` **: The size of each element in the allocation (default: 1 byte).
2. **`element_columns` **: For structured data, specifies the number of columns (default: 1 for 1D arrays).
3. **`element_rows` **: For structured data, specifies the number of rows (default: 1 for 1D arrays).

These fields will help describe how the allocated memory is expected to be used (e.g., as an array or matrix). This metadata will remain optional, so existing FORTH-style memory usage won't be impacted.
#### **WordHeap Metadata Defaults**
Without structured data, the system will behave as follows:
- `element_size` defaults to 1 (memory accessed as raw bytes).
- `element_columns` defaults to 1 (1D layout).
- `element_rows` defaults to 1 (1D layout).

When metadata is explicitly set, it will enable array-aware operations (e.g., indexing).
### **Details**
#### Updated `WordAllocation` Structure
use
use
``` cpp
struct WordAllocation {
    void *dataPtr;              // Pointer to allocated memory
    size_t size;                // Total size of the allocation in bytes
    WordDataType dataType;      // (Optional) Type of the data (default: raw bytes)
    size_t element_size;        // Size of each element in the memory block (for arrays)
    size_t element_columns;     // Number of columns (for structured arrays)
    size_t element_rows;        // Number of rows (for structured arrays)

    // Constructor initializes default values for metadata fields
    WordAllocation() 
        : dataPtr(nullptr), size(0), dataType(WordDataType::DEFAULT),
          element_size(1), element_columns(1), element_rows(1) {}
};
```
#### Allocation Enhancements
The `allocate` function in `WordHeap` is enhanced to accept the following **optional parameters** at allocation time:
1. **`element_size` **: Size of each element.
2. **`element_columns` and `element_rows`**: Dimensions of the data, for use in arrays/matrices.

Default values ensure that users who do not need structured data can continue allocating memory without specifying these parameters.
#### New Metadata Use Cases
- Users can interpret memory as an array or matrix based on metadata.
- If metadata isn't needed, memory defaults to FORTH conventions of raw byte access.
- Metadata is managed internally in `WordHeap` but remains accessible for structured data operations.

### **Consequences**
#### Benefits
1. **Non-Intrusive Design**:
    - Backward compatibility: Words can continue to interact with memory as raw bytes, ensuring FORTH-like behavior is unaffected.
    - Metadata is optional and has no impact unless explicitly used.

2. **Improved Array Support**:
    - Element size and array dimensions mean easier indexing and slicing of multi-dimensional arrays or large data blocks.
    - Reduces error-prone manual calculations for projects working with arrays or structured data.

3. **Extensibility**:
    - The new metadata provides a natural path for further enhancements (e.g., row-major/column-major distinction, dynamic reshaping).
    - Support for additional data types or higher-dimensional arrays could be added later without further changes to `WordHeap`.

4. **Ease of Access**:
    - Metadata like `element_size` ensures that interpreting raw memory becomes straightforward for array users.
    - Built-in helpers (e.g., indexing or querying metadata fields) reduce user effort.

#### Drawbacks
1. **Increased Memory Overhead**:
    - Each `WordAllocation` now includes metadata fields (`element_size`, `element_columns`, and `element_rows`), slightly increasing memory usage.
    - However, since metadata is stored per allocation (not per element), this overhead is minimal.

2. **Initial User Complexity**:
    - Familiarization is required for users who want to leverage the benefits of structured memory.

#### Alternatives Considered
1. **External Array Metadata Map**:
    - Storing array-related metadata in a separate map (keyed by word name) was considered but rejected.
    - Reason: Separating metadata and memory increases lookup complexity and makes deallocation or querying more error-prone.

2. **Fixed Structure or Layout Enforcements**:
    - Enforcing strict layout conventions for arrays was not pursued to maintain the flexibility and raw byte handling of FORTH.

### **Code Examples**
#### 1. Allocation of Structured Data (2D Array Example)
``` cpp
WordHeap &heap = WordHeap::instance();

// Allocate a 3x4 array of floats (4 bytes per float)
heap.allocate("matrix", 3 * 4 * 4, WordDataType::FLOAT_ARRAY, 4, 4, 3);
```
#### 2. Accessing Metadata
``` cpp
auto *alloc = heap.getAllocation("matrix");
if (alloc) {
    std::cout << "Rows: " << alloc->element_rows 
              << ", Columns: " << alloc->element_columns
              << ", Element Size: " << alloc->element_size << " bytes" 
              << std::endl;
}
```
**Output:**
``` 
Rows: 3, Columns: 4, Element Size: 4 bytes
```
#### 3. Structured Access (Indexing)
``` cpp
size_t row = 1, col = 2; // Access element at row 1, column 2
WordAllocation *alloc = heap.getAllocation("matrix");
void *element = reinterpret_cast<void *>(
    reinterpret_cast<uintptr_t>(alloc->dataPtr) +
    row * alloc->element_columns * alloc->element_size +
    col * alloc->element_size
);
std::cout << "Element Address: " << element << std::endl;
```
### **Follow-up Tasks**
1. Add helper functions for structured operations (e.g., indexing helpers, reshape).
2. Test support for multi-dimensional data access across various operations.
3. Document metadata fields and their uses for developers.

### **Status: Accepted**
This design enables seamless integration of structured data support into `WordHeap` while preserving backward compatibility and alignment with FORTH semantics. The implementation ensures flexibility for future enhancements and reduces the burden on users needing structured memory access.
