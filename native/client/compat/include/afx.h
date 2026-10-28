// Native replacement for the MFC core header <afx.h>: CString (the MBCS
// CStringA of the original build), CFile, CArchive, the MFC exception classes
// and CPoint, CSize, CRect. Only the members that the client sources use are
// provided. Differences from MFC are listed in native/client/compat/README.md.
#pragma once
#include <dirent.h>
#include <fnmatch.h>
#include <vector>
#include "windows.h"
#include <fcntl.h>

// Original headers test _AFX to select their CString overloads (Packet.h).
#ifndef _AFX
#define _AFX
#endif

// ---- Diagnostics --------------------------------------------------------------
// As in an MFC release build: TRACE and ASSERT produce no code, VERIFY keeps
// its expression.
#define TRACE(...) ((void)0)
#define TRACE0(...) ((void)0)
#define TRACE1(...) ((void)0)
#define TRACE2(...) ((void)0)
#define TRACE3(...) ((void)0)
#define ASSERT(expr) ((void)0)
#define ASSERT_VALID(expr) ((void)0)
#define _ASSERT(expr) ((void)0)
#define _ASSERTE(expr) ((void)0)
#define VERIFY(expr) ((void)(expr))
#define DEBUG_NEW new
#define AFX_INLINE inline
#define afx_msg

// ---- Exceptions ---------------------------------------------------------------
// MFC throws pointers ("throw new CFileException") and the handler calls
// Delete(). TRY and CATCH map to try and catch of the pointer type.
class CException {
public:
    virtual ~CException() {}
    void Delete() { delete this; }
    virtual BOOL GetErrorMessage(LPTSTR text, UINT size, UINT* context = nullptr) {
        (void)context; if (text && size) text[0] = 0; return FALSE;
    }
};
class CMemoryException : public CException {};
class CNotSupportedException : public CException {};
inline void AfxThrowMemoryException() { throw new CMemoryException; }
inline void AfxThrowNotSupportedException() { throw new CNotSupportedException; }
// The debug heap of the Microsoft CRT does not exist natively; the flag set is a no-op (returns 0).
inline int _CrtSetDbgFlag(int) { return 0; }
#define TRY try {
#define CATCH(type, name) } catch (type* name) { (void)name;
#define AND_CATCH(type, name) } catch (type* name) { (void)name;
#define CATCH_ALL(name) } catch (CException* name) { (void)name;
#define AND_CATCH_ALL(name) } catch (CException* name) { (void)name;
#define END_CATCH }
#define END_CATCH_ALL }
#define THROW(pointer) throw pointer
#define THROW_LAST() throw

// ---- CString -------------------------------------------------------------------
// One pointer wide, as in MFC: the object holds only a pointer to its
// zero-terminated text. The length and capacity are stored in front of the
// text. Each object owns its buffer (no reference counting). The text is a
// sequence of bytes in the code page of the game data; no conversion is made.
// Searching, trimming and case mapping work on single bytes and map the case
// of ASCII letters only (README, "CString").
class CString {
    struct Header { int length; int capacity; };
    char* text_;

    // The shared empty buffer has capacity -1. The test does not compare
    // addresses: each dynamic library has its own copy of this object.
    static char* nil() { static struct { Header header; char text[1]; } empty = {{0, -1}, {0}}; return empty.text; }
    static Header* header(char* text) { return reinterpret_cast<Header*>(text) - 1; }
    bool owned() const { return header(text_)->capacity >= 0; }
    // At least 7 characters, in steps of 8 bytes, as the MFC allocator does.
    static char* allocate(int capacity) {
        if (capacity < 0 || capacity > 0x7FFFFFF0) AfxThrowMemoryException();
        capacity = (capacity | 7);
        Header* block = static_cast<Header*>(std::malloc(sizeof(Header) + static_cast<std::size_t>(capacity) + 1));
        if (!block) AfxThrowMemoryException();
        block->length = 0; block->capacity = capacity;
        char* text = reinterpret_cast<char*>(block + 1); text[0] = 0;
        return text;
    }
    void release() { if (owned()) std::free(header(text_)); text_ = nil(); }
    void reserve(int capacity) {
        if (owned() && header(text_)->capacity >= capacity) return;
        const int old = GetLength();
        int grown = owned() && header(text_)->capacity < 0x3FFFFFFF ? header(text_)->capacity * 2 : 0;
        if (grown < capacity) grown = capacity;
        char* text = allocate(grown);
        std::memcpy(text, text_, static_cast<std::size_t>(old) + 1);
        header(text)->length = old;
        release(); text_ = text;
    }
    void set_length(int length) { if (owned()) { header(text_)->length = length; text_[length] = 0; } }
    void assign(const char* from, int length) {
        if (length <= 0) { Empty(); return; }
        if (from >= text_ && from <= text_ + GetLength()) { // part of this string
            std::memmove(text_, from, static_cast<std::size_t>(length)); set_length(length); return;
        }
        reserve(length);
        std::memcpy(text_, from, static_cast<std::size_t>(length)); set_length(length);
    }
    void append(const char* from, int length) {
        if (length <= 0) return;
        const int old = GetLength();
        if (from >= text_ && from <= text_ + old) { // part of this string
            const std::ptrdiff_t offset = from - text_;
            reserve(old + length); from = text_ + offset;
        } else reserve(old + length);
        std::memmove(text_ + old, from, static_cast<std::size_t>(length)); set_length(old + length);
    }
    static int text_length(const char* text) { return text ? static_cast<int>(std::strlen(text)) : 0; }
    static bool is_space(char value) { return value == ' ' || (value >= '\t' && value <= '\r'); }

public:
    CString() : text_(nil()) {}
    CString(const CString& other) : text_(nil()) { assign(other.text_, other.GetLength()); }
    CString(const char* text) : text_(nil()) { assign(text, text_length(text)); }
    CString(const char* text, int length) : text_(nil()) { assign(text, text ? length : 0); }
    // explicit, as in MFC with _ATL_CSTRING_EXPLICIT_CONSTRUCTORS (Client and TChart
    // StdAfx.h): "CString s = 0;" then selects the const char* form (TNetSender.cpp:16).
    explicit CString(const unsigned char* text) : text_(nil()) { assign(reinterpret_cast<const char*>(text), text_length(reinterpret_cast<const char*>(text))); }
    explicit CString(char value, int repeat = 1) : text_(nil()) {
        if (repeat > 0) { reserve(repeat); std::memset(text_, value, static_cast<std::size_t>(repeat)); set_length(repeat); }
    }
    CString(const std::string& text) : text_(nil()) { assign(text.data(), static_cast<int>(text.size())); }
    ~CString() { if (owned()) std::free(header(text_)); }

