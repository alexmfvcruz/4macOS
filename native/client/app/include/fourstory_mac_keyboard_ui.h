#pragma once
class TComponent;
class CPoint;
void fourstory_keyboard_attach(TComponent* parent,TComponent** buttons,CPoint* defaults,int count);
class CRect;
class CString;
bool fourstory_keyboard_tooltip(TComponent* parent,const CPoint& point,CString& text,CRect& rect);
