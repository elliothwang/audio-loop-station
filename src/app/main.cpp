#include "MainComponent.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Manage app's startup and shutdown
class AudioLoopStationApplication : public juce::JUCEApplication {
  public:
    const juce::String getApplicationName() override {
        return "Audio Loop Station";
    }

    const juce::String getApplicationVersion() override {
        return "0.1.0";
    }

    // Make main window
    void initialise(const juce::String&) override {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override {
        // Delete windown on shutdown
        mainWindow.reset();
    }

    void systemRequestedQuit() override {
        quit();
    }

  private:
    class MainWindow : public juce::DocumentWindow {
      public:
        explicit MainWindow(const juce::String& name)
            : DocumentWindow(name, juce::Colours::darkgrey, juce::DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true); // Put UI inside window.
            setResizable(true, false);
            // Narrower than this squeezes the waveforms to nothing. The content's own height is
            // the minimum, so the four lanes always fit.
            constexpr int kMinimumWidth = 980; // [px]
            constexpr int kMaximumSize = 8192; // [px]
            setResizeLimits(kMinimumWidth, getHeight(), kMaximumSize, kMaximumSize);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        // Application quits when user closes window.
        void closeButtonPressed() override {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(AudioLoopStationApplication)