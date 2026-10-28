#include "StdAfx.h"
#include "TClient.h"
#include "TClientWnd.h"
#include "TClientGame.h"
#include <fourstory_display_preview.h>
#include <fourstory_preview_timer.h>
#include <fourstory_sdl_backend.h>

namespace {
constexpr DWORD kPreviewId = 0xf5010002;
constexpr DWORD kResumeMilliseconds = 700;
class DisplayPreview;
DisplayPreview* livePreview = nullptr;
HWND unfocusedWindow = nullptr;
void drawShade(IDirect3DDevice9* device, int width, int height, DWORD opacity = 0x88) {
    if (!device) return;
    IDirect3DStateBlock9* saved = nullptr;
    if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &saved))) return;
    if (FAILED(saved->Capture())) { saved->Release(); return; }
    struct Vertex { float x,y,z,w; DWORD color; };
    const float r = float(width)-.5f, b = float(height)-.5f;
    const DWORD color = opacity << 24;
    const Vertex vertices[] = {{-.5f,-.5f,0,1,color},{r,-.5f,0,1,color},
        {-.5f,b,0,1,color},{r,b,0,1,color}};
    device->SetVertexShader(nullptr); device->SetPixelShader(nullptr);
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    device->SetTexture(0,nullptr);
    device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
    device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
    device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
    device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
    device->SetRenderState(D3DRS_ZENABLE,FALSE); device->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);
    device->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE);
    device->SetRenderState(D3DRS_COLORWRITEENABLE,0xf);
    device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,TRUE);
    device->SetRenderState(D3DRS_SRCBLENDALPHA,D3DBLEND_ONE);
    device->SetRenderState(D3DRS_DESTBLENDALPHA,D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_BLENDOPALPHA,D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
    device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
    device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
    device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
    device->SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    device->SetRenderState(D3DRS_FILLMODE,D3DFILL_SOLID);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex));
    saved->Apply(); saved->Release();
}
struct Snapshot {
    DWORD width, height;
    FLOAT scale;
    bool fullscreen;
    CString font;
};
class DisplayPreview : public CTClientUIBase {
    CTClientGame* game_;
    Snapshot previous_;
    fourstory::PreviewTimer timer_;
    bool pending_ = false, restoring_ = false, undo_ = false, keep_ = false;
    TComponent *title_, *settings_, *instruction_, *countdown_;
    TComponent* pressed_ = nullptr;
    bool consumedDown_ = false;
    bool resuming_ = false;
    DWORD resumedAt_ = 0;
    TComponent* resumeText_ = nullptr;
    CFont readableFonts_[4];
    CGDIFont readableGdi_[4];
    CGDIFont* originalFonts_[4] = {};
    TButton *keepButton_, *undoButton_;
    Snapshot current() const {
        auto& o = game_->GetMainWnd()->m_Device.m_option;
        return {TRealScreenX(o), TRealScreenY(o), CD3DDevice::m_fUIScale,
            fourstory::shell::game_fullscreen(game_->GetMainWnd()->GetSafeHwnd()),
            theApp.GetProfileString("Settings", "Font", "")};
    }
    void refreshFonts() {
        TComponent* labels[] = {title_, settings_, instruction_, countdown_};
        // Use the selected game's face, with larger, high-contrast preview text.
        const int body = min(18, max(11, (int(game_->GetScreenX()) - 24) / 29));
        for (int i=0; i<4; ++i) {
            if (!originalFonts_[i] || !originalFonts_[i]->m_pFont) continue;
            LOGFONT lf = {};
            originalFonts_[i]->m_pFont->GetLogFont(&lf);
            lf.lfHeight = -(i == 0 ? body + 5 : body);
            lf.lfWeight = i == 0 ? FW_BOLD : FW_NORMAL;
            readableFonts_[i].DeleteObject();
            if (!readableFonts_[i].CreateFontIndirect(&lf)) continue;
            readableGdi_[i].m_pFont = &readableFonts_[i];
            readableGdi_[i].m_clr = 0xffffffff;
            labels[i]->SetFont(&readableGdi_[i], nullptr);
        }
    }
    void layout() {
        const int width = game_->GetScreenX(), height = game_->GetScreenY();
        MoveComponent(CPoint(0, 0));
        SetComponentSize(CSize(width, height));
        CSize keepSize, undoSize;
        keepButton_->GetComponentSize(&keepSize); undoButton_->GetComponentSize(&undoSize);
        const int groupHeight = 130 + keepSize.cy + 8 + undoSize.cy;
        const int top = max(8, (height - groupHeight) / 2);
        TComponent* labels[] = {title_, settings_, instruction_, countdown_};
        const int offsets[] = {0, 35, 68, 96};
        for (int i = 0; i < 4; ++i) {
            labels[i]->SetComponentSize(CSize(max(1, width - 16), 32));
            labels[i]->MoveComponent(CPoint(8, top + offsets[i]));
            labels[i]->SetTextAlign(ALIGN_CENTER);
            if (auto* font = labels[i]->Get3DFont()) {
                font->m_bOutLine = TRUE; font->m_dwLineColor = 0xff000000;
                font->m_dwColor = 0xffffffff;
            }
        }
        // Match the Outro's vertical arrangement: large primary, small secondary.
        keepButton_->MoveComponent(CPoint(max(0,(width - keepSize.cx)/2), top + 130));
        undoButton_->MoveComponent(CPoint(max(0,(width - undoSize.cx)/2), top + 130 + keepSize.cy + 8));
    }
    void layoutResume() {
        const int width = game_->GetScreenX(), height = game_->GetScreenY();
        MoveComponent(CPoint(0,0)); SetComponentSize(CSize(width,height));
        resumeText_->MoveComponent(CPoint(8,max(0,(height-36)/2)));
        resumeText_->SetComponentSize(CSize(max(1,width-16),36));
        resumeText_->SetTextAlign(ALIGN_CENTER);
        if (auto* font = resumeText_->Get3DFont()) {
            font->m_bOutLine = TRUE; font->m_dwLineColor = 0xff000000;
            font->m_dwColor = 0xffffffff;
        }
    }
    void shade(DWORD opacity = 0x88) {
        drawShade(game_->GetMainWnd()->m_Device.m_pDevice,
            game_->GetScreenX(),game_->GetScreenY(),opacity);
    }
    void finish(bool undo) {
        pending_ = false; timer_.stop(); undo_ = keep_ = false; resuming_ = false;
        if (pressed_) static_cast<TButton*>(pressed_)->Select(FALSE);
        pressed_ = nullptr;
        ShowComponent(FALSE);
        if (undo) {
            restoring_ = true;
            auto& o = game_->GetMainWnd()->m_Device.m_option;
            o.m_bWindowedMode = !previous_.fullscreen;
            game_->ChangeResolution(previous_.width, previous_.height);
            game_->ApplyUIScale(previous_.scale);
            theApp.WriteProfileString("Settings", "Font", previous_.font);
            ReloadTachyonFonts(previous_.font);
            game_->RefreshNativeDisplaySettings();
            restoring_ = false;
        }
        theApp.SaveStdProfileSettings();
    }
public:
    DisplayPreview(CTClientGame* game, FRAMEDESC_SHAREDPTR desc)
        : CTClientUIBase(game, desc), game_(game) {
        m_id = kPreviewId;
        livePreview = this;
        title_ = FindKid(26876); settings_ = FindKid(26927); countdown_ = FindKid(11460);
        instruction_ = new TComponent(this, *countdown_, false);
        instruction_->m_id = kPreviewId + 1; AddKid(instruction_);
        TComponent* labels[] = {title_, settings_, instruction_, countdown_};
        for (int i=0; i<4; ++i) originalFonts_[i] = labels[i]->GetFont();
        instruction_->m_strText = "Press Left Control (^) key to revert changes";
        undoButton_ = static_cast<TButton*>(FindKid(26878));
        keepButton_ = static_cast<TButton*>(FindKid(26879));
        // Do not inherit any exit commands or illustrated-overlay behavior.
        for (auto* kid : m_kids) {
            kid->ShowComponent(FALSE);
            for (int i = 0; i < TCML_MENU_COUNT; ++i) kid->m_menu[i] = 0;
        }
        keepButton_->m_strText = "Keep settings";
        undoButton_->m_strText = "Undo";
        resumeText_ = new TComponent(this, *title_, false);
        resumeText_->m_id = kPreviewId + 2; resumeText_->m_strText = "Resuming...";
        for (int i=0; i<TCML_MENU_COUNT; ++i) resumeText_->m_menu[i] = 0;
        AddKid(resumeText_);
        m_bUseBasis = FALSE;
        ShowComponent(FALSE);
    }
    ~DisplayPreview() override {
        if (livePreview == this) livePreview = nullptr;
        TComponent* labels[] = {title_, settings_, instruction_, countdown_};
        // Children outlive derived members; detach their references to our fonts.
        for (int i=0; i<4; ++i) labels[i]->SetFont(originalFonts_[i], nullptr);
        resumeText_->SetFont(originalFonts_[0],nullptr);
    }
    void focusResumed(HWND window) {
        if (window != game_->GetMainWnd()->GetSafeHwnd() ||
            game_->GetMainWnd()->GetMainFrame() != game_ || timer_.active() || pending_) return;
        refreshFonts();
        resumeText_->SetFont(&readableGdi_[0],nullptr);
        resuming_ = true; resumedAt_ = GetTickCount();
        game_->RemoveKid(this); game_->AddKid(this);
        layoutResume(); ShowComponent(TRUE);
    }
    void nativeModeChanged(HWND window, bool fullscreen) {
        if (window != game_->GetMainWnd()->GetSafeHwnd()) return;
        auto& o = game_->GetMainWnd()->m_Device.m_option;
        const bool userChanged = bool(!o.m_bWindowedMode) != fullscreen;
        // Game-setting commands update the option before requesting SDL. A
        // differing value here is a traffic-light/Spaces transition instead.
        if (userChanged && !pending_ && !restoring_ &&
            game_->GetMainWnd()->GetMainFrame() == game_) {
            if (!timer_.active()) {
                previous_ = current(); previous_.fullscreen = !fullscreen;
            }
            pending_ = true;
        }
        o.m_bWindowedMode = !fullscreen;
        game_->RefreshNativeDisplaySettings();
        theApp.SaveStdProfileSettings();
    }
    void ShowComponent(BOOL visible) override {
        // Parent frames show all children on world entry; this overlay is opt-in.
        visible = visible && (timer_.active() || resuming_);
        CTClientUIBase::ShowComponent(FALSE);
        m_bVisible = visible;
        TComponent* controls[] = {title_, settings_, instruction_, countdown_, keepButton_, undoButton_};
        for (auto* control : controls) control->ShowComponent(visible && timer_.active());
        resumeText_->ShowComponent(visible && resuming_ && !timer_.active());
    }
    void begin() {
        if (restoring_ || pending_) return;
        // Consecutive changes get a fresh eight seconds, preserving the first
        // snapshot so Undo can restore the settings before this preview began.
        if (!timer_.active()) previous_ = current();
        pending_ = true;
    }
    void tick() {
        if (resuming_ && DWORD(GetTickCount()-resumedAt_) >= kResumeMilliseconds) {
            resuming_ = false; ShowComponent(timer_.active());
        }
        if (resuming_) layoutResume();
        if (pending_) {
            // A notice may open between the settings callback and this frame.
            // Preserve its input ownership and start our timer after it closes.
            if (game_->GetMainWnd()->GetMainFrame() != game_) return;
            // Application commands and device reset run before the next game frame.
            pending_ = false;
            const Snapshot now = current();
            if (now.width == previous_.width && now.height == previous_.height &&
                now.scale == previous_.scale && now.fullscreen == previous_.fullscreen && now.font == previous_.font) {
                finish(false); return;
            }
            timer_.start(GetTickCount(), (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0);
            game_->RemoveKid(this); game_->AddKid(this);
            resuming_ = false;
            refreshFonts(); ShowComponent(TRUE);
        }
        if (!timer_.active()) return;
        // Other modals keep their own input rules. A display preview never
        // becomes the main input frame and never changes character movement.
        if (game_->GetMainWnd()->GetMainFrame() != game_) return;
        const auto result = timer_.update(GetTickCount(), (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0);
        if (undo_ || result == fourstory::PreviewTimer::Undo) { finish(true); return; }
        if (keep_ || result == fourstory::PreviewTimer::Keep) { finish(false); return; }
        const Snapshot now = current();
        title_->m_strText.Format("%u x %u  |  %s", now.width, now.height, now.fullscreen ? "Fullscreen" : "Windowed");
        CString face = now.font.IsEmpty() ? CString("Game default") : now.font;
        settings_->m_strText.Format("UI scale: %d%%  |  Font: %s (%d%%)",
            int(now.scale * 100 + .5f), LPCSTR(face), theApp.GetProfileInt("Settings", "FontSize", 100));
        countdown_->m_strText.Format("These settings will be kept in %u seconds", timer_.remaining(GetTickCount()));
        layout();
    }
    HRESULT Render(DWORD tick) override {
        if (!IsVisible() || game_->GetMainWnd()->GetMainFrame() != game_ ||
            unfocusedWindow == game_->GetMainWnd()->GetSafeHwnd()) return S_OK;
        if (resuming_ && !timer_.active()) {
            const DWORD elapsed = min(DWORD(GetTickCount()-resumedAt_),kResumeMilliseconds);
            // Smoothstep keeps both ends gentle. Fade the lettering and its
            // outline with the shade so no opaque text disappears at the end.
            const float progress = float(elapsed) / float(kResumeMilliseconds);
            const float opacity = 1.0f - progress * progress * (3.0f - 2.0f * progress);
            const DWORD alpha = DWORD(255.0f * opacity + .5f);
            resumeText_->SetTextClr((alpha << 24) | 0x00ffffff);
            shade(DWORD(0x88 * opacity + .5f));
        } else shade();
        return CTClientUIBase::Render(tick);
    }
    // Rendering covers the game; input only belongs to the two buttons.
    BOOL HitTest(CPoint) override { return FALSE; }
    bool click(UINT flags, CPoint pt, bool down) {
        if (down) {
            // A fresh press replaces any stale ownership after focus loss.
            if (pressed_) static_cast<TButton*>(pressed_)->Select(FALSE);
            consumedDown_ = false; pressed_ = nullptr;
            if (!IsVisible() || !timer_.active()) return false;
            pressed_ = keepButton_->HitTest(pt) ? keepButton_ : undoButton_->HitTest(pt) ? undoButton_ : nullptr;
            consumedDown_ = pressed_ != nullptr;
            if (pressed_) pressed_->OnLButtonDown(flags,pt);
            return consumedDown_;
        }
        // Always deliver a game-originating release to the game, even if the
        // cursor has since moved over one of our buttons.
        if (!consumedDown_) return false;
        consumedDown_ = false;
        auto* pressed = pressed_; pressed_ = nullptr;
        if (pressed) {
            pressed->OnLButtonUp(flags,pt);
            static_cast<TButton*>(pressed)->Select(FALSE);
        }
        if (pressed && IsVisible() && timer_.active() && pressed->HitTest(pt)) {
            if (pressed == undoButton_) undo_ = true;
            else keep_ = true;
        }
        return true;
    }
};
DisplayPreview* find(CTClientGame* game) {
    return game ? static_cast<DisplayPreview*>(game->FindKid(kPreviewId)) : nullptr;
}
}
void fourstory_display_preview_attach(CTClientGame* game, FRAMEDESC_SHAREDPTR desc) {
    if (game && desc && !find(game)) game->AddKid(new DisplayPreview(game, desc));
}
void fourstory_display_preview_begin(CTClientGame* game) { if (auto* preview = find(game)) preview->begin(); }
void fourstory_display_preview_tick(CTClientGame* game) { if (auto* preview = find(game)) preview->tick(); }

extern "C" void fourstory_display_mode_changed(HWND window, int fullscreen) {
    if (livePreview) livePreview->nativeModeChanged(window, fullscreen != 0);
}

bool fourstory_display_preview_click(CTClientGame* game, UINT flags, CPoint point, bool down) {
    auto* preview = find(game);
    return preview && preview->click(flags,point,down);
}

extern "C" void fourstory_display_focus_resumed(HWND window) {
    if (unfocusedWindow == window) unfocusedWindow = nullptr;
    if (livePreview) livePreview->focusResumed(window);
}

extern "C" void fourstory_display_focus_lost(HWND window) {
    unfocusedWindow = window;
}
void fourstory_display_background_shade(CTClientWnd* window) {
    if (!window || window->GetSafeHwnd() != unfocusedWindow) return;
    // Runs after the full game/login UI, including when the HUD is hidden.
    // Only darken the image; never add text or change input/modal ownership.
    drawShade(window->m_Device.m_pDevice,
        window->m_Device.m_option.m_dwScreenX,window->m_Device.m_option.m_dwScreenY);
}
