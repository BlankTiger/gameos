#pragma once

#include <algorithm>

#include "kstd/int.hh"
#include "kstd/time.hh"
#include "kstd/gfx.hh"
#include "kstd/ps2.hh"
#include "kstd/string.hh"

constexpr f32 PLAYER_SPEED_PX_PER_SEC = 300.0f;
constexpr u64 FPS_MAX                 = 144;

struct Player {
    f32 x;
    f32 y;
    Resource_View sprite;
};

struct Game {
    Player player;
    u64    time_ms    = 0;
    f64    dt         = 0.0;
    u64    dt_real    = 0;
    f64    time_scale = 1.0;
    f64    fps        = 0.0;
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

    const auto scale = PLAYER_SPEED_PX_PER_SEC * dt;
    player.x = std::clamp(static_cast<f32>(player.x + dx * scale), f32{0}, max_x);
    player.y = std::clamp(static_cast<f32>(player.y + dy * scale), f32{0}, max_y);
}

auto draw_player(const Player& player) -> void {
    gfx::draw_sprite(player.sprite, player.x, player.y);
}

auto update(Game& game) -> void {
    update_player(game.player, game.dt);
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

    const auto fps_text = sprint("FPS: %", game.fps);
    gfx::draw_text(8, 8, fps_text);

    gfx::draw_frame();
}

auto game_main() -> void {
    Player player{
        .x      = static_cast<f32>(gfx::width())  / 2,
        .y      = static_cast<f32>(gfx::height()) / 2,
        .sprite = @embed("cool.png"),
    };
    Game game{player};

    u64 last_tick = time::get_ticks();

    constexpr auto TARGET_TICKS = time::ticks_per_frame(FPS_MAX);

    while (!ps2::is_pressed(ps2::Scancode::ESCAPE)) {
        const u64 frame_start = time::get_ticks();
        const u64 elapsed     = frame_start - last_tick;
        last_tick = frame_start;

        game.dt_real  = elapsed;
        game.dt       = static_cast<f64>(game.dt_real) / time::TICK_RATE * game.time_scale;
        game.time_ms += time::ticks_to_ms(game.dt_real);
        game.fps      = 1 / game.dt;

        update(game);
        draw(game);

        const u64 frame_ticks = time::get_ticks() - frame_start;
        if (frame_ticks < TARGET_TICKS) {
            time::sleep_ticks(TARGET_TICKS - frame_ticks);
        }
    }
}
