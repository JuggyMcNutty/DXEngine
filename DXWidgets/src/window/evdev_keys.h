#pragma once

#include <dxwidgets/window/window.h>
#include <linux/input-event-codes.h>
#include <cstdint>

// A key's name in the X11 and Wayland windows, from its Linux evdev code
// (an X key code less 8), as the SDL3 window names it from SDL's scancode
// (SDL3DisplayWindow::KeyEventToInputKey, LayoutKeyName and
// ScancodeToInputKey, which this follows key for key): the Windows
// virtual-key code the original's key message carries, which is EInputKey's
// (dx-reverse-info/windrv-dll.md, the keys). The keypad's digits and point
// by Num Lock; a typing key as the original's Wine names it, by its place
// on a QWERTY or by its layout family's table (Wine's winex11.drv
// keyboard.c, main_key_vkey_*), the family told by the keysyms a few keys
// have at their first level in the layout in force, which levelZero gives
// (a Latin-1 keysym is its character's code); any other key by its place
// on a US keyboard. No name is InputKey{}, X11's None macro hiding InputKey::None.

// A key by its place on a US keyboard, as Windows' virtual-key code names it
// there; the ISO key by the left Shift and an ABNT or JIS keyboard's own
// keys as their places' codes (VK_OEM_102, VK_OEM_5)
inline InputKey EvdevKeyPlace(uint32_t code)
{
	switch (code)
	{
	case KEY_ESC:        return InputKey::Escape;
	case KEY_1:          return InputKey::_1;
	case KEY_2:          return InputKey::_2;
	case KEY_3:          return InputKey::_3;
	case KEY_4:          return InputKey::_4;
	case KEY_5:          return InputKey::_5;
	case KEY_6:          return InputKey::_6;
	case KEY_7:          return InputKey::_7;
	case KEY_8:          return InputKey::_8;
	case KEY_9:          return InputKey::_9;
	case KEY_0:          return InputKey::_0;
	case KEY_MINUS:      return InputKey::Minus;
	case KEY_EQUAL:      return InputKey::Equals;
	case KEY_BACKSPACE:  return InputKey::Backspace;
	case KEY_TAB:        return InputKey::Tab;
	case KEY_Q:          return InputKey::Q;
	case KEY_W:          return InputKey::W;
	case KEY_E:          return InputKey::E;
	case KEY_R:          return InputKey::R;
	case KEY_T:          return InputKey::T;
	case KEY_Y:          return InputKey::Y;
	case KEY_U:          return InputKey::U;
	case KEY_I:          return InputKey::I;
	case KEY_O:          return InputKey::O;
	case KEY_P:          return InputKey::P;
	case KEY_LEFTBRACE:  return InputKey::LeftBracket;
	case KEY_RIGHTBRACE: return InputKey::RightBracket;
	case KEY_ENTER:      return InputKey::Enter;
	case KEY_LEFTCTRL:   return InputKey::Ctrl;
	case KEY_A:          return InputKey::A;
	case KEY_S:          return InputKey::S;
	case KEY_D:          return InputKey::D;
	case KEY_F:          return InputKey::F;
	case KEY_G:          return InputKey::G;
	case KEY_H:          return InputKey::H;
	case KEY_J:          return InputKey::J;
	case KEY_K:          return InputKey::K;
	case KEY_L:          return InputKey::L;
	case KEY_SEMICOLON:  return InputKey::Semicolon;
	case KEY_APOSTROPHE: return InputKey::SingleQuote;
	case KEY_GRAVE:      return InputKey::Tilde;
	case KEY_LEFTSHIFT:  return InputKey::Shift;
	case KEY_BACKSLASH:  return InputKey::Backslash;
	case KEY_Z:          return InputKey::Z;
	case KEY_X:          return InputKey::X;
	case KEY_C:          return InputKey::C;
	case KEY_V:          return InputKey::V;
	case KEY_B:          return InputKey::B;
	case KEY_N:          return InputKey::N;
	case KEY_M:          return InputKey::M;
	case KEY_COMMA:      return InputKey::Comma;
	case KEY_DOT:        return InputKey::Period;
	case KEY_SLASH:      return InputKey::Slash;
	case KEY_RIGHTSHIFT: return InputKey::Shift;
	case KEY_KPASTERISK: return InputKey::GreyStar;
	case KEY_LEFTALT:    return InputKey::Alt;
	case KEY_SPACE:      return InputKey::Space;
	case KEY_CAPSLOCK:   return InputKey::CapsLock;
	case KEY_F1:         return InputKey::F1;
	case KEY_F2:         return InputKey::F2;
	case KEY_F3:         return InputKey::F3;
	case KEY_F4:         return InputKey::F4;
	case KEY_F5:         return InputKey::F5;
	case KEY_F6:         return InputKey::F6;
	case KEY_F7:         return InputKey::F7;
	case KEY_F8:         return InputKey::F8;
	case KEY_F9:         return InputKey::F9;
	case KEY_F10:        return InputKey::F10;
	case KEY_NUMLOCK:    return InputKey::NumLock;
	case KEY_SCROLLLOCK: return InputKey::ScrollLock;
	case KEY_KP7:        return InputKey::NumPad7;
	case KEY_KP8:        return InputKey::NumPad8;
	case KEY_KP9:        return InputKey::NumPad9;
	case KEY_KPMINUS:    return InputKey::GreyMinus;
	case KEY_KP4:        return InputKey::NumPad4;
	case KEY_KP5:        return InputKey::NumPad5;
	case KEY_KP6:        return InputKey::NumPad6;
	case KEY_KPPLUS:     return InputKey::GreyPlus;
	case KEY_KP1:        return InputKey::NumPad1;
	case KEY_KP2:        return InputKey::NumPad2;
	case KEY_KP3:        return InputKey::NumPad3;
	case KEY_KP0:        return InputKey::NumPad0;
	case KEY_KPDOT:      return InputKey::NumPadPeriod;
	case KEY_102ND:      return InputKey::JoyZ;      // VK_OEM_102
	case KEY_F11:        return InputKey::F11;
	case KEY_F12:        return InputKey::F12;
	case KEY_RO:         return InputKey::JoyZ;      // VK_OEM_102: ABNT's /?, JIS's ro
	case KEY_KPENTER:    return InputKey::Enter;
	case KEY_RIGHTCTRL:  return InputKey::Ctrl;
	case KEY_KPSLASH:    return InputKey::GreySlash;
	case KEY_SYSRQ:
	case KEY_PRINT:      return InputKey::PrintScrn; // VK_SNAPSHOT
	case KEY_RIGHTALT:   return InputKey::Alt;
	case KEY_HOME:       return InputKey::Home;
	case KEY_UP:         return InputKey::Up;
	case KEY_PAGEUP:     return InputKey::PageUp;
	case KEY_LEFT:       return InputKey::Left;
	case KEY_RIGHT:      return InputKey::Right;
	case KEY_END:        return InputKey::End;
	case KEY_DOWN:       return InputKey::Down;
	case KEY_PAGEDOWN:   return InputKey::PageDown;
	case KEY_INSERT:     return InputKey::Insert;
	case KEY_DELETE:     return InputKey::Delete;
	case KEY_PAUSE:      return InputKey::Pause;
	case KEY_YEN:        return InputKey::Backslash; // VK_OEM_5: JIS's yen
	case KEY_LEFTMETA:   return InputKey::Unknown5B; // VK_LWIN
	case KEY_RIGHTMETA:  return InputKey::Unknown5C; // VK_RWIN
	case KEY_COMPOSE:
	case KEY_MENU:       return InputKey::Unknown5D; // VK_APPS
	case KEY_HELP:       return InputKey::Help;
	case KEY_SELECT:     return InputKey::Select;
	case KEY_CLEAR:      return InputKey::OEMClear;
	case KEY_F13:        return InputKey::F13;
	case KEY_F14:        return InputKey::F14;
	case KEY_F15:        return InputKey::F15;
	case KEY_F16:        return InputKey::F16;
	case KEY_F17:        return InputKey::F17;
	case KEY_F18:        return InputKey::F18;
	case KEY_F19:        return InputKey::F19;
	case KEY_F20:        return InputKey::F20;
	case KEY_F21:        return InputKey::F21;
	case KEY_F22:        return InputKey::F22;
	case KEY_F23:        return InputKey::F23;
	case KEY_F24:        return InputKey::F24;
	default:             return InputKey{};
	}
}