    CString& operator=(const CString& other) { if (this != &other) assign(other.text_, other.GetLength()); return *this; }
    CString& operator=(const char* text) { assign(text, text_length(text)); return *this; }
    CString& operator=(char value) { assign(&value, 1); return *this; }
    CString& operator+=(const CString& other) { append(other.text_, other.GetLength()); return *this; }
    CString& operator+=(const char* text) { append(text, text_length(text)); return *this; }
    CString& operator+=(char value) { append(&value, 1); return *this; }

    operator const char*() const { return text_; }
    const char* GetString() const { return text_; }
    int GetLength() const { return header(text_)->length; }
    BOOL IsEmpty() const { return GetLength() == 0; }
    void Empty() { release(); }
    char GetAt(int index) const { return text_[index]; }
    // A template, so that every integer index type selects this operator and
    // not the built-in one through the conversion to const char*.
    template<class Index> char operator[](Index index) const { return text_[index]; }
    void SetAt(int index, char value) { if (index >= 0 && index < GetLength()) text_[index] = value; }

    // GetBuffer gives write access to at least `minimum` bytes plus the
    // terminator. ReleaseBuffer(-1) takes the length from the terminator.
    char* GetBuffer(int minimum = 0) { reserve(minimum > GetLength() ? minimum : GetLength()); return text_; }
    char* GetBufferSetLength(int length) { reserve(length); set_length(length); return text_; }
    void ReleaseBuffer(int length = -1) {
        if (!owned()) return;
        if (length < 0) length = static_cast<int>(strnlen(text_, static_cast<std::size_t>(header(text_)->capacity)));
        if (length > header(text_)->capacity) length = header(text_)->capacity;
        set_length(length);
    }
    void Preallocate(int capacity) { reserve(capacity); }
    void FreeExtra() {}

    CString& Append(const char* text) { append(text, text_length(text)); return *this; }
    CString& Append(const char* text, int length) { append(text, length); return *this; }
    CString& AppendChar(char value) { append(&value, 1); return *this; }

    void FormatV(const char* format, va_list args) {
        va_list copy; va_copy(copy, args);
        const int length = fourstory::compat::vsnprintf_args(nullptr, 0, format, copy); va_end(copy);
        if (length <= 0) { Empty(); return; }
        CString result; result.reserve(length);
        fourstory::compat::vsnprintf_args(result.text_, static_cast<std::size_t>(length) + 1, format, args);
        result.set_length(length);
        swap(result);
    }
    void AppendFormatV(const char* format, va_list args) { CString part; part.FormatV(format, args); *this += part; }
    template<class... Args> void Format(const char* format, const Args&... args) {
        const int length = fourstory::compat::snprintf_args(nullptr, 0, format, args...);
        if (length <= 0) { Empty(); return; }
        CString result; result.reserve(length); // the arguments may refer to this string
        fourstory::compat::snprintf_args(result.text_, static_cast<std::size_t>(length) + 1, format, args...);
        result.set_length(length);
        swap(result);
    }
    template<class... Args> void AppendFormat(const char* format, const Args&... args) {
        CString part; part.Format(format, args...); *this += part;
    }
    void swap(CString& other) { char* text = text_; text_ = other.text_; other.text_ = text; }

    int Compare(const char* other) const { return std::strcmp(text_, other ? other : ""); }
    int CompareNoCase(const char* other) const { return strcasecmp(text_, other ? other : ""); }
    int Collate(const char* other) const { return Compare(other); }
    int CollateNoCase(const char* other) const { return CompareNoCase(other); }

    CString Mid(int first) const { return Mid(first, GetLength()); }
    CString Mid(int first, int count) const {
        const int length = GetLength();
        if (first < 0) first = 0;
        if (count < 0) count = 0;
        if (first > length) first = length;
        if (count > length - first) count = length - first;
        return CString(text_ + first, count);
    }
    CString Left(int count) const { return Mid(0, count); }
    CString Right(int count) const {
        const int length = GetLength();
        if (count < 0) count = 0;
        if (count > length) count = length;
        return CString(text_ + length - count, count);
    }
    CString SpanIncluding(const char* set) const { return Left(static_cast<int>(std::strspn(text_, set))); }
    CString SpanExcluding(const char* set) const { return Left(static_cast<int>(std::strcspn(text_, set))); }

    int Find(char value, int start = 0) const {
        if (start < 0 || start >= GetLength()) return -1;
        const char* found = static_cast<const char*>(std::memchr(text_ + start, value, static_cast<std::size_t>(GetLength() - start)));
        return found ? static_cast<int>(found - text_) : -1;
    }
    int Find(const char* part, int start = 0) const {
        if (!part || start < 0 || start > GetLength()) return -1;
        const char* found = std::strstr(text_ + start, part);
        return found ? static_cast<int>(found - text_) : -1;
    }
    int ReverseFind(char value) const {
        for (int index = GetLength() - 1; index >= 0; --index) if (text_[index] == value) return index;
        return -1;
    }
    int FindOneOf(const char* set) const {
        const std::size_t index = std::strcspn(text_, set ? set : "");
        return index < static_cast<std::size_t>(GetLength()) ? static_cast<int>(index) : -1;
    }

    CString& MakeUpper() { for (int i = 0, n = GetLength(); i < n; ++i) if (text_[i] >= 'a' && text_[i] <= 'z') text_[i] = static_cast<char>(text_[i] - 32); return *this; }
    CString& MakeLower() { for (int i = 0, n = GetLength(); i < n; ++i) if (text_[i] >= 'A' && text_[i] <= 'Z') text_[i] = static_cast<char>(text_[i] + 32); return *this; }
    CString& MakeReverse() { for (int i = 0, j = GetLength() - 1; i < j; ++i, --j) { char c = text_[i]; text_[i] = text_[j]; text_[j] = c; } return *this; }

