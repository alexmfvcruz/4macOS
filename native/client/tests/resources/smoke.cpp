// Loads the release indexes and eager resources through the original engine.
// Lazy GetTEX/Restore/LockMAP coverage belongs to the canonical C2 dump.
#include <fourstory_engine_resources.h>
#include "../../device/empty_device.h"
#include <cstdio>
#include <stdexcept>

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: c2-resource-smoke ASSET_DIRECTORY\n");
        return 2;
    }
    try {
        if (chdir(argv[1])) throw std::runtime_error("asset directory unavailable");
        {
            CD3DDevice context;
            context.m_option.m_nTextureDetail = static_cast<TEXTURE_DETAIL_LEVEL>(0);
            context.m_option.m_dwScreenX = 640;
            context.m_option.m_dwScreenY = 480;
            if (FAILED(fourstory::device::CreateEmptyDevice(&context.m_pDevice)))
                throw std::runtime_error("empty device failed");
            CTachyonRes::m_pDEVICE = &context;
            CTachyonMesh::InitGlobalVB();
            {
                CTachyonRes resources;
                resources.Init("TClient", 0);
                std::printf("COUNTS tex=%zu imgbuf=%zu img=%zu ani=%zu mesh=%zu obj=%zu sfx=%zu map=%zu\n",
                    resources.m_mapTEX.size(), resources.m_mapIMGBUF.size(), resources.m_mapIMG.size(),
                    resources.m_mapANI.size(), resources.m_mapMESH.size(), resources.m_mapOBJ.size(),
                    resources.m_mapSFX.size(), resources.m_mapMAP.size());
            }
            CTachyonMesh::ReleaseGlobalVB();
            CTachyonRes::m_pDEVICE = nullptr;
        }
        const auto& s = fourstory::device::statistics();
        std::printf("DEVICE live_objects=%lld live_bytes=%lld textures=%llu vertex_buffers=%llu index_buffers=%llu failed_creations=%llu unsupported_methods=%zu\n",
            s.live_objects, s.live_bytes, s.textures, s.vertex_buffers, s.index_buffers,
            s.failed_creations, s.not_executed.size());
        if (s.live_objects || s.live_bytes || s.failed_creations || !s.not_executed.empty())
            throw std::runtime_error("unexpected device failure, unsupported call or leaked resources");
    } catch (CFileException* error) {
        std::fprintf(stderr, "FAIL file cause=%d errno=%d\n", error->m_cause, error->m_lOsError);
        error->Delete();
        return 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s\n", error.what());
        return 1;
    }
}
