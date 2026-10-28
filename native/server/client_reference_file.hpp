#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace fourstory::server {
// Read-only startup reference. Mirrors Login LoadDataLogin's initialized 8-byte
// accumulator, including retained high bytes after a final short ReadFile.
class ClientReferenceFile {
    std::FILE* file_=nullptr;
    std::uint64_t checksum_=0;
public:
    ClientReferenceFile()=default;
    ClientReferenceFile(const ClientReferenceFile&)=delete;
    ClientReferenceFile& operator=(const ClientReferenceFile&)=delete;
    ~ClientReferenceFile(){close();}
    void close() noexcept {if(file_) std::fclose(file_);file_=nullptr;checksum_=0;}
    bool open(const char* path) {
        close();if(!path) return false;
        const auto fd=::open(path,O_RDONLY|O_CLOEXEC);
        if(fd<0) return false;
        struct stat info{};
        if(fstat(fd,&info)<0 || !S_ISREG(info.st_mode)) {::close(fd);return false;}
        file_=fdopen(fd,"rb");if(!file_) {::close(fd);return false;}
        std::array<unsigned char,8> block{};
        for(;;) {
            const auto count=std::fread(block.data(),1,block.size(),file_);
            if(!count || std::ferror(file_)) break;
            std::uint64_t value=0;
            for(unsigned i=0;i<8;++i) value|=std::uint64_t(block[i])<<(8*i);
            checksum_^=value;
        }
        return true;
    }
    bool available() const noexcept {return file_!=nullptr;}
    std::uint64_t checksum() const noexcept {return checksum_;}
};
}
