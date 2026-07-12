#include "WaveformComponent.h"

#include <cmath>

WaveformComponent::WaveformComponent()
    : thumbnail(512, formatManager, thumbnailCache)
{
    formatManager.registerBasicFormats();
    thumbnail.addChangeListener(this);
}

WaveformComponent::~WaveformComponent()
{
    thumbnail.removeChangeListener(this);
}

void WaveformComponent::setFile(const juce::File& file)
{
    thumbnail.clear();
    currentPositionSeconds = 0.0;
    thumbnail.setSource(new juce::FileInputSource(file));
    repaint();
}

void WaveformComponent::clear()
{
    thumbnail.clear();
    audioLengthSeconds = 0.0;
    currentPositionSeconds = 0.0;
    repaint();
}

void WaveformComponent::setCurrentPosition(double seconds)
{
    if (! std::isfinite(seconds) || seconds < 0.0)
        seconds = 0.0;

    if (audioLengthSeconds > 0.0)
        seconds = juce::jlimit(0.0, audioLengthSeconds, seconds);

    if (std::abs(currentPositionSeconds - seconds) > 0.001)
    {
        currentPositionSeconds = seconds;
        repaint();
    }
}

void WaveformComponent::setAudioLength(double seconds)
{
    if (! std::isfinite(seconds) || seconds < 0.0)
        seconds = 0.0;

    audioLengthSeconds = seconds;
    setCurrentPosition(currentPositionSeconds);
    repaint();
}

void WaveformComponent::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto innerBounds = getLocalBounds().reduced(8);
    const auto innerBoundsFloat = innerBounds.toFloat();

    graphics.setColour(juce::Colour(0xff2a2b2e));
    graphics.fillRoundedRectangle(bounds, 8.0f);

    graphics.setColour(juce::Colour(0xff4a4d52));
    graphics.drawRoundedRectangle(bounds.reduced(0.5f), 8.0f, 1.0f);

    graphics.setColour(juce::Colour(0xff5d6168));
    graphics.drawHorizontalLine(getHeight() / 2, 8.0f, static_cast<float>(getWidth() - 8));

    if (audioLengthSeconds <= 0.0 || thumbnail.getTotalLength() <= 0.0)
    {
        graphics.setColour(juce::Colours::lightgrey);
        graphics.setFont(juce::Font(juce::FontOptions(18.0f)));
        graphics.drawText("Open an audio file",
                          getLocalBounds(),
                          juce::Justification::centred,
                          true);
        return;
    }

    graphics.setColour(juce::Colour(0xff7fc7ff));
    thumbnail.drawChannels(graphics, innerBounds, 0.0, audioLengthSeconds, 1.0f);

    const auto ratio = juce::jlimit(0.0, 1.0, currentPositionSeconds / audioLengthSeconds);
    const auto playheadX = innerBoundsFloat.getX()
                       + static_cast<float>(ratio) * innerBoundsFloat.getWidth();

    graphics.setColour(juce::Colour(0xffffc857));
    graphics.drawLine(playheadX,
                      innerBoundsFloat.getY(),
                      playheadX,
                      innerBoundsFloat.getBottom(),
                      2.0f);
}

void WaveformComponent::mouseDown(const juce::MouseEvent& event)
{
    if (audioLengthSeconds <= 0.0 || event.mods.isRightButtonDown())
        return;

    const auto bounds = getLocalBounds().toFloat().reduced(8.0f);

    if (bounds.getWidth() <= 0.0f)
        return;

    const auto ratio = (event.position.x - bounds.getX()) / bounds.getWidth();
    const auto safeRatio = juce::jlimit(0.0f, 1.0f, ratio);
    const auto targetSeconds = static_cast<double>(safeRatio) * audioLengthSeconds;

    if (onSeek)
        onSeek(targetSeconds);
}

void WaveformComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &thumbnail)
        repaint();
}
