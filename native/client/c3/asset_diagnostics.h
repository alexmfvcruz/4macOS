#pragma once
#include <filesystem>
#include <fstream>
// Opt-in, first-frame-only evidence from actual bound textures and samplers.
class AssetDiagnostics {
    std::filesystem::path directory_;
    std::ofstream metadata_;
    unsigned draw_=0;
public:
    explicit AssetDiagnostics(const std::string& prefix):directory_(prefix+"-diagnostics"){}
    void draw(IDirect3DDevice9* device,bool firstFrame){
        if(const char* bias=std::getenv("C3_FORCE_LOD_BIAS")){float value=std::stof(bias);if(!std::isfinite(value))throw std::runtime_error("invalid diagnostic LOD bias");DWORD bits;std::memcpy(&bits,&value,4);for(DWORD stage=0;stage<2;++stage)check(device->SetSamplerState(stage,D3DSAMP_MIPMAPLODBIAS,bits),"diagnostic override bias");}
        if(!firstFrame||!std::getenv("C3_DIAGNOSTICS"))return;
        if(!metadata_.is_open()){
            std::filesystem::create_directories(directory_);metadata_.open(directory_/"state.tsv");
            if(!metadata_)throw std::runtime_error("diagnostic metadata open failed");
            metadata_<<"draw\tstage\tproperty\tvalue\n";
        }
        for(DWORD stage=0;stage<2;++stage){
            for(auto state:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER,D3DSAMP_MIPMAPLODBIAS,D3DSAMP_MAXMIPLEVEL,D3DSAMP_MAXANISOTROPY,D3DSAMP_SRGBTEXTURE,D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV}){
                DWORD value=0;check(device->GetSamplerState(stage,state,&value),"diagnostic sampler");metadata_<<draw_<<'\t'<<stage<<'\t'<<"sampler"<<unsigned(state)<<'\t'<<value<<'\n';
            }
            Com<IDirect3DBaseTexture9> base;check(device->GetTexture(stage,base.put()),"diagnostic texture");if(!base.p)continue;
            if(base->GetType()!=D3DRTYPE_TEXTURE)throw std::runtime_error("diagnostic non-2D texture");
            auto* texture=static_cast<IDirect3DTexture9*>(base.p);
            metadata_<<draw_<<'\t'<<stage<<"\tlevels\t"<<texture->GetLevelCount()<<'\n';
            for(UINT level=0;level<texture->GetLevelCount();++level){
                D3DSURFACE_DESC desc{};check(texture->GetLevelDesc(level,&desc),"diagnostic mip description");
                const auto name="draw"+std::to_string(draw_)+"-stage"+std::to_string(stage)+"-mip"+std::to_string(level);
                metadata_<<draw_<<'\t'<<stage<<'\t'<<"mip"<<level<<'\t'<<desc.Width<<','<<desc.Height<<','<<unsigned(desc.Format)<<'\n';
                unsigned rows=desc.Height,rowBytes=0;
                switch(desc.Format){
                    case D3DFMT_DXT1:rowBytes=((desc.Width+3)/4)*8;rows=(rows+3)/4;break;
                    case D3DFMT_DXT2:case D3DFMT_DXT3:case D3DFMT_DXT4:case D3DFMT_DXT5:rowBytes=((desc.Width+3)/4)*16;rows=(rows+3)/4;break;
                    case D3DFMT_A8R8G8B8:case D3DFMT_X8R8G8B8:rowBytes=desc.Width*4;break;
                    case D3DFMT_R5G6B5:case D3DFMT_A4R4G4B4:case D3DFMT_A1R5G5B5:rowBytes=desc.Width*2;break;
                    case D3DFMT_A8:rowBytes=desc.Width;break;
                    default:throw std::runtime_error("diagnostic unsupported texture format");
                }
                D3DLOCKED_RECT lock{};check(texture->LockRect(level,&lock,nullptr,D3DLOCK_READONLY),"diagnostic mip lock");
                if(!lock.pBits||lock.Pitch<int(rowBytes)){texture->UnlockRect(level);throw std::runtime_error("diagnostic invalid texture pitch");}
                std::ofstream output(directory_/(name+".bin"),std::ios::binary);
                for(unsigned row=0;row<rows;++row)output.write(static_cast<const char*>(lock.pBits)+size_t(row)*lock.Pitch,rowBytes);
                output.close();check(texture->UnlockRect(level),"diagnostic mip unlock");if(!output)throw std::runtime_error("diagnostic mip write failed");
            }
        }
        metadata_.flush();if(!metadata_)throw std::runtime_error("diagnostic metadata write failed");++draw_;
    }
};
