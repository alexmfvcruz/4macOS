#pragma once
#include <algorithm>
#include <climits>
#include <cstring>
#include <stdexcept>
#include <string>
#include "legacy_format.hpp"

namespace fourstory::server {
// CStringA's byte-oriented operations used by the server. This owns bytes and
// never assumes UTF-8. Casing matches the measured en-US/1252 Windows reference:
// MakeUpper uses Windows byte casing; CompareNoCase uses default CRT C locale.
// Other codepages/locales need a separate verified profile.
class LegacyString {
    std::string bytes_;
    static int index(std::size_t value) {
        if(value==std::string::npos) return -1;
        if(value>INT_MAX) throw std::length_error("Legacy string index overflow");
        return static_cast<int>(value);
    }
    static unsigned char upper_1252(unsigned char byte) {
        if((byte>='a' && byte<='z') || (byte>=0xe0 && byte<=0xf6) || (byte>=0xf8 && byte<=0xfe))
            return static_cast<unsigned char>(byte-32);
        if(byte==0x9a) return 0x8a;
        if(byte==0x9c) return 0x8c;
        if(byte==0x9e) return 0x8e;
        if(byte==0xff) return 0x9f;
        return byte;
    }
public:
    LegacyString()=default;
    LegacyString(const char* text):bytes_(text?text:"") {}
    explicit LegacyString(std::string text):bytes_(std::move(text)) {}
    const std::string& bytes() const noexcept {return bytes_;}
    operator const char*() const noexcept {return bytes_.c_str();}
    const char* GetString() const noexcept {return bytes_.c_str();}
    int GetLength() const {return index(bytes_.size());}
    bool IsEmpty() const noexcept {return bytes_.empty();}
    void Empty() noexcept {bytes_.clear();}
#ifdef FOURSTORY_NATIVE_MODULE
    // Complete original modules: ATL CString::Format semantics (C printf with
    // CString arguments passed as their bytes); never throws on a mismatch.
    template<class T> static decltype(auto) printf_argument(const T& value) {
        if constexpr(std::is_same_v<T,LegacyString>) return value.bytes_.c_str();
        else return value;
    }
    template<class... Args> void Format(const char* format,const Args&... values) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#pragma clang diagnostic ignored "-Wformat-security"
        const int size=std::snprintf(nullptr,0,format?format:"",printf_argument(values)...);
        if(size<0) {bytes_.clear();return;}
        std::string text(static_cast<std::size_t>(size)+1,'\0');
        std::snprintf(text.data(),text.size(),format?format:"",printf_argument(values)...);
#pragma clang diagnostic pop
        text.resize(static_cast<std::size_t>(size));bytes_=std::move(text);
    }
#else
    template<class... Args> void Format(const char* format,const Args&... values) {
        auto convert=[](const auto& value)->legacy_format::Arg {
            if constexpr(std::is_same_v<std::decay_t<decltype(value)>,LegacyString>) return value.bytes();
            else return legacy_format::argument(value);
        };
        bytes_=legacy_format::render(format,{convert(values)...});
    }
#endif
    void Trim() {
        const auto first=bytes_.find_first_not_of(" \t\r\n\v\f");
        if(first==std::string::npos) {Empty();return;}
        const auto last=bytes_.find_last_not_of(" \t\r\n\v\f");
        bytes_=bytes_.substr(first,last-first+1);
    }
    LegacyString Left(int count) const {return LegacyString(bytes_.substr(0,static_cast<std::size_t>(std::max(0,count))));}
    LegacyString Right(int count) const {
        const auto size=std::min(bytes_.size(),static_cast<std::size_t>(std::max(0,count)));
        return LegacyString(bytes_.substr(bytes_.size()-size));
    }
    LegacyString Mid(int start,int count=INT_MAX) const {
        const auto offset=std::min(bytes_.size(),static_cast<std::size_t>(std::max(0,start)));
        return LegacyString(bytes_.substr(offset,static_cast<std::size_t>(std::max(0,count))));
    }
    int Find(char value,int start=0) const {return index(bytes_.find(value,static_cast<std::size_t>(std::max(0,start))));}
    int Find(const char* value,int start=0) const {
        if(!value) throw std::invalid_argument("Null legacy search string");
        return index(bytes_.find(value,static_cast<std::size_t>(std::max(0,start))));
    }
    int ReverseFind(char value) const {return index(bytes_.rfind(value));}
    LegacyString Tokenize(const char* delimiters,int& start) const {
        if(!delimiters) throw std::invalid_argument("Null token delimiters");
        if(start<0) return {};
        const auto first=bytes_.find_first_not_of(delimiters,static_cast<std::size_t>(start));
        if(first==std::string::npos) {start=-1;return {};}
        const auto end=bytes_.find_first_of(delimiters,first);
        start=index(end==std::string::npos?bytes_.size()+1:end+1);
        return LegacyString(bytes_.substr(first,end==std::string::npos?end:end-first));
    }
    LegacyString& MakeUpper() {
        for(auto& byte:bytes_) byte=static_cast<char>(upper_1252(static_cast<unsigned char>(byte)));
        return *this;
    }
    int CompareNoCase(const LegacyString& other) const {
        auto lower=[](unsigned char byte){return byte>='A' && byte<='Z'?byte+32:byte;};
        const auto size=std::min(bytes_.size(),other.bytes_.size());
        for(std::size_t i=0;i<size;++i) {
            const int difference=lower(static_cast<unsigned char>(bytes_[i]))-lower(static_cast<unsigned char>(other.bytes_[i]));
            if(difference) return difference;
        }
        return bytes_.size()==other.bytes_.size()?0:(bytes_.size()<other.bytes_.size()?-1:1);
    }
    LegacyString& operator+=(const LegacyString& other) {bytes_+=other.bytes_;return *this;}
    // Further CStringA members used by the complete original modules.
    LegacyString(char value,int count):bytes_(static_cast<std::size_t>(std::max(0,count)),value) {}
    char GetAt(int at) const {
        if(at<0 || static_cast<std::size_t>(at)>=bytes_.size()) throw std::out_of_range("Legacy string index");
        return bytes_[static_cast<std::size_t>(at)];
    }
    char operator[](int at) const {return GetAt(at);}
    void SetAt(int at,char value) {
        if(at<0 || static_cast<std::size_t>(at)>=bytes_.size()) throw std::out_of_range("Legacy string index");
        bytes_[static_cast<std::size_t>(at)]=value;
    }
    void AppendChar(char value) {bytes_+=value;}
    LegacyString& operator+=(char value) {bytes_+=value;return *this;}
    LegacyString& operator+=(const char* value) {if(value) bytes_+=value;return *this;}
    void TrimLeft() {
        const auto first=bytes_.find_first_not_of(" \t\r\n\v\f");
        bytes_=first==std::string::npos?std::string():bytes_.substr(first);
    }
    void TrimRight() {
        const auto last=bytes_.find_last_not_of(" \t\r\n\v\f");
        bytes_=last==std::string::npos?std::string():bytes_.substr(0,last+1);
    }
    void TrimLeft(const char* set) {
        const auto first=bytes_.find_first_not_of(set?set:"");
        bytes_=first==std::string::npos?std::string():bytes_.substr(first);
    }
    void TrimRight(const char* set) {
        const auto last=bytes_.find_last_not_of(set?set:"");
        bytes_=last==std::string::npos?std::string():bytes_.substr(0,last+1);
    }
    void TrimLeft(char value) {const auto first=bytes_.find_first_not_of(value);bytes_=first==std::string::npos?std::string():bytes_.substr(first);}
    void TrimRight(char value) {const auto last=bytes_.find_last_not_of(value);bytes_=last==std::string::npos?std::string():bytes_.substr(0,last+1);}
    int FindOneOf(const char* set) const {
        if(!set) throw std::invalid_argument("Null legacy character set");
        return index(bytes_.find_first_of(set));
    }
    int Replace(const char* from,const char* to) {
        if(!from || !*from) return 0;
        const std::string target(from),replacement(to?to:"");
        int count=0;
        for(std::size_t at=bytes_.find(target);at!=std::string::npos;at=bytes_.find(target,at+replacement.size())) {
            bytes_.replace(at,target.size(),replacement);++count;
        }
        return count;
    }
    int Replace(char from,char to) {int count=0;for(auto& c:bytes_) if(c==from) {c=to;++count;}return count;}
    int Remove(char value) {
        const auto before=bytes_.size();
        bytes_.erase(std::remove(bytes_.begin(),bytes_.end(),value),bytes_.end());
        return static_cast<int>(before-bytes_.size());
    }
    int Insert(int at,const char* value) {
        const auto offset=std::min(bytes_.size(),static_cast<std::size_t>(std::max(0,at)));
        bytes_.insert(offset,value?value:"");return GetLength();
    }
    int Insert(int at,char value) {
        const auto offset=std::min(bytes_.size(),static_cast<std::size_t>(std::max(0,at)));
        bytes_.insert(offset,1,value);return GetLength();
    }
    int Delete(int at,int count=1) {
        if(at<0 || count<=0 || static_cast<std::size_t>(at)>=bytes_.size()) return GetLength();
        bytes_.erase(static_cast<std::size_t>(at),static_cast<std::size_t>(count));return GetLength();
    }
    LegacyString& MakeLower() {
        // Default C locale lowering, matching the measured CompareNoCase profile.
        for(auto& byte:bytes_) if(byte>='A' && byte<='Z') byte=static_cast<char>(byte+32);
        return *this;
    }
    int Compare(const char* other) const {return std::strcmp(bytes_.c_str(),other?other:"");}
    template<class... Args> void AppendFormat(const char* format,const Args&... values) {
        LegacyString piece;piece.Format(format,values...);bytes_+=piece.bytes_;
    }
    // GetBuffer exposes writable storage; ReleaseBuffer restores the length.
    char* GetBuffer(int minimum=0) {
        if(minimum>0 && bytes_.size()<static_cast<std::size_t>(minimum)) bytes_.resize(static_cast<std::size_t>(minimum));
        return bytes_.data();
    }
    char* GetBufferSetLength(int length) {bytes_.resize(static_cast<std::size_t>(std::max(0,length)));return bytes_.data();}
    void ReleaseBuffer(int length=-1) {
        if(length<0) bytes_.resize(std::strlen(bytes_.c_str()));
        else bytes_.resize(std::min(bytes_.size(),static_cast<std::size_t>(length)));
    }
    friend LegacyString operator+(LegacyString left,const LegacyString& right) {left+=right;return left;}
    friend bool operator==(const LegacyString& left,const LegacyString& right) {return left.bytes_==right.bytes_;}
    friend bool operator!=(const LegacyString& left,const LegacyString& right) {return !(left==right);}
    friend bool operator==(const LegacyString& left,const char* right) {return left==LegacyString(right);}
    friend bool operator==(const char* left,const LegacyString& right) {return LegacyString(left)==right;}
    friend bool operator!=(const LegacyString& left,const char* right) {return !(left==right);}
    friend bool operator!=(const char* left,const LegacyString& right) {return !(left==right);}
    friend bool operator<(const LegacyString& left,const LegacyString& right) {return left.bytes_<right.bytes_;}
};
}
