#pragma once

enum am_state { am_inactive, am_active, am_timing_out };

void activity_monitor_start();
void activity_monitor_stop();
void activity_monitor_signify_activity(int active); // 0 means inactive, non-zero means active
enum am_state activity_status(); // true if non inactive; false if inactive