    CString& TrimRight() { int n = GetLength(); while (n > 0 && is_space(text_[n - 1])) --n; set_length(n); return *this; }
    CString& TrimLeft() { int n = 0; while (is_space(text_[n])) ++n; return Delete(0, n), *this; }
    CString& Trim() { return TrimRight().TrimLeft(); }
    CString& TrimRight(char value) { int n = GetLength(); while (n > 0 && text_[n - 1] == value) --n; set_length(n); return *this; }
    CString& TrimLeft(char value) { int n = 0; while (text_[n] && text_[n] == value) ++n; return Delete(0, n), *this; }
    CString& Trim(char value) { return TrimRight(value).TrimLeft(value); }
    CString& TrimRight(const char* set) { int n = GetLength(); while (n > 0 && set && std::strchr(set, text_[n - 1])) --n; set_length(n); return *this; }
    CString& TrimLeft(const char* set) { int n = 0; while (text_[n] && set && std::strchr(set, text_[n])) ++n; return Delete(0, n), *this; }
    CString& Trim(const char* set) { return TrimRight(set).TrimLeft(set); }

    // Returns the new length.
    int Delete(int index, int count = 1) {
        const int length = GetLength();
        if (index < 0) index = 0;
        if (count <= 0 || index >= length) return length;
        if (count > length - index) count = length - index;
        std::memmove(text_ + index, text_ + index + count, static_cast<std::size_t>(length - index - count));
        set_length(length - count);
        return length - count;
    }
    int Insert(int index, const char* part) {
        const int count = text_length(part), length = GetLength();
        if (index < 0) index = 0;
        if (index > length) index = length;
        if (!count) return length;
        const CString copy(part, count); // part may refer to this string
        reserve(length + count);
        std::memmove(text_ + index + count, text_ + index, static_cast<std::size_t>(length - index));
        std::memcpy(text_ + index, copy.text_, static_cast<std::size_t>(count));
        set_length(length + count);
        return length + count;
    }
    int Insert(int index, char value) { const char part[2] = {value, 0}; return value ? Insert(index, part) : GetLength(); }
    // Returns the number of removed characters.
    int Remove(char value) {
        const int length = GetLength(); int kept = 0;
        for (int i = 0; i < length; ++i) if (text_[i] != value) text_[kept++] = text_[i];
        set_length(kept);
        return length - kept;
    }
    // Returns the number of replacements.
    int Replace(char from, char to) {
        int count = 0;
        if (from == to) return 0;
        for (int i = 0, n = GetLength(); i < n; ++i) if (text_[i] == from) { text_[i] = to; ++count; }
        return count;
    }
    int Replace(const char* from, const char* to) {
        const int from_length = text_length(from), to_length = text_length(to);
        if (!from_length) return 0;
        int count = 0;
        CString result;
        const char* cursor = text_;
        const char* end = text_ + GetLength();
        while (cursor < end) { // MFC searches each zero-terminated section
            const char* found = std::strstr(cursor, from);
            if (!found) {
                const int section = static_cast<int>(std::strlen(cursor));
                result.append(cursor, section); cursor += section;
                if (cursor < end) { result.AppendChar(0); ++cursor; }
                continue;
            }
            result.append(cursor, static_cast<int>(found - cursor));
            result.append(to, to_length);
            cursor = found + from_length; ++count;
        }
        if (count) swap(result);
        return count;
    }
    // Returns the next token and moves `start`; `start` is -1 after the last one.
    // String resources (C4-P): defined in compat/src/mfc.cpp (libcompat_ui).
    BOOL LoadString(UINT id);
    CString Tokenize(const char* delimiters, int& start) const {
        if (start < 0) return CString();
        if (!delimiters || !*delimiters) {
            if (start < GetLength()) { const int first = start; start = -1; return Mid(first); }
        } else if (start < GetLength()) {
            const char* begin = text_ + start;
            const int skip = static_cast<int>(std::strspn(begin, delimiters));
            if (start + skip < GetLength()) {
                const int count = static_cast<int>(std::strcspn(begin + skip, delimiters));
                const int first = start + skip;
                start = first + count + 1;
                return Mid(first, count);
            }
        }
        start = -1;
        return CString();
    }

    friend CString operator+(const CString& a, const CString& b) { CString r(a); r += b; return r; }
    friend CString operator+(const CString& a, const char* b) { CString r(a); r += b; return r; }
    friend CString operator+(const char* a, const CString& b) { CString r(a); r += b; return r; }
    friend CString operator+(const CString& a, char b) { CString r(a); r += b; return r; }
    friend CString operator+(char a, const CString& b) { CString r(a, 1); r += b; return r; }
    friend bool operator==(const CString& a, const CString& b) { return a.Compare(b.text_) == 0; }
    friend bool operator==(const CString& a, const char* b) { return a.Compare(b) == 0; }
    friend bool operator==(const char* a, const CString& b) { return b.Compare(a) == 0; }
    friend bool operator!=(const CString& a, const CString& b) { return a.Compare(b.text_) != 0; }
    friend bool operator!=(const CString& a, const char* b) { return a.Compare(b) != 0; }
    friend bool operator!=(const char* a, const CString& b) { return b.Compare(a) != 0; }
    friend bool operator<(const CString& a, const CString& b) { return a.Compare(b.text_) < 0; }
    friend bool operator<(const CString& a, const char* b) { return a.Compare(b) < 0; }
    friend bool operator<(const char* a, const CString& b) { return b.Compare(a) > 0; }
    friend bool operator>(const CString& a, const CString& b) { return a.Compare(b.text_) > 0; }
    friend bool operator>(const CString& a, const char* b) { return a.Compare(b) > 0; }
    friend bool operator>(const char* a, const CString& b) { return b.Compare(a) < 0; }
    friend bool operator<=(const CString& a, const CString& b) { return a.Compare(b.text_) <= 0; }
    friend bool operator<=(const CString& a, const char* b) { return a.Compare(b) <= 0; }
    friend bool operator<=(const char* a, const CString& b) { return b.Compare(a) >= 0; }
    friend bool operator>=(const CString& a, const CString& b) { return a.Compare(b.text_) >= 0; }
    friend bool operator>=(const CString& a, const char* b) { return a.Compare(b) >= 0; }
    friend bool operator>=(const char* a, const CString& b) { return b.Compare(a) <= 0; }
};
static_assert(sizeof(CString) == sizeof(char*), "CString must be one pointer wide");
typedef CString CStringA;
inline const char* fourstory_compat_cstr(const CString& text) { return text.GetString(); }