template<typename LevelZero>
InputKey EvdevKeyName(uint32_t code, bool numLock, const LevelZero& levelZero)
{
	if (!numLock)
	{
		switch (code)
		{
		case KEY_KP0:   return InputKey::Insert;
		case KEY_KP1:   return InputKey::End;
		case KEY_KP2:   return InputKey::Down;
		case KEY_KP3:   return InputKey::PageDown;
		case KEY_KP4:   return InputKey::Left;
		case KEY_KP5:   return InputKey::Unknown0C; // VK_CLEAR
		case KEY_KP6:   return InputKey::Right;
		case KEY_KP7:   return InputKey::Home;
		case KEY_KP8:   return InputKey::Up;
		case KEY_KP9:   return InputKey::PageUp;
		case KEY_KPDOT: return InputKey::Delete;
		default: break;
		}
	}

	InputKey place = EvdevKeyPlace(code);
	bool typing = (place >= InputKey::_0 && place <= InputKey::_9) || (place >= InputKey::A && place <= InputKey::Z) ||
		(place >= InputKey::Semicolon && place <= InputKey::Tilde) || (place >= InputKey::LeftBracket && place <= InputKey::SingleQuote) ||
		place == InputKey::JoyZ;
	if (!typing)
		return place;

	auto at = [&](uint32_t key) { return (uint32_t)levelZero(key); };
	enum { QWERTY, AZERTY, QWERTZ, Dvorak, Swedish, ABNT, JP106 } family = QWERTY;
	if (at(KEY_Q) == 'a' && at(KEY_W) == 'z')
		family = AZERTY;
	else if (at(KEY_Y) == 'z' && at(KEY_Z) == 'y')
		family = QWERTZ;
	else if (at(KEY_S) == 'o' && at(KEY_D) == 'e')
		family = Dvorak;
	else if (at(KEY_LEFTBRACE) == 0xe5 && at(KEY_SEMICOLON) == 0xf6 && at(KEY_APOSTROPHE) == 0xe4)
		family = Swedish;
	else if (at(KEY_SEMICOLON) == 0xe7 && at(KEY_RIGHTBRACE) == '[')
		family = ABNT;
	else if (at(KEY_LEFTBRACE) == '@' && at(KEY_APOSTROPHE) == ':')
		family = JP106;

	switch (family)
	{
	case QWERTZ:
		switch (code)
		{
		case KEY_Y:          return InputKey::Z;
		case KEY_Z:          return InputKey::Y;
		default: break;
		}
		break;
	case AZERTY:
		switch (code)
		{
		case KEY_GRAVE:      return InputKey::SingleQuote;  // VK_OEM_7
		case KEY_MINUS:      return InputKey::LeftBracket;  // VK_OEM_4
		case KEY_Q:          return InputKey::A;
		case KEY_W:          return InputKey::Z;
		case KEY_LEFTBRACE:  return InputKey::RightBracket; // VK_OEM_6
		case KEY_RIGHTBRACE: return InputKey::Semicolon;    // VK_OEM_1
		case KEY_A:          return InputKey::Q;
		case KEY_SEMICOLON:  return InputKey::M;
		case KEY_APOSTROPHE: return InputKey::Tilde;        // VK_OEM_3
		case KEY_Z:          return InputKey::W;
		case KEY_M:          return InputKey::Comma;
		case KEY_COMMA:      return InputKey::Period;
		case KEY_DOT:        return InputKey::Slash;        // VK_OEM_2
		case KEY_SLASH:      return InputKey::UnknownDF;    // VK_OEM_8
		default: break;
		}
		break;
	case Swedish:
		switch (code)
		{
		case KEY_GRAVE:      return InputKey::Backslash;    // VK_OEM_5
		case KEY_MINUS:      return InputKey::Equals;       // VK_OEM_PLUS
		case KEY_EQUAL:      return InputKey::LeftBracket;  // VK_OEM_4
		case KEY_LEFTBRACE:  return InputKey::RightBracket; // VK_OEM_6
		case KEY_RIGHTBRACE: return InputKey::Semicolon;    // VK_OEM_1
		case KEY_SEMICOLON:  return InputKey::Tilde;        // VK_OEM_3
		case KEY_BACKSLASH:  return InputKey::Slash;        // VK_OEM_2
		case KEY_SLASH:      return InputKey::Minus;
		default: break;
		}
		break;
	case ABNT:
		switch (code)
		{
		case KEY_APOSTROPHE: return InputKey::UnknownDF;    // VK_OEM_8
		case KEY_102ND:      return InputKey::SingleQuote;  // VK_OEM_7
		default: break;
		}
		break;
	case JP106:
		switch (code)
		{
		case KEY_GRAVE:      return InputKey::JoyPovRight;  // Hankaku/Zenkaku, VK_DBE_SBCSCHAR
		case KEY_EQUAL:      return InputKey::SingleQuote;  // VK_OEM_7
		case KEY_LEFTBRACE:  return InputKey::Tilde;        // VK_OEM_3
		case KEY_RIGHTBRACE: return InputKey::LeftBracket;  // VK_OEM_4
		case KEY_SEMICOLON:  return InputKey::Equals;       // VK_OEM_PLUS
		case KEY_APOSTROPHE: return InputKey::Semicolon;    // VK_OEM_1
		case KEY_BACKSLASH:  return InputKey::RightBracket; // VK_OEM_6
		default: break;
		}
		break;
	case Dvorak:
		switch (code)
		{
		case KEY_MINUS:      return InputKey::LeftBracket;
		case KEY_EQUAL:      return InputKey::RightBracket;
		case KEY_Q:          return InputKey::SingleQuote;
		case KEY_W:          return InputKey::Comma;
		case KEY_E:          return InputKey::Period;
		case KEY_R:          return InputKey::P;
		case KEY_T:          return InputKey::Y;
		case KEY_Y:          return InputKey::F;
		case KEY_U:          return InputKey::G;
		case KEY_I:          return InputKey::C;
		case KEY_O:          return InputKey::R;
		case KEY_P:          return InputKey::L;
		case KEY_LEFTBRACE:  return InputKey::Slash;
		case KEY_RIGHTBRACE: return InputKey::Equals;
		case KEY_S:          return InputKey::O;
		case KEY_D:          return InputKey::E;
		case KEY_F:          return InputKey::U;
		case KEY_G:          return InputKey::I;
		case KEY_H:          return InputKey::D;
		case KEY_J:          return InputKey::H;
		case KEY_K:          return InputKey::T;
		case KEY_L:          return InputKey::N;
		case KEY_SEMICOLON:  return InputKey::S;
		case KEY_APOSTROPHE: return InputKey::Minus;
		case KEY_Z:          return InputKey::Semicolon;
		case KEY_X:          return InputKey::Q;
		case KEY_C:          return InputKey::J;
		case KEY_V:          return InputKey::K;
		case KEY_B:          return InputKey::X;
		case KEY_N:          return InputKey::B;
		case KEY_COMMA:      return InputKey::W;
		case KEY_DOT:        return InputKey::V;
		case KEY_SLASH:      return InputKey::Z;
		default: break;
		}
		break;
	default:
		break;
	}
	return place;
}
