#include "core/LoopPlayer.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

using als::LoopPlayer;

namespace {

const std::vector<float> kLoop{0.1f, 0.2f, 0.3f, 0.4f};

// Plays one block of `count` samples into fresh left and right buffers.
struct Block {
    Block(LoopPlayer& player, std::size_t count) : left(count, -1.0f), right(count, -1.0f) {
        player.process(kLoop.data(), kLoop.size(), left.data(), right.data(), count);
    }
    std::vector<float> left;
    std::vector<float> right;
};

} // namespace

TEST_CASE("a stopped player leaves the output alone", "[loop_player]") {
    LoopPlayer player;
    const Block block(player, 3);
    REQUIRE(block.left == std::vector<float>(3, -1.0f));
    REQUIRE(player.position() == 0);
    REQUIRE(player.length() == 4);
}

TEST_CASE("a playing player writes the loop to both channels", "[loop_player]") {
    LoopPlayer player; // never prepared, so there's no fade
    player.play();
    const Block block(player, 6);
    REQUIRE(block.left == std::vector<float>{0.1f, 0.2f, 0.3f, 0.4f, 0.1f, 0.2f});
    REQUIRE(block.right == block.left);
    REQUIRE(player.position() == 2);
}

TEST_CASE("play after stop starts from the top", "[loop_player]") {
    LoopPlayer player;
    player.play();
    const Block first(player, 3);
    player.stop();
    player.play(); // inside the same block, so the player never saw it stopped
    const Block second(player, 2);
    REQUIRE(second.left == std::vector<float>{0.1f, 0.2f});
}

TEST_CASE("a player without a loop stays silent", "[loop_player]") {
    LoopPlayer player;
    player.play();
    std::vector<float> left(3, -1.0f);
    player.process(nullptr, 0, left.data(), nullptr, left.size());
    REQUIRE(left == std::vector<float>(3, -1.0f));
    REQUIRE(player.length() == 0);
}