// ---- CObject -----------------------------------------------------------------
// The MFC root class with run-time class information (DECLARE_DYNAMIC,
// IMPLEMENT_DYNAMIC, RUNTIME_CLASS, IsKindOf; C4-P). Dynamic creation and
// CObject serialization are not provided.
// ---- File exceptions (after CString: MFC's CFileException holds the file name) ----
class CFileException : public CException {
public:
    enum { none, genericException, fileNotFound, badPath, tooManyOpenFiles, accessDenied, invalidFile,
           removeCurrentDir, directoryFull, badSeek, hardIO, sharingViolation, lockViolation, diskFull, endOfFile };
    int m_cause;
    LONG m_lOsError;
    CString m_strFileName;
    explicit CFileException(int cause = none, LONG os_error = -1, LPCTSTR file_name = nullptr) : m_cause(cause), m_lOsError(os_error), m_strFileName(file_name ? file_name : "") {}
    static int ErrnoToException(int error) {
        switch (error) {
        case 0: return none;
        case ENOENT: return fileNotFound;
        case ENOTDIR: case ENAMETOOLONG: return badPath;
        case EMFILE: case ENFILE: return tooManyOpenFiles;
        case EACCES: case EPERM: case EROFS: return accessDenied;
        case EBADF: return invalidFile;
        case ENOSPC: return diskFull;
        case EIO: return hardIO;
        default: return genericException;
        }
    }
};
class CArchiveException : public CException {
public:
    enum { none, genericException, readOnly, endOfFile, writeOnly, badIndex, badClass, badSchema, bufferFull };
    int m_cause;
    explicit CArchiveException(int cause = none) : m_cause(cause) {}
};
inline void AfxThrowFileException(int cause, LONG os_error = -1, LPCTSTR file_name = nullptr) { throw new CFileException(cause, os_error, file_name); }
inline void AfxThrowArchiveException(int cause) { throw new CArchiveException(cause); }
class CArchive;
class CObject;
struct CRuntimeClass {
    const char* m_lpszClassName;
    int m_nObjectSize;
    const CRuntimeClass* m_pBaseClass;
    BOOL IsDerivedFrom(const CRuntimeClass* base) const {
        for (const CRuntimeClass* c = this; c; c = c->m_pBaseClass) if (c == base) return TRUE;
        return FALSE;
    }
};
#define RUNTIME_CLASS(class_name) (&class_name::class##class_name)
#define DECLARE_DYNAMIC(class_name) \
public: static const CRuntimeClass class##class_name; virtual const CRuntimeClass* GetRuntimeClass() const;
#define DECLARE_DYNCREATE(class_name) DECLARE_DYNAMIC(class_name)
#define IMPLEMENT_DYNAMIC(class_name, base_class_name) \
const CRuntimeClass class_name::class##class_name = { #class_name, sizeof(class_name), RUNTIME_CLASS(base_class_name) }; \
const CRuntimeClass* class_name::GetRuntimeClass() const { return RUNTIME_CLASS(class_name); }
#define IMPLEMENT_DYNCREATE(class_name, base_class_name) IMPLEMENT_DYNAMIC(class_name, base_class_name)
// Header-only definition of CObject::classCObject (C++14 has no inline variables).
template<int N = 0> struct fourstory_cobject_runtime { static const CRuntimeClass classCObject; };
template<int N> const CRuntimeClass fourstory_cobject_runtime<N>::classCObject = { "CObject", 0, nullptr };
class CObject : public fourstory_cobject_runtime<> {
public:
    virtual const CRuntimeClass* GetRuntimeClass() const { return &classCObject; }
    BOOL IsKindOf(const CRuntimeClass* c) const { return GetRuntimeClass()->IsDerivedFrom(c); }
    virtual ~CObject() {}
    virtual void Serialize(CArchive&) {}
    BOOL IsSerializable() const { return FALSE; }
protected:
    CObject() {}
private:
    CObject(const CObject&);
    CObject& operator=(const CObject&);
};

// ---- CFile -------------------------------------------------------------------------
// Binary file over a POSIX descriptor. Sharing flags are accepted and ignored.
class CFile {
public:
    enum OpenFlags {
        modeRead = 0x0000, modeWrite = 0x0001, modeReadWrite = 0x0002,
        shareCompat = 0x0000, shareExclusive = 0x0010, shareDenyWrite = 0x0020, shareDenyRead = 0x0030, shareDenyNone = 0x0040,
        modeNoInherit = 0x0080, modeCreate = 0x1000, modeNoTruncate = 0x2000,
        typeText = 0x4000, typeBinary = 0x8000
    };
    enum SeekPosition { begin = 0x0, current = 0x1, end = 0x2 };
    static HANDLE hFileNull() { return INVALID_HANDLE_VALUE; }
    HANDLE m_hFile;

    CFile() : m_hFile(INVALID_HANDLE_VALUE), descriptor_(-1) {}
    // Throws CFileException* when the file cannot be opened, as MFC does.
    CFile(LPCTSTR path, UINT flags) : m_hFile(INVALID_HANDLE_VALUE), descriptor_(-1) {
        CFileException error;
        if (!Open(path, flags, &error)) AfxThrowFileException(error.m_cause, error.m_lOsError, path);
    }
    virtual ~CFile() { if (descriptor_ >= 0) ::close(descriptor_); }

