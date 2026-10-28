#pragma once
class TComponent;
class TList;
// Native-only quest tracker header: hide/show button and a UI-lock drag frame.
void fourstory_quest_tracker_attach(TComponent* pannel,TList* list);
// Call after the gauge pannel has moved the list to its default position.
void fourstory_quest_tracker_reset();
// The game's UI reset: forget the dragged position and show the list again.
void fourstory_quest_tracker_restore_default();
// Profile storage beside the original CustomUI positions.
void fourstory_quest_tracker_save();
void fourstory_quest_tracker_load();
