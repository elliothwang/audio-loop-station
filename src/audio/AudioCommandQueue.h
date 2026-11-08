#pragma once

#include "core/LoopController.h"

#include <array>
#include <cstddef>
#include <juce_core/juce_core.h>

namespace als {

// One action. Command and track w/ defaults. Track only used for delete action.
struct AudioCommand {
    Command command = Command::Stop;
    std::size_t track = 0;
};

// UI thread is command creator, Audio thread is command consumer.
class AudioCommandQueue {
  public:
    // UI thread command added to queue. Will return False if queue full.
    [[nodiscard]] bool push(const AudioCommand& command) noexcept {
        const auto slot = fifo.write(1); // Ask FIFO for space to write 1 command

        if (slot.blockSize1 == 0) {
            return false;
        }

        commands[static_cast<std::size_t>(slot.startIndex1)] = command; // Copy command into array
        return true;
    }

    // Audio thread calls to retrieve a command
    [[nodiscard]] bool pop(AudioCommand& command) noexcept {
        const auto slot = fifo.read(1); //   Ask FIFO for single pending command

        if (slot.blockSize1 == 0) {
            return false;
        }

        command = commands[static_cast<std::size_t>(
            slot.startIndex1)]; // Copy command into call's command variable
        return true;
    }

  private:
    // Storage size for pending commands
    static constexpr int storageSize = 65;

    std::array<AudioCommand, storageSize> commands{}; // 65 command slots in queue
    juce::AbstractFifo fifo{storageSize}; // Create object to track queue's read and write positions
};

} // namespace als