    virtual BOOL Open(LPCTSTR path, UINT flags, CFileException* error = nullptr) {
        if (descriptor_ >= 0) { ::close(descriptor_); descriptor_ = -1; m_hFile = INVALID_HANDLE_VALUE; }
        int mode = (flags & 3) == modeWrite ? O_WRONLY : (flags & 3) == modeReadWrite ? O_RDWR : O_RDONLY;
        if (flags & modeCreate) mode |= O_CREAT | ((flags & modeNoTruncate) ? 0 : O_TRUNC);
        path_ = path ? path : "";
        descriptor_ = ::open(fourstory::compat::native_path(path).c_str(), mode | O_CLOEXEC, 0644);
        if (descriptor_ < 0) {
            if (error) { error->m_cause = CFileException::ErrnoToException(errno); error->m_lOsError = errno; error->m_strFileName = path; }
            return FALSE;
        }
        m_hFile = reinterpret_cast<HANDLE>(static_cast<std::intptr_t>(descriptor_));
        return TRUE;
    }
    // Returns the number of bytes read; less than `count` only at the end of the file.
    virtual UINT Read(void* buffer, UINT count) {
        UINT total = 0;
        while (total < count) {
            const ssize_t got = ::read(descriptor_, static_cast<char*>(buffer) + total, count - total);
            if (got < 0) { if (errno == EINTR) continue; AfxThrowFileException(CFileException::ErrnoToException(errno), errno, path_.c_str()); }
            if (got == 0) break;
            total += static_cast<UINT>(got);
        }
        return total;
    }
    virtual void Write(const void* buffer, UINT count) {
        UINT total = 0;
        while (total < count) {
            const ssize_t put = ::write(descriptor_, static_cast<const char*>(buffer) + total, count - total);
            if (put < 0) { if (errno == EINTR) continue; AfxThrowFileException(CFileException::ErrnoToException(errno), errno, path_.c_str()); }
            total += static_cast<UINT>(put);
        }
    }
    virtual ULONGLONG Seek(LONGLONG offset, UINT from) {
        const off_t position = ::lseek(descriptor_, static_cast<off_t>(offset), from == begin ? SEEK_SET : from == current ? SEEK_CUR : SEEK_END);
        if (position < 0) AfxThrowFileException(CFileException::badSeek, errno, path_.c_str());
        return static_cast<ULONGLONG>(position);
    }
    void SeekToBegin() { Seek(0, begin); }
    ULONGLONG SeekToEnd() { return Seek(0, end); }
    virtual ULONGLONG GetPosition() const {
        const off_t position = ::lseek(descriptor_, 0, SEEK_CUR);
        if (position < 0) AfxThrowFileException(CFileException::badSeek, errno, path_.c_str());
        return static_cast<ULONGLONG>(position);
    }
    virtual ULONGLONG GetLength() const {
        struct stat info;
        if (::fstat(descriptor_, &info) != 0) AfxThrowFileException(CFileException::ErrnoToException(errno), errno, path_.c_str());
        return static_cast<ULONGLONG>(info.st_size);
    }
    virtual void Flush() {}
    virtual void Close() {
        const int descriptor = descriptor_;
        descriptor_ = -1; m_hFile = INVALID_HANDLE_VALUE;
        if (descriptor >= 0 && ::close(descriptor) != 0) AfxThrowFileException(CFileException::ErrnoToException(errno), errno, path_.c_str());
    }
    virtual void Abort() { if (descriptor_ >= 0) ::close(descriptor_); descriptor_ = -1; m_hFile = INVALID_HANDLE_VALUE; }
    CString GetFilePath() const { return CString(path_); }
    CString GetFileName() const {
        const std::size_t slash = path_.find_last_of("\\/");
        return CString(slash == std::string::npos ? path_ : path_.substr(slash + 1));
    }

private:
    CFile(const CFile&);
    CFile& operator=(const CFile&);
    int descriptor_;
    std::string path_;
};

// ---- CStdioFile --------------------------------------------------------------
// Line-oriented text file. With typeText (the MFC default for this class)
// WriteString writes "\n" as CR LF and ReadString removes the CR before LF,
// as the Microsoft CRT text mode does. With typeBinary the bytes are kept.
// Ctrl-Z is not treated as the end of a text file.
class CStdioFile : public CFile {
public:
    CStdioFile() : text_(true) {}
    CStdioFile(LPCTSTR path, UINT flags) : CFile(), text_(!(flags & typeBinary)) {
        CFileException error;
        if (!Open(path, flags, &error)) AfxThrowFileException(error.m_cause, error.m_lOsError, path);
    }
    BOOL Open(LPCTSTR path, UINT flags, CFileException* error = nullptr) override {
        text_ = !(flags & typeBinary);
        pending_.clear(); end_ = false;
        return CFile::Open(path, flags, error);
    }
    void WriteString(LPCTSTR text) {
        if (!text) return;
        if (!text_) { Write(text, static_cast<UINT>(std::strlen(text))); return; }
        std::string out;
        for (const char* p = text; *p; ++p) { if (*p == '\n') out += '\r'; out += *p; }
        Write(out.data(), static_cast<UINT>(out.size()));
    }
    // Reads one line without its line end. FALSE at the end of the file when
    // nothing was read.
    BOOL ReadString(CString& line) {
        std::string text;
        bool any = false;
        for (;;) {
            if (pending_.empty()) {
                if (end_) break;
                char buffer[4096];
                const UINT got = Read(buffer, sizeof(buffer));
                if (!got) { end_ = true; break; }
                pending_.append(buffer, got);
            }
            const std::size_t newline = pending_.find('\n');
            any = true;
            if (newline == std::string::npos) { text += pending_; pending_.clear(); continue; }
            text.append(pending_, 0, newline); pending_.erase(0, newline + 1);
            if (text_ && !text.empty() && text.back() == '\r') text.pop_back();
            line = CString(text);
            return TRUE;
        }
        if (text_ && !text.empty() && text.back() == '\r') text.pop_back();
        line = CString(text);
        return any ? TRUE : FALSE;
    }
    // The read buffer is given back before any other position-dependent call.
    ULONGLONG Seek(LONGLONG offset, UINT from) override { sync(); return CFile::Seek(offset, from); }
    ULONGLONG GetPosition() const override { return CFile::GetPosition() - pending_.size(); }
    void Write(const void* buffer, UINT count) override { sync(); CFile::Write(buffer, count); }
    void Close() override { pending_.clear(); end_ = false; CFile::Close(); }
    void Abort() override { pending_.clear(); end_ = false; CFile::Abort(); }
private:
    void sync() {
        if (!pending_.empty()) { const LONGLONG back = -static_cast<LONGLONG>(pending_.size()); pending_.clear(); CFile::Seek(back, current); }
        end_ = false;
    }
    bool text_;
    bool end_ = false;
    std::string pending_;
};

