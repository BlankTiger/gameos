#pragma once

#include "kstd/enum_array.hh"
#include "kstd/ring_buffer.hh"
#include "kstd/synchronization.hh"

namespace input {

//
// Values identify physical keys. Backends translate their own key codes to
// this enum before submitting events.
//
enum struct Key : u16 {
    UNKNOWN,

    DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5,
    DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9, DIGIT_0,

    Q, W, E, R, T, Y, U, I, O, P,
    A, S, D, F, G, H, J, K, L,
    Z, X, C, V, B, N, M,

    F1, F2, F3, F4,  F5,  F6,
    F7, F8, F9, F10, F11, F12,

    MINUS, EQUALS,

    ESCAPE, BACKSPACE,
    ENTER, SPACE, TAB,

    SEMICOLON, APOSTROPHE, GRAVE,
    SLASH, BACKSLASH,
    COMMA, PERIOD,

    LEFT_BRACKET, RIGHT_BRACKET,
    LEFT_SHIFT,   RIGHT_SHIFT,
    LEFT_ALT,     RIGHT_ALT,
    LEFT_CONTROL, RIGHT_CONTROL,

    CAPS_LOCK, NUM_LOCK, SCROLL_LOCK,

    KEYPAD_0,
    KEYPAD_1, KEYPAD_2, KEYPAD_3,
    KEYPAD_4, KEYPAD_5, KEYPAD_6,
    KEYPAD_7, KEYPAD_8, KEYPAD_9,
    KEYPAD_MULTIPLY, KEYPAD_PERIOD,
    KEYPAD_MINUS,    KEYPAD_PLUS,

    RIGHT_ARROW, LEFT_ARROW,
    UP_ARROW,    DOWN_ARROW,

    COUNT,
};

enum struct Mouse_Button : u8 {
    LEFT,
    MIDDLE,
    RIGHT,
    X1,
    X2,
    COUNT,
};

enum struct Event_Type : u8 {
    KEY_DOWN,
    KEY_UP,
    MOUSE_MOTION,
    MOUSE_BUTTON_DOWN,
    MOUSE_BUTTON_UP,
    MOUSE_WHEEL,
};

struct Event_Key {
    Key  key;
    bool repeat;
};

struct Event_Mouse_Motion {
    s32 xrel;
    s32 yrel;
};

struct Event_Mouse_Button {
    Mouse_Button button;
};

struct Event_Mouse_Wheel {
    s32 x;
    s32 y;
};

struct Event {
    Event_Type type;

