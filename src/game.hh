#pragma once

#include <algorithm>

#include "kstd/int.hh"
#include "kstd/time.hh"
#include "kstd/gfx.hh"
#include "kstd/ps2.hh"

constexpr f32 PLAYER_SPEED_PX_PER_SEC = 300.0f;
constexpr f64 TARGET_FRAME_TIME_TICKS = static_cast<f64>(time::TICK_RATE) / 60.0;

struct Player {
    f32 x;
    f32 y;
    Resource_View sprite;
};

struct Game {
    Player player;
};

auto update_player(Player& player, f64 dt) -> void {
    f32 dx = 0.0f;
    f32 dy = 0.0f;
    if (ps2::is_pressed(ps2::Scancode::W)) dy -= 1.0f;
    if (ps2::is_pressed(ps2::Scancode::S)) dy += 1.0f;
    if (ps2::is_pressed(ps2::Scancode::A)) dx -= 1.0f;
    if (ps2::is_pressed(ps2::Scancode::D)) dx += 1.0f;

    const auto max_x = static_cast<f32>(gfx::width()  - player.sprite.width);
    const auto max_y = static_cast<f32>(gfx::height() - player.sprite.height);

    player.x = std::clamp(static_cast<f32>(player.x + dx * PLAYER_SPEED_PX_PER_SEC * dt), f32{0}, max_x);
    player.y = std::clamp(static_cast<f32>(player.y + dy * PLAYER_SPEED_PX_PER_SEC * dt), f32{0}, max_y);
}

auto draw_player(const Player& player) -> void {
    gfx::draw_sprite(player.sprite, player.x, player.y);
}

auto update(Game& game, f64 dt) -> void {
    update_player(game.player, dt);
}

auto draw(const Game& game) -> void {
    gfx::clear(gfx::BLACK);

    {
        gfx::draw_rect(250, 250, 100, 100, gfx::BLUE);
        gfx::draw_rect(300, 300, 80,  80,  gfx::Color{0, 0, 200, 128});

        gfx::draw_rect(400, 250, 100, 100, gfx::GREEN);
        gfx::draw_rect(450, 300, 80,  80,  gfx::Color{0, 200, 0, 128});

        gfx::draw_rect(550, 250, 100, 100, gfx::RED);
        gfx::draw_rect(600, 300, 80,  80,  gfx::Color{200, 0, 0, 128});

        gfx::draw_rect(700, 250, 100, 100, gfx::WHITE);
        gfx::draw_rect(750, 300, 80,  80,  gfx::Color{230, 230, 230, 128});

        gfx::draw_rect(250, 400, 550, 300, gfx::WHITE);
        gfx::draw_circle(525, 550, 100, gfx::RED);
    }

    draw_player(game.player);
    gfx::draw_frame();
}

auto game_main() -> void {
    Player player{
        .x      = static_cast<f32>(gfx::width())  / 2,
        .y      = static_cast<f32>(gfx::height()) / 2,
        .sprite = @embed("cool.png"),
    };
    Game game{player};

    auto last_frame_ticks     = static_cast<f64>(time::get_ticks());
    auto frame_deadline_ticks = last_frame_ticks;

    while (!ps2::is_pressed(ps2::Scancode::ESCAPE)) {
        const auto frame_start_ticks = static_cast<f64>(time::get_ticks());
        const auto dt = (frame_start_ticks - last_frame_ticks) / static_cast<f64>(time::TICK_RATE);
        last_frame_ticks = frame_start_ticks;

        update(game, dt);
        draw(game);

        frame_deadline_ticks += TARGET_FRAME_TIME_TICKS;
        const auto now_ticks = static_cast<f64>(time::get_ticks());
        if (now_ticks < frame_deadline_ticks) {
            time::sleep_ms(static_cast<u64>(frame_deadline_ticks - now_ticks));
        } else {
            frame_deadline_ticks = now_ticks; // fell behind schedule
        }
    }
}
