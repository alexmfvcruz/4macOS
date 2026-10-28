#pragma once
class CTClientWnd;
class TComponent;
namespace fourstory { namespace accounts {
void open(CTClientWnd* window);
void logged_out();
void render(CTClientWnd* window, unsigned tick);
bool input(unsigned kind, unsigned key, unsigned repeat, unsigned flags, int x=0, int y=0);
void shutdown();
}}