// ---- CTime ---------------------------------------------------------------------
// Seconds since 1970 in local time, as MFC's CTime. Only the members below.
class CTimeSpan {
public:
    CTimeSpan(__time64_t seconds = 0) : seconds_(seconds) {}
    CTimeSpan(LONG days, int hours, int minutes, int seconds) : seconds_(((static_cast<__time64_t>(days) * 24 + hours) * 60 + minutes) * 60 + seconds) {}
    __time64_t GetTimeSpan() const { return seconds_; }
    LONGLONG GetDays() const { return seconds_ / 86400; }
    LONGLONG GetTotalHours() const { return seconds_ / 3600; }
    LONGLONG GetTotalMinutes() const { return seconds_ / 60; }
    LONGLONG GetTotalSeconds() const { return seconds_; }
    LONG GetHours() const { return static_cast<LONG>(GetTotalHours() - GetDays() * 24); }
    LONG GetMinutes() const { return static_cast<LONG>(GetTotalMinutes() - GetTotalHours() * 60); }
    LONG GetSeconds() const { return static_cast<LONG>(GetTotalSeconds() - GetTotalMinutes() * 60); }
private:
    __time64_t seconds_;
};
class CTime {
public:
    CTime() : time_(0) {}
    CTime(__time64_t time) : time_(time) {}
    CTime(int year, int month, int day, int hour, int minute, int second) {
        std::tm parts{};
        parts.tm_year = year - 1900; parts.tm_mon = month - 1; parts.tm_mday = day;
        parts.tm_hour = hour; parts.tm_min = minute; parts.tm_sec = second; parts.tm_isdst = -1;
        time_ = static_cast<__time64_t>(std::mktime(&parts));
    }
    static CTime GetCurrentTime() { return CTime(static_cast<__time64_t>(std::time(nullptr))); }
    __time64_t GetTime() const { return time_; }
    int GetYear() const { return local().tm_year + 1900; }
    int GetMonth() const { return local().tm_mon + 1; }
    int GetDay() const { return local().tm_mday; }
    int GetHour() const { return local().tm_hour; }
    int GetMinute() const { return local().tm_min; }
    int GetSecond() const { return local().tm_sec; }
    int GetDayOfWeek() const { return local().tm_wday + 1; } // 1 = Sunday
    CTimeSpan operator-(const CTime& other) const { return CTimeSpan(time_ - other.time_); }
    CTime operator-(const CTimeSpan& span) const { return CTime(time_ - span.GetTimeSpan()); }
    CTime operator+(const CTimeSpan& span) const { return CTime(time_ + span.GetTimeSpan()); }
    bool operator==(const CTime& other) const { return time_ == other.time_; }
    bool operator!=(const CTime& other) const { return time_ != other.time_; }
    bool operator<(const CTime& other) const { return time_ < other.time_; }
    bool operator>(const CTime& other) const { return time_ > other.time_; }
    bool operator<=(const CTime& other) const { return time_ <= other.time_; }
    bool operator>=(const CTime& other) const { return time_ >= other.time_; }
private:
    std::tm local() const { std::tm parts{}; const time_t value = static_cast<time_t>(time_); localtime_r(&value, &parts); return parts; }
    __time64_t time_;
};

// ---- CArchive ----------------------------------------------------------------------
// Scalar and CString serialization in the MFC binary format (little endian).
// Object serialization (CObject, CRuntimeClass) is not provided. The archive
// reads and writes the file directly; MFC's internal buffer is not emulated,
// so the file position is always exact.
class CArchive {
public:
    enum Mode { store = 0, load = 1, bNoFlushOnDelete = 2, bNoByteSwap = 4 };
    CFile* m_pFile;

    CArchive(CFile* file, UINT mode, int buffer_size = 4096, void* buffer = nullptr) : m_pFile(file), mode_(mode) {
        (void)buffer_size; (void)buffer;
    }
    ~CArchive() {}
    BOOL IsLoading() const { return (mode_ & load) != 0; }
    BOOL IsStoring() const { return (mode_ & load) == 0; }
    CFile* GetFile() const { return m_pFile; }
    void Flush() {}
    void Close() { m_pFile = nullptr; }
    void Abort() { m_pFile = nullptr; }

    UINT Read(void* buffer, UINT count) {
        if (!IsLoading()) AfxThrowArchiveException(CArchiveException::writeOnly);
        return count ? m_pFile->Read(buffer, count) : 0;
    }
    void Write(const void* buffer, UINT count) {
        if (!IsStoring()) AfxThrowArchiveException(CArchiveException::readOnly);
        if (count) m_pFile->Write(buffer, count);
    }

    // Windows widths: long and unsigned long are 32 bits in the file.
    CArchive& operator>>(bool& value) { BYTE raw; get(raw); value = raw != 0; return *this; }
    CArchive& operator>>(char& value) { return get(value); }
    CArchive& operator>>(signed char& value) { return get(value); }
    CArchive& operator>>(unsigned char& value) { return get(value); }
    CArchive& operator>>(short& value) { return get(value); }
    CArchive& operator>>(unsigned short& value) { return get(value); }
    CArchive& operator>>(int& value) { return get(value); }
    CArchive& operator>>(unsigned int& value) { return get(value); }
    CArchive& operator>>(long& value) { int raw; get(raw); value = raw; return *this; }
    CArchive& operator>>(unsigned long& value) { unsigned int raw; get(raw); value = raw; return *this; }
    CArchive& operator>>(long long& value) { return get(value); }
    CArchive& operator>>(unsigned long long& value) { return get(value); }
    CArchive& operator>>(float& value) { return get(value); }
    CArchive& operator>>(double& value) { return get(value); }
    CArchive& operator>>(char16_t& value) { return get(value); }

    CArchive& operator<<(bool value) { return put(static_cast<BYTE>(value ? 1 : 0)); }
    CArchive& operator<<(char value) { return put(value); }
    CArchive& operator<<(signed char value) { return put(value); }
    CArchive& operator<<(unsigned char value) { return put(value); }
    CArchive& operator<<(short value) { return put(value); }
    CArchive& operator<<(unsigned short value) { return put(value); }
    CArchive& operator<<(int value) { return put(value); }
    CArchive& operator<<(unsigned int value) { return put(value); }
    CArchive& operator<<(long value) { return put(static_cast<int>(value)); }
    CArchive& operator<<(unsigned long value) { return put(static_cast<unsigned int>(value)); }
    CArchive& operator<<(long long value) { return put(value); }
    CArchive& operator<<(unsigned long long value) { return put(value); }
    CArchive& operator<<(float value) { return put(value); }
    CArchive& operator<<(double value) { return put(value); }
    CArchive& operator<<(char16_t value) { return put(value); }

