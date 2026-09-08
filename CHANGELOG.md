# Changelog

## 2026-09-07 - v1.0.0
- fix(ui): slow touch repeat rate for held inputs
- fix(eeprom): initialize fresh-board defaults for temp delay and min temp
- fix(sensor): only write temp min/max when values actually change
- fix(ui): stop status screen from redrawing unchanged min/max values
- refactor(ui): simplify touch detection and state handling
- fix(cycle): separate manual cycle state from main auto-cycle state
- fix(actuator): improve abort/shake sequencing
- refactor: modularize firmware structure and split responsibilities
- fix: improve touch handling and button hit detection
- fix: decouple manual punch logic from scheduler state
- fix: initialize persistent settings for new boards
- fix: ensure held touch input does not re-trigger too quickly