    union {
        Event_Key          key_down;
        Event_Key          key_up;
        Event_Mouse_Motion mouse_motion;
        Event_Mouse_Button mouse_button_down;
        Event_Mouse_Button mouse_button_up;
        Event_Mouse_Wheel  mouse_wheel;
    };
};

struct Mouse_Delta {
    s32 x;
    s32 y;
};

struct Poll_Result {
    Event event;
    bool  is_present;
};

namespace hidden {

constexpr s64 EVENT_QUEUE_CAPACITY = 256;

inline Ring_Buffer<Event, EVENT_QUEUE_CAPACITY> events;
inline synchronization::Spinlock                lock;

inline Enum_Array<Key, bool> keys;
inline Enum_Array<Key, bool> next_frame_pressed;
inline Enum_Array<Key, bool> next_frame_released;
inline Enum_Array<Key, bool> next_frame_repeated;
inline Enum_Array<Key, bool> frame_pressed;
inline Enum_Array<Key, bool> frame_released;
inline Enum_Array<Key, bool> frame_repeated;

inline Enum_Array<Mouse_Button, bool> mouse_buttons;
inline Enum_Array<Mouse_Button, bool> next_frame_button_pressed;
inline Enum_Array<Mouse_Button, bool> next_frame_button_released;
inline Enum_Array<Mouse_Button, bool> frame_button_pressed;
inline Enum_Array<Mouse_Button, bool> frame_button_released;

inline Mouse_Delta next_frame_motion{};
inline Mouse_Delta frame_motion{};
inline Mouse_Delta next_frame_wheel{};
inline Mouse_Delta frame_wheel{};
inline u64         dropped_event_count = 0;

auto submit_key(Event_Key key_event, bool is_down) -> void {
    if (key_event.key == Key::UNKNOWN) return;

    const bool was_down = keys[key_event.key];
    keys[key_event.key] = is_down;

    if (is_down) {
        if (was_down) {
            if (key_event.repeat) next_frame_repeated[key_event.key] = true;
        } else {
            next_frame_pressed[key_event.key] = true;
        }
    } else if (was_down) {
        next_frame_released[key_event.key] = true;
    }
}

auto submit_mouse_button(Mouse_Button button, bool is_down) -> void {
    const bool was_down = mouse_buttons[button];
    mouse_buttons[button] = is_down;

    if (is_down && !was_down)
        next_frame_button_pressed[button] = true;
    else if (!is_down && was_down)
        next_frame_button_released[button] = true;
}

auto clear_state() -> void {
    events.clear();
    clear_enum_array(keys,                       false);
    clear_enum_array(next_frame_pressed,         false);
    clear_enum_array(next_frame_released,        false);
    clear_enum_array(next_frame_repeated,        false);
    clear_enum_array(frame_pressed,              false);
    clear_enum_array(frame_released,             false);
    clear_enum_array(frame_repeated,             false);
    clear_enum_array(mouse_buttons,              false);
    clear_enum_array(next_frame_button_pressed,  false);
    clear_enum_array(next_frame_button_released, false);
    clear_enum_array(frame_button_pressed,       false);
    clear_enum_array(frame_button_released,      false);
    next_frame_motion = {};
    frame_motion      = {};
    next_frame_wheel  = {};
    frame_wheel       = {};
    dropped_event_count = 0;
}

}

// Called by device backends, including interrupt handlers. State is updated
// even when queue is full so a dropped event cannot leave a key held forever.
auto submit_event(Event event) -> void {
    using namespace hidden;

    auto guard = lock.scoped_irq_lock();

    using enum Event_Type;
    switch (event.type) {
        case KEY_DOWN: submit_key(event.key_down, true);  break;
        case KEY_UP:   submit_key(event.key_up,   false); break;

        case MOUSE_MOTION: {
            next_frame_motion.x += event.mouse_motion.xrel;
            next_frame_motion.y += event.mouse_motion.yrel;
        } break;
        case MOUSE_WHEEL: {
            next_frame_wheel.x += event.mouse_wheel.x;
            next_frame_wheel.y += event.mouse_wheel.y;
        } break;

        case MOUSE_BUTTON_DOWN: submit_mouse_button(event.mouse_button_down.button, true);  break;
        case MOUSE_BUTTON_UP:   submit_mouse_button(event.mouse_button_up.button,   false); break;
    }

    if (events.full()) {
        ++dropped_event_count;
    } else {
        events.push_back(event);
    }
}

auto begin_frame() -> void {
    using namespace hidden;

    auto guard = lock.scoped_irq_lock();

    frame_pressed         = next_frame_pressed;
    frame_released        = next_frame_released;
    frame_repeated        = next_frame_repeated;
    frame_button_pressed  = next_frame_button_pressed;
    frame_button_released = next_frame_button_released;
    frame_motion          = next_frame_motion;
    frame_wheel           = next_frame_wheel;

    clear_enum_array(next_frame_pressed,         false);
    clear_enum_array(next_frame_released,        false);
    clear_enum_array(next_frame_repeated,        false);
    clear_enum_array(next_frame_button_pressed,  false);
    clear_enum_array(next_frame_button_released, false);
    next_frame_motion = {};
    next_frame_wheel  = {};
}

auto poll_event() -> Poll_Result {
    using namespace hidden;

    auto guard = lock.scoped_irq_lock();

    if (events.empty())
        return { .event = {}, .is_present = false };

    return { .event = events.pop_front(), .is_present = true };
}

auto key_held(Key key) -> bool {
    auto guard = hidden::lock.scoped_irq_lock();
    return hidden::keys[key];
}

auto key_pressed(Key key) -> bool {
    auto guard = hidden::lock.scoped_irq_lock();
    return hidden::frame_pressed[key];
}

auto key_released(Key key) -> bool {
    auto guard = hidden::lock.scoped_irq_lock();
    return hidden::frame_released[key];
}

auto key_repeated(Key key) -> bool {
    auto guard = hidden::lock.scoped_irq_lock();
    return hidden::frame_repeated[key];
}

auto mouse_button_held(Mouse_Button button) -> bool {
    auto guard = hidden::lock.scoped_irq_lock();
    return hidden::mouse_buttons[button];
}

auto mouse_button_pressed(Mouse_Button button) -> bool {
    auto guard = hidden::lock.scoped_irq_lock();
    return hidden::frame_button_pressed[button];
}

auto mouse_button_released(Mouse_Button button) -> bool {
    auto guard = hidden::lock.scoped_irq_lock();
    return hidden::frame_button_released[button];
}

auto mouse_motion() -> Mouse_Delta {
    auto guard = hidden::lock.scoped_irq_lock();
    return hidden::frame_motion;
}

auto mouse_wheel() -> Mouse_Delta {
    auto guard = hidden::lock.scoped_irq_lock();
    return hidden::frame_wheel;
}

auto dropped_event_count() -> u64 {
    auto guard = hidden::lock.scoped_irq_lock();
    return hidden::dropped_event_count;
}

auto reset() -> void {
    using namespace hidden;

    auto guard = lock.scoped_irq_lock();
    clear_state();
}

#ifdef UNIT_TESTS_GAMEOS_INPUT

TEST(Input, repeated_key_down_is_not_pressed_again) {
    reset();
    begin_frame();

    submit_event(Event{ .type = Event_Type::KEY_DOWN, .key_down = { Key::A, false } });
    submit_event(Event{ .type = Event_Type::KEY_DOWN, .key_down = { Key::A, true } });
    begin_frame();

    EXPECT_TRUE(key_held(Key::A));
    EXPECT_TRUE(key_pressed(Key::A));
    EXPECT_TRUE(key_repeated(Key::A));
}

TEST(Input, release_sets_released_edge_and_clears_down_state) {
    reset();
    submit_event(Event{ .type = Event_Type::KEY_DOWN, .key_down = { Key::A, false } });
    begin_frame();
    submit_event(Event{ .type = Event_Type::KEY_UP, .key_up = { Key::A, false } });
    begin_frame();

    EXPECT_FALSE(key_held(Key::A));
    EXPECT_TRUE(key_released(Key::A));
}

TEST(Input, events_are_polled_in_fifo_order) {
    reset();
    submit_event(Event{ .type = Event_Type::KEY_DOWN, .key_down = { Key::A, false } });
    submit_event(Event{ .type = Event_Type::KEY_UP, .key_up = { Key::A, false } });

    auto first = poll_event();
    auto second = poll_event();

    ASSERT_TRUE(first.is_present);
    ASSERT_TRUE(second.is_present);
    EXPECT_EQ(first.event.type, Event_Type::KEY_DOWN);
    EXPECT_EQ(second.event.type, Event_Type::KEY_UP);
}

TEST(Input, mouse_motion_is_accumulated_for_frame) {
    reset();
    submit_event(Event{ .type = Event_Type::MOUSE_MOTION, .mouse_motion = { 3, -2 } });
    submit_event(Event{ .type = Event_Type::MOUSE_MOTION, .mouse_motion = { 4, 5 } });
    begin_frame();

    const auto motion = mouse_motion();
    EXPECT_EQ(motion.x, 7);
    EXPECT_EQ(motion.y, 3);
}

TEST(Input, queue_overflow_does_not_break_state) {
    reset();
    for (int index = 0; index < hidden::EVENT_QUEUE_CAPACITY; ++index)
        submit_event(Event{ .type = Event_Type::MOUSE_MOTION, .mouse_motion = { 1, 1 } });

    submit_event(Event{ .type = Event_Type::KEY_DOWN, .key_down = { Key::A, false } });

    EXPECT_TRUE(key_held(Key::A));
    EXPECT_EQ(dropped_event_count(), 1);
}

#endif

}