    // MFC string format: a length prefix of 1, 3, 7 or 15 bytes, then the
    // characters without a terminator. The prefix FF FFFE marks UTF-16 text.
    CArchive& operator>>(CString& text) {
        bool wide = false;
        ULONGLONG length = read_length(wide);
        // The text cannot be longer than the rest of the file. The test
        // comes before the allocation: a damaged prefix can name 4 GB.
        const ULONGLONG position = m_pFile->GetPosition(), size = m_pFile->GetLength();
        const ULONGLONG available = size > position ? size - position : 0;
        if (length > 0x3FFFFFFFull || length * (wide ? 2u : 1u) > available) AfxThrowArchiveException(CArchiveException::endOfFile);
        const UINT bytes = static_cast<UINT>(length) * (wide ? 2u : 1u);
        std::string raw(bytes, '\0');
        if (bytes && m_pFile->Read(&raw[0], bytes) != bytes) AfxThrowArchiveException(CArchiveException::endOfFile);
        if (!wide) { text = CString(raw.data(), static_cast<int>(bytes)); return *this; }
        // UTF-16 text: code units up to 0xFF map to one byte, others to '?'
        // (README, "CArchive"). Not found in the supplied client data.
        std::string narrow(static_cast<std::size_t>(length), '?');
        for (UINT i = 0; i < static_cast<UINT>(length); ++i) {
            const unsigned unit = static_cast<unsigned char>(raw[2 * i]) | (static_cast<unsigned>(static_cast<unsigned char>(raw[2 * i + 1])) << 8);
            if (unit <= 0xFF) narrow[i] = static_cast<char>(unit);
        }
        text = CString(narrow.data(), static_cast<int>(narrow.size()));
        return *this;
    }
    CArchive& operator<<(const CString& text) {
        const unsigned int length = static_cast<unsigned int>(text.GetLength());
        if (length < 0xFF) put(static_cast<BYTE>(length));
        else if (length < 0xFFFE) { put(static_cast<BYTE>(0xFF)); put(static_cast<WORD>(length)); }
        else { put(static_cast<BYTE>(0xFF)); put(static_cast<WORD>(0xFFFF)); put(length); }
        Write(text.GetString(), length);
        return *this;
    }

private:
    CArchive(const CArchive&);
    CArchive& operator=(const CArchive&);
    UINT mode_;
    template<class T> CArchive& get(T& value) {
        if (!IsLoading()) AfxThrowArchiveException(CArchiveException::writeOnly);
        if (m_pFile->Read(&value, sizeof(T)) != sizeof(T)) AfxThrowArchiveException(CArchiveException::endOfFile);
        return *this;
    }
    template<class T> CArchive& put(const T& value) {
        if (!IsStoring()) AfxThrowArchiveException(CArchiveException::readOnly);
        m_pFile->Write(&value, sizeof(T));
        return *this;
    }
    ULONGLONG read_length(bool& wide) {
        BYTE small; get(small);
        if (small < 0xFF) return small;
        WORD medium; get(medium);
        if (medium == 0xFFFE) { // UTF-16 marker; the length follows in the same format
            wide = true;
            get(small);
            if (small < 0xFF) return small;
            get(medium);
        }
        if (medium < 0xFFFF) return medium;
        DWORD large; get(large);
        if (large < 0xFFFFFFFFu) return large;
        ULONGLONG huge; get(huge);
        return huge;
    }
};
#if !defined(__LITTLE_ENDIAN__)
#error "CArchive of native/client/compat requires a little-endian target"
#endif

// ---- CFileFind (C4) ----------------------------------------------------------------
// Searches one directory for a name with the wildcards * and ? (case is
// ignored, as on Windows). The directory part goes through native_path.
class CFileFind {
public:
    CFileFind() {}
    virtual ~CFileFind() { Close(); }
    virtual BOOL FindFile(LPCTSTR name = nullptr, DWORD = 0) {
        Close();
        std::string pattern = name ? name : "*.*";
        for (char& c : pattern) if (c == '\\') c = '/';
        const std::size_t slash = pattern.rfind('/');
        directory_ = slash == std::string::npos ? std::string() : pattern.substr(0, slash + 1);
        std::string mask = slash == std::string::npos ? pattern : pattern.substr(slash + 1);
        if (mask == "*.*") mask = "*";
        const std::string native_dir = directory_.empty() ? std::string(".") : fourstory::compat::native_path(directory_.c_str());
        if (DIR* d = opendir(native_dir.c_str())) {
            while (dirent* e = readdir(d))
                if (fnmatch(mask.c_str(), e->d_name, FNM_CASEFOLD) == 0) found_.push_back(e->d_name);
            closedir(d);
        }
        native_dir_ = native_dir;
        next_ = 0;
        return found_.empty() ? FALSE : TRUE;
    }
    // MFC: returns FALSE for the last file; the current file is then still valid.
    virtual BOOL FindNextFile() {
        if (next_ >= found_.size()) return FALSE;
        current_ = found_[next_++];
        return next_ < found_.size() ? TRUE : FALSE;
    }
    void Close() { found_.clear(); current_.clear(); next_ = 0; }
    CString GetFileName() const { return CString(current_.c_str()); }
    CString GetFilePath() const { return CString((directory_ + current_).c_str()); }
    BOOL IsDots() const { return current_ == "." || current_ == ".." ? TRUE : FALSE; }
    BOOL IsDirectory() const {
        struct stat st;
        return stat((native_dir_ + "/" + current_).c_str(), &st) == 0 && S_ISDIR(st.st_mode) ? TRUE : FALSE;
    }
private:
    std::string directory_, native_dir_, current_;
    std::vector<std::string> found_;
    std::size_t next_ = 0;
};

