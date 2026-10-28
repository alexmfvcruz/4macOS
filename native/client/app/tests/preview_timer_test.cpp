#include <fourstory_preview_timer.h>
#include <cassert>
#include <cstdio>
using fourstory::PreviewTimer;
int main() {
    PreviewTimer t;
    t.start(100, false);
    assert(t.remaining(100)==8 && t.remaining(1100)==7);
    assert(t.update(8099,false)==PreviewTimer::Waiting);
    assert(t.update(8100,false)==PreviewTimer::Keep && !t.active());
    t.start(0,true); // Control was held when the popup appeared.
    assert(t.update(100,true)==PreviewTimer::Waiting);
    assert(t.update(200,false)==PreviewTimer::Waiting);
    assert(t.update(300,true)==PreviewTimer::Undo);
    t.start(0,false);
    assert(t.update(8000,true)==PreviewTimer::Keep); // Deadline wins.
    t.start(0xfffffff0u,false); // Tick count wraps during preview.
    assert(t.remaining(984)==7);
    assert(t.update(7984,false)==PreviewTimer::Keep);
    t.start(0,false); t.stop();
    assert(t.update(10,true)==PreviewTimer::Waiting);
    std::puts("PASS preview timer: timeout, undo, held key, deadline, wrap, cancellation");
}
