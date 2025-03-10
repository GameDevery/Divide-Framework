

#include "Headers/AutoKeyRepeat.h"

namespace Divide {
namespace Input {

AutoRepeatKey::AutoRepeatKey(const D64 repeatDelay, const D64 initialDelay) noexcept
    : _key(nullptr, 0), 
      _elapsed(0.0),
      _delay(initialDelay),
      _repeatDelay(repeatDelay),
      _initialDelay(initialDelay)
{
}

void AutoRepeatKey::begin(const KeyEvent &evt) noexcept {
    _key = evt;
    _elapsed = 0.0;
    _delay = _initialDelay;
}

void AutoRepeatKey::end( [[maybe_unused]] const KeyEvent &evt ) noexcept
{
    _key._key = KeyCode::KC_UNASSIGNED;
}

// Inject key repeats if the _repeatDelay expired between calls
void AutoRepeatKey::update(const U64 deltaTimeUS) {
    if (_key._key == KeyCode::KC_UNASSIGNED) {
        return;
    }

    _elapsed += Time::MicrosecondsToSeconds(deltaTimeUS);
    if (_elapsed < _delay) return;

    _elapsed -= _delay;
    _delay = _repeatDelay;

    do {
        repeatKey(_key);
        _elapsed -= _repeatDelay;
    } while (_elapsed >= _repeatDelay);

    _elapsed = 0.0;
}

};  // namespace Input
};  // namespace Divide