// ---- CPoint, CSize, CRect -------------------------------------------------------------
class CSize : public SIZE {
public:
    CSize() { cx = 0; cy = 0; }
    CSize(int x, int y) { cx = x; cy = y; }
    CSize(SIZE size) { cx = size.cx; cy = size.cy; }
    CSize(POINT point) { cx = point.x; cy = point.y; }
    // MFC: cx is the signed low word, cy the signed high word.
    CSize(DWORD size) { cx = static_cast<short>(size & 0xFFFF); cy = static_cast<short>(size >> 16); }
    CSize operator-() const { return CSize(-cx, -cy); }
    bool operator==(SIZE other) const { return cx == other.cx && cy == other.cy; }
    bool operator!=(SIZE other) const { return !(*this == other); }
    CSize operator+(SIZE other) const { return CSize(cx + other.cx, cy + other.cy); }
    CSize operator-(SIZE other) const { return CSize(cx - other.cx, cy - other.cy); }
    void operator+=(SIZE other) { cx += other.cx; cy += other.cy; }
    void operator-=(SIZE other) { cx -= other.cx; cy -= other.cy; }
};
class CPoint : public POINT {
public:
    CPoint() { x = 0; y = 0; }
    CPoint(int px, int py) { x = px; y = py; }
    CPoint(POINT point) { x = point.x; y = point.y; }
    CPoint(SIZE size) { x = size.cx; y = size.cy; }
    // MFC: the low word is x, the high word y (signed), as in a message lParam.
    CPoint(DWORD packed) { x = static_cast<SHORT>(LOWORD(packed)); y = static_cast<SHORT>(HIWORD(packed)); }
    void Offset(int dx, int dy) { x += dx; y += dy; }
    void Offset(POINT point) { x += point.x; y += point.y; }
    void SetPoint(int px, int py) { x = px; y = py; }
    bool operator==(POINT other) const { return x == other.x && y == other.y; }
    bool operator!=(POINT other) const { return !(*this == other); }
    CPoint operator+(POINT other) const { return CPoint(x + other.x, y + other.y); }
    // The difference of two points is a size, as in MFC.
    CSize operator-(POINT other) const { return CSize(x - other.x, y - other.y); }
    CPoint operator+(SIZE size) const { return CPoint(x + size.cx, y + size.cy); }
    CPoint operator-(SIZE size) const { return CPoint(x - size.cx, y - size.cy); }
    CPoint operator-() const { return CPoint(-x, -y); }
    void operator+=(POINT other) { x += other.x; y += other.y; }
    void operator-=(POINT other) { x -= other.x; y -= other.y; }
    void operator+=(SIZE size) { x += size.cx; y += size.cy; }
    void operator-=(SIZE size) { x -= size.cx; y -= size.cy; }
};
class CRect : public RECT {
public:
    CRect() { left = top = right = bottom = 0; }
    CRect(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
    CRect(const RECT& rect) { left = rect.left; top = rect.top; right = rect.right; bottom = rect.bottom; }
    CRect(LPCRECT rect) { left = rect->left; top = rect->top; right = rect->right; bottom = rect->bottom; }
    CRect(POINT point, SIZE size) { left = point.x; top = point.y; right = point.x + size.cx; bottom = point.y + size.cy; }
    CRect(POINT top_left, POINT bottom_right) { left = top_left.x; top = top_left.y; right = bottom_right.x; bottom = bottom_right.y; }
    int Width() const { return right - left; }
    int Height() const { return bottom - top; }
    CSize Size() const { return CSize(right - left, bottom - top); }
    CPoint TopLeft() const { return CPoint(left, top); }
    CPoint BottomRight() const { return CPoint(right, bottom); }
    CPoint CenterPoint() const { return CPoint((left + right) / 2, (top + bottom) / 2); }
    operator LPRECT() { return this; }
    operator LPCRECT() const { return this; }
    BOOL IsRectEmpty() const { return right <= left || bottom <= top; }
    BOOL IsRectNull() const { return left == 0 && right == 0 && top == 0 && bottom == 0; }
    // The right and bottom edges are outside the rectangle.
    BOOL PtInRect(POINT point) const { return point.x >= left && point.x < right && point.y >= top && point.y < bottom; }
    void SetRect(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
    void SetRect(POINT top_left, POINT bottom_right) { SetRect(top_left.x, top_left.y, bottom_right.x, bottom_right.y); }
    void SetRectEmpty() { left = top = right = bottom = 0; }
    void CopyRect(LPCRECT rect) { *static_cast<RECT*>(this) = *rect; }
    BOOL EqualRect(LPCRECT rect) const { return left == rect->left && top == rect->top && right == rect->right && bottom == rect->bottom; }
    void InflateRect(int x, int y) { left -= x; right += x; top -= y; bottom += y; }
    void InflateRect(SIZE size) { InflateRect(size.cx, size.cy); }
    void InflateRect(LPCRECT rect) { InflateRect(rect->left, rect->top, rect->right, rect->bottom); }
    void DeflateRect(SIZE size) { InflateRect(-size.cx, -size.cy); }
    void DeflateRect(LPCRECT rect) { InflateRect(-rect->left, -rect->top, -rect->right, -rect->bottom); }
    void InflateRect(int l, int t, int r, int b) { left -= l; top -= t; right += r; bottom += b; }
    void DeflateRect(int x, int y) { InflateRect(-x, -y); }
    void DeflateRect(int l, int t, int r, int b) { InflateRect(-l, -t, -r, -b); }
    void OffsetRect(int x, int y) { left += x; right += x; top += y; bottom += y; }
    void MulDiv(int multiplier, int divisor) {
        left = ::MulDiv(left, multiplier, divisor); top = ::MulDiv(top, multiplier, divisor);
        right = ::MulDiv(right, multiplier, divisor); bottom = ::MulDiv(bottom, multiplier, divisor);
    }
    void OffsetRect(POINT point) { OffsetRect(point.x, point.y); }
    void MoveToXY(int x, int y) { OffsetRect(x - left, y - top); }
    void NormalizeRect() {
        if (left > right) { LONG value = left; left = right; right = value; }
        if (top > bottom) { LONG value = top; top = bottom; bottom = value; }
    }
    // An empty source gives an empty result and FALSE, as IntersectRect does.
    BOOL IntersectRect(LPCRECT a, LPCRECT b) {
        left = a->left > b->left ? a->left : b->left; top = a->top > b->top ? a->top : b->top;
        right = a->right < b->right ? a->right : b->right; bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
        if (left < right && top < bottom) return TRUE;
        SetRectEmpty(); return FALSE;
    }
    BOOL UnionRect(LPCRECT a, LPCRECT b) {
        const bool empty_a = a->right <= a->left || a->bottom <= a->top, empty_b = b->right <= b->left || b->bottom <= b->top;
        if (empty_a && empty_b) { SetRectEmpty(); return FALSE; }
        if (empty_a) { CopyRect(b); return TRUE; }
        if (empty_b) { CopyRect(a); return TRUE; }
        left = a->left < b->left ? a->left : b->left; top = a->top < b->top ? a->top : b->top;
        right = a->right > b->right ? a->right : b->right; bottom = a->bottom > b->bottom ? a->bottom : b->bottom;
        return TRUE;
    }
    bool operator==(const RECT& other) const { return EqualRect(&other) != 0; }
    bool operator!=(const RECT& other) const { return !EqualRect(&other); }
    void operator+=(POINT point) { OffsetRect(point.x, point.y); }
    void operator-=(POINT point) { OffsetRect(-point.x, -point.y); }
    CRect operator+(POINT point) const { CRect rect(*this); rect.OffsetRect(point.x, point.y); return rect; }
    CRect operator-(POINT point) const { CRect rect(*this); rect.OffsetRect(-point.x, -point.y); return rect; }
};
