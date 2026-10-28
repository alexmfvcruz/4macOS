#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace fourstory::server::legacy_format {
using Arg=std::variant<std::uint64_t,double,std::string>;
template<class T,std::enable_if_t<std::is_integral_v<T>,int> =0>
Arg argument(T value) {return static_cast<std::uint64_t>(value);}
template<class T,std::enable_if_t<std::is_enum_v<T>,int> =0>
Arg argument(T value) {return argument(static_cast<std::underlying_type_t<T>>(value));}
inline Arg argument(double value) {return value;}
inline Arg argument(float value) {return static_cast<double>(value);}
inline Arg argument(const std::string& value) {return value;}
inline Arg argument(const char* value) {
    if(!value) throw std::invalid_argument("Null legacy format string argument");
    return std::string(value);
}
template<class T> std::string print(const std::string& spec,T value) {
    const int size=std::snprintf(nullptr,0,spec.c_str(),value);
    if(size<0 || size>100000) throw std::length_error("Legacy formatted value too large");
    std::vector<char> buffer(static_cast<std::size_t>(size)+1);
    if(std::snprintf(buffer.data(),buffer.size(),spec.c_str(),value)!=size)
        throw std::runtime_error("Legacy formatting failed");
    return std::string(buffer.data(),static_cast<std::size_t>(size));
}
// Parse first, then pass exactly the matching type to printf. Win32 long and
// default integer fields are 32 bits regardless of the native argument width.
inline std::string render(const char* format,const std::vector<Arg>& arguments) {
    if(!format) throw std::invalid_argument("Null legacy format");
    std::string result;std::size_t next=0;
    for(std::size_t at=0;format[at];) {
        if(format[at]!='%') {result+=format[at++];continue;}
        ++at;if(format[at]=='%') {result+='%';++at;continue;}
        std::string prefix="%";
        while(format[at] && std::strchr("-+ #0",format[at])) prefix+=format[at++];
        unsigned field=0;
        while(format[at]>='0' && format[at]<='9') {
            field=field*10+static_cast<unsigned>(format[at]-'0');
            if(field>100000) throw std::length_error("Legacy format width too large");
            prefix+=format[at++];
        }
        if(format[at]=='.') {
            prefix+=format[at++];field=0;
            while(format[at]>='0' && format[at]<='9') {
                field=field*10+static_cast<unsigned>(format[at]-'0');
                if(field>100000) throw std::length_error("Legacy format precision too large");
                prefix+=format[at++];
            }
        }
        unsigned bits=32;bool length=false;
        if(format[at]=='h') {length=true;++at;bits=16;if(format[at]=='h'){++at;bits=8;}}
        else if(format[at]=='l') {length=true;++at;if(format[at]=='l'){++at;bits=64;}}
        else if(format[at]=='I') {
            length=true;++at;
            if(std::strncmp(format+at,"64",2)==0) {bits=64;at+=2;}
            else if(std::strncmp(format+at,"32",2)==0) at+=2;
            else throw std::invalid_argument("Unsupported legacy integer length");
        }
        const char conversion=format[at];
        if(!conversion || !std::strchr("diuoxXcsfFeEgG",conversion))
            throw std::invalid_argument("Unsupported legacy format conversion");
        ++at;if(next>=arguments.size()) throw std::invalid_argument("Missing legacy format argument");
        const auto& value=arguments[next++];
        if(std::strchr("diuoxXc",conversion)) {
            const auto* integer=std::get_if<std::uint64_t>(&value);
            if(!integer) throw std::invalid_argument("Legacy integer format type mismatch");
            auto narrowed=*integer;
            if(bits<64) narrowed&=(std::uint64_t(1)<<bits)-1;
            if(conversion=='c') {
                if(length) throw std::invalid_argument("Unsupported wide legacy character");
                result+=print(prefix+"c",static_cast<int>(narrowed&255));
            } else if(conversion=='d' || conversion=='i') {
                if(bits<64 && (narrowed&(std::uint64_t(1)<<(bits-1)))) narrowed|=~((std::uint64_t(1)<<bits)-1);
                std::int64_t signed_value;std::memcpy(&signed_value,&narrowed,8);
                result+=print(prefix+"ll"+conversion,static_cast<long long>(signed_value));
            } else result+=print(prefix+"ll"+conversion,static_cast<unsigned long long>(narrowed));
        } else if(conversion=='s') {
            const auto* string=std::get_if<std::string>(&value);
            if(length || !string) throw std::invalid_argument("Legacy string format type mismatch");
            result+=print(prefix+"s",string->c_str());
        } else {
            const auto* floating=std::get_if<double>(&value);
            if(length || !floating) throw std::invalid_argument("Legacy floating format type mismatch");
            result+=print(prefix+conversion,*floating);
        }
        if(result.size()>1000000) throw std::length_error("Legacy formatted string too large");
    }
    if(next!=arguments.size()) throw std::invalid_argument("Extra legacy format arguments");
    return result;
}
}
