// Native-only addition: a header for the on-screen quest tracker (the gauge
// pannel's quest list). The header hides or shows the list, and while the
// game's UI lock is off the tracker drags like the other CTClientUIBase frames.
#include "StdAfx.h"
#include "TClient.h"
#include "TClientGame.h"
#include "TChatFrame.h"
#include <fourstory_quest_tracker_ui.h>
#include <algorithm>
namespace {
constexpr DWORD kTrackerId=0xf5010003,kLabelId=0xf5010004,kArrowId[2]={0xf5010005,0xf5010006};
const char* kKey="QuestTracker";
const char* kHiddenKey="QuestTrackerHidden";
const DWORD kLabelColor=0xFFD9C298;   // objective text color in CTGaugePannel::UpdateQuestINFO
class QuestTracker;
QuestTracker* liveTracker=nullptr;

class QuestTracker : public CTClientUIBase {
    TList* list_;
    TComponent* label_=nullptr;
    TButton* arrows_[2]={nullptr,nullptr};   // 0: up (list shown), 1: down (list hidden)
    bool hidden_=false,pressed_=false,hover_=false,custom_=false;
    CPoint pressPos_;
    int header_=18,arrowW_=0,textW_=-1;

    CGDIFont* listFont() const {
        if(list_->m_pGDIFont)return list_->m_pGDIFont;
        for(auto* holder:list_->m_itemholders)
            if(holder)for(auto* column:*holder)
                if(column && column->m_pGDIFont)return column->m_pGDIFont;
        return nullptr;
    }
    // The original chat scroll bar arrows; the up arrow is the down arrow turned 180 degrees.
    void loadArrows() {
        if(arrows_[0])return;
        auto* game=CTClientGame::GetInstance();
        auto* chat=game?game->GetChatFrame():nullptr;
        if(!chat)return;
        for(auto* chatList:chat->m_vListArray) {
            TComponent* scroll=chatList?chatList->m_pScroll:nullptr;
            if(!scroll || !scroll->IsTypeOf(TCML_TYPE_SCROLL))continue;
            TComponent* source[2]={static_cast<TScroll*>(scroll)->m_pUpLeft,static_cast<TScroll*>(scroll)->m_pDownRight};
            if(!source[0] || !source[1] || !source[0]->IsTypeOf(TCML_TYPE_BUTTON) || !source[1]->IsTypeOf(TCML_TYPE_BUTTON))continue;
            for(int i=0;i<2;++i) {
                auto* arrow=new TButton(this,*static_cast<TButton*>(source[i]));
                arrow->m_id=kArrowId[i];arrow->m_bNoHIT=TRUE;arrow->m_bSND=FALSE;
                arrow->m_strTooltip.Empty();
                AddKid(arrow);arrows_[i]=arrow;
            }
            return;
        }
    }
    int contentHeight() const {
        int rows=std::min(list_->GetItemCount()-list_->m_nTop,list_->m_nItemPerPage),bottom=0;
        for(int i=0;i<rows && i<int(list_->m_itemholders.size());++i)
            if(auto* holder=list_->m_itemholders[i])
                for(auto* column:*holder)if(column)bottom=std::max<int>(bottom,column->m_rc.bottom);
        return bottom;
    }
    void layout() {
        loadArrows();
        CSize arrow=arrows_[0]?arrows_[0]->m_rc.Size():CSize(0,0);
        int header=std::max<int>(18,arrow.cy+2);
        arrowW_=arrow.cx;
        int width=list_->m_rc.Width();
        for(auto* button:arrows_)if(button)button->MoveComponent(CPoint(width-arrow.cx,(header-arrow.cy)/2));
        label_->m_rc=CRect(0,0,std::max(0,width-arrow.cx-4),header);
        m_rc.right=m_rc.left+width;
        m_rc.bottom=m_rc.top+header+(hidden_?0:contentHeight());
        if(header!=header_){header_=header;placeList();}
    }
    void placeList(){list_->MoveComponent(CPoint(m_rc.left,m_rc.top+header_));}
    bool headerHit(CPoint pt) {
        if(!m_bVisible)return false;
        CRect rc(m_rc.left,m_rc.top,m_rc.right,m_rc.top+header_);
        ComponentToScreen(&rc);
        if(!rc.PtInRect(pt))return false;
        // Measured once per label text, not on every mouse move.
        if(textW_<0){CSize text(0,0);label_->GetTextExtentPoint(labelText(),text);textW_=text.cx;}
        return pt.x>=rc.right-arrowW_-4-textW_-4;
    }
    const char* labelText() const {return hidden_?"Show quests":"Hide quests";}
    void setHidden(bool hidden) {
        hidden_=hidden;textW_=-1;list_->ShowComponent(!hidden_);
    }
    void toggle() {
        setHidden(!hidden_);
        theApp.WriteProfileInt(TREG_CUSTOMUI,kHiddenKey,hidden_?1:0);
    }
public:
    QuestTracker(TComponent* pannel,TList* list):CTClientUIBase(pannel,FRAMEDESC_SHAREDPTR()),list_(list) {
        m_id=kTrackerId;m_bCanDrag=TRUE;m_bDropTarget=FALSE;m_bSND=FALSE;
        m_bHitTestType=HITTEST_TYPE_RECT;
        label_=new TComponent(this,*list,false);
        label_->m_id=kLabelId;label_->m_bType=TCML_TYPE_COMPONENT;label_->m_bNoHIT=TRUE;
        label_->m_pEnable=label_->m_pDisable=nullptr;label_->m_strTooltip.Empty();
        label_->m_flagAlign=ALIGN_RIGHT;
        if(!label_->m_pGDIFont)label_->SetFont(listFont(),nullptr);
        label_->SetTextClr(kLabelColor);
        AddKid(label_);
        m_rc=CRect(list->m_rc.left,list->m_rc.top-header_,list->m_rc.right,list->m_rc.top);
        liveTracker=this;
    }
    ~QuestTracker() override {if(liveTracker==this)liveTracker=nullptr;}
    void reset() {
        m_rcDrag=CRect(CPoint(0,0),m_vBasis[TBASISPOINT_RIGHT_BOTTOM]);
        if(!custom_) {
            // Default: directly above the list's original position.
            m_bBasisPoint=TBASISPOINT_RIGHT_MIDDLE;
            m_vCompOffset=CPoint(list_->m_rc.left,list_->m_rc.top-header_)-m_vBasis[TBASISPOINT_RIGHT_MIDDLE];
        }
        ResetPosition();
    }
    void save() {
        theApp.WriteProfileInt(TREG_CUSTOMUI,CString(kKey)+TREG_CUSTOMUI_POS_X,m_vCompOffset.x);
        theApp.WriteProfileInt(TREG_CUSTOMUI,CString(kKey)+TREG_CUSTOMUI_POS_Y,m_vCompOffset.y);
        theApp.WriteProfileInt(TREG_CUSTOMUI,CString(kKey)+TREG_CUSTOMUI_BASIS,custom_?m_bBasisPoint:0xFF);
        theApp.WriteProfileInt(TREG_CUSTOMUI,kHiddenKey,hidden_?1:0);
    }
    void load() {
        CPoint offset(theApp.GetProfileInt(TREG_CUSTOMUI,CString(kKey)+TREG_CUSTOMUI_POS_X,T_INVALID),
                      theApp.GetProfileInt(TREG_CUSTOMUI,CString(kKey)+TREG_CUSTOMUI_POS_Y,T_INVALID));
        UINT basis=theApp.GetProfileInt(TREG_CUSTOMUI,CString(kKey)+TREG_CUSTOMUI_BASIS,0xFF);
        if(offset.x!=T_INVALID && offset.y!=T_INVALID && basis<TBASISPOINT_COUNT) {
            custom_=true;m_bBasisPoint=BYTE(basis);m_vCompOffset=offset;ResetPosition();
        }
        setHidden(theApp.GetProfileInt(TREG_CUSTOMUI,kHiddenKey,0)!=0);
    }
    void restoreDefault(){custom_=false;setHidden(false);}
    void MoveComponent(CPoint pt) override {CTClientUIBase::MoveComponent(pt);placeList();}
    BOOL HitTest(CPoint pt) override {
        if(!m_bVisible)return FALSE;
        CRect rc=m_rc;ComponentToScreen(&rc);
        if(!rc.PtInRect(pt))return FALSE;
        // Unlocked UI: the whole tracker is a drag handle, as for the gauges.
        return !m_bDragLock || headerHit(pt);
    }
    void OnLButtonDown(UINT flags,CPoint pt) override {
        pressed_=headerHit(pt);pressPos_=m_rc.TopLeft();
        CTClientUIBase::OnLButtonDown(flags,pt);
    }
    void OnLButtonUp(UINT flags,CPoint pt) override {
        bool dragged=m_bDraging!=FALSE,moved=m_rc.TopLeft()!=pressPos_;
        CTClientUIBase::OnLButtonUp(flags,pt);
        if(dragged && moved)custom_=true;
        if(pressed_ && !moved && headerHit(pt))toggle();
        pressed_=false;
    }
    void OnMouseMove(UINT flags,CPoint pt) override {
        CTClientUIBase::OnMouseMove(flags,pt);
        hover_=headerHit(pt);
    }
    HRESULT Render(DWORD tick) override {
        // Showing the pannel shows every kid, the hidden list included.
        if((list_->IsVisible()!=FALSE)==hidden_)list_->ShowComponent(!hidden_);
        m_bVisible=list_->GetItemCount()>0;
        if(!m_bVisible || !m_pParent || !m_pParent->IsVisible())return S_OK;
        layout();
        label_->m_strText=labelText();
        for(int i=0;i<2;++i)if(arrows_[i]) {
            bool active=(i==1)==hidden_;
            arrows_[i]->ShowComponent(active);
            arrows_[i]->m_bState=!active?TBUTTON_STATE_NULL:pressed_ && hover_?TBUTTON_STATE_DOWN:hover_?TBUTTON_STATE_HOVER:TBUTTON_STATE_NULL;
        }
        return CTClientUIBase::Render(tick);
    }
};
}
void fourstory_quest_tracker_attach(TComponent* pannel,TList* list) {
    if(!pannel || !list)return;
    pannel->AddKid(new QuestTracker(pannel,list));
}
void fourstory_quest_tracker_reset(){if(liveTracker)liveTracker->reset();}
void fourstory_quest_tracker_restore_default(){if(liveTracker)liveTracker->restoreDefault();}
void fourstory_quest_tracker_save(){if(liveTracker)liveTracker->save();}
void fourstory_quest_tracker_load(){if(liveTracker)liveTracker->load();}
