#include "TimelineComponent.h"

namespace
{
constexpr auto headerHeight = 30;
constexpr auto trackHeight = 72;
constexpr auto labelWidth = 150;
}

void TimelineComponent::setProjectModel(const ProjectModel* model)
{
    projectModel = model;
    repaint();
}

void TimelineComponent::setPosition(double seconds)
{
    positionSeconds = juce::jmax(0.0, seconds);
    repaint();
}

void TimelineComponent::setPixelsPerSecond(double value)
{
    pixelsPerSecond = juce::jlimit(20.0, 500.0, value);
    repaint();
}

void TimelineComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff252629));

    auto bounds = getLocalBounds();
    graphics.setColour(juce::Colour(0xff303238));
    graphics.fillRect(bounds.removeFromTop(headerHeight));

    const auto bpm = projectModel != nullptr ? projectModel->getBpm() : 120.0;
    const auto secondsPerBeat = 60.0 / bpm;
    const auto secondsPerBar = secondsPerBeat * 4.0;
    const auto maxSeconds = projectModel != nullptr
        ? juce::jmax(16.0, projectModel->getProjectLengthSeconds() + 4.0)
        : 16.0;

    graphics.setFont(juce::Font(juce::FontOptions(13.0f)));

    for (auto bar = 0; bar <= static_cast<int>(maxSeconds / secondsPerBar) + 1; ++bar)
    {
        const auto x = labelWidth + static_cast<int>(static_cast<double>(bar) * secondsPerBar * pixelsPerSecond);
        graphics.setColour(juce::Colour(0xff646a73));
        graphics.drawVerticalLine(x, 0.0f, static_cast<float>(getHeight()));
        graphics.setColour(juce::Colours::white);
        graphics.drawText(juce::String(bar + 1), x + 4, 4, 40, 20, juce::Justification::left);

        for (auto beat = 1; beat < 4; ++beat)
        {
            const auto beatX = x + static_cast<int>(static_cast<double>(beat) * secondsPerBeat * pixelsPerSecond);
            graphics.setColour(juce::Colour(0xff3d4148));
            graphics.drawVerticalLine(beatX, static_cast<float>(headerHeight), static_cast<float>(getHeight()));
        }
    }

    if (projectModel != nullptr)
    {
        auto y = headerHeight;

        for (const auto& track : projectModel->getAudioTracks())
        {
            graphics.setColour(juce::Colour(0xff202124));
            graphics.fillRect(0, y, getWidth(), trackHeight - 1);
            graphics.setColour(juce::Colours::lightgrey);
            graphics.drawText(track->state.name, 10, y, labelWidth - 20, 24, juce::Justification::centredLeft);

            if (track->hasAudio())
            {
                const auto isDraggedClip = draggingAudioClip && track->state.id == draggingTrackId;

                if (isDraggedClip)
                {
                    y += trackHeight;
                    continue;
                }

                const auto start = track->clips.empty() ? 0.0 : track->clips.front().startTimeSeconds;
                const auto duration = track->clips.empty()
                    ? track->getAudioDurationSeconds()
                    : track->clips.front().lengthSeconds;
                const auto x = secondsToX(start);
                const auto width = static_cast<int>(duration * pixelsPerSecond);
                graphics.setColour(juce::Colour(0xff2676a8));
                graphics.fillRoundedRectangle(x,
                                              static_cast<float>(y + 14),
                                              static_cast<float>(juce::jmax(20, width)),
                                              36.0f,
                                              4.0f);
                graphics.setColour(juce::Colours::white);
                const auto name = track->clips.empty()
                    ? juce::String("Audio Clip")
                    : track->clips.front().sourceFile.getFileName();
                graphics.drawText(name, static_cast<int>(x) + 8, y + 20, juce::jmax(20, width) - 16, 22,
                                  juce::Justification::centredLeft, true);
            }

            y += trackHeight;
        }

        for (const auto& track : projectModel->getMidiTracks())
        {
            graphics.setColour(juce::Colour(0xff202124));
            graphics.fillRect(0, y, getWidth(), trackHeight - 1);
            graphics.setColour(juce::Colours::lightgrey);
            graphics.drawText(track->state.name, 10, y, labelWidth - 20, 24, juce::Justification::centredLeft);

            for (const auto& clip : track->clips)
            {
                const auto start = projectModel->getTempoMap().beatsToSeconds(clip.startBeat);
                const auto length = projectModel->getTempoMap().beatsToSeconds(clip.lengthBeats);
                graphics.setColour(juce::Colour(0xff7b5cc7));
                graphics.fillRoundedRectangle(static_cast<float>(labelWidth) + static_cast<float>(start * pixelsPerSecond),
                                              static_cast<float>(y + 14),
                                              static_cast<float>(juce::jmax(24, static_cast<int>(length * pixelsPerSecond))),
                                              36.0f,
                                              4.0f);
                graphics.setColour(juce::Colours::white);
                graphics.drawText("MIDI Clip", labelWidth + static_cast<int>(start * pixelsPerSecond) + 8,
                                  y + 20, 100, 22, juce::Justification::left);
            }

            y += trackHeight;
        }
    }

    if (draggingAudioClip)
    {
        if (const auto previewY = getAudioTrackY(dragPreviewTrackId))
        {
            const auto x = secondsToX(dragPreviewStartSeconds);
            const auto width = juce::jmax(36.0f, static_cast<float>(dragPreviewLengthSeconds * pixelsPerSecond));

            graphics.setColour(juce::Colour(0x663da9e8));
            graphics.fillRect(0, static_cast<int>(*previewY), getWidth(), trackHeight - 1);
            graphics.setColour(juce::Colour(0xdd3da9e8));
            graphics.fillRoundedRectangle(x, *previewY + 14.0f, width, 36.0f, 4.0f);
            graphics.setColour(juce::Colour(0xffffc857));
            graphics.drawRoundedRectangle(x, *previewY + 14.0f, width, 36.0f, 4.0f, 2.0f);
            graphics.drawVerticalLine(static_cast<int>(x), 0.0f, static_cast<float>(getHeight()));
            graphics.setColour(juce::Colours::white);
            graphics.drawText(dragPreviewName,
                              static_cast<int>(x) + 8,
                              static_cast<int>(*previewY) + 20,
                              static_cast<int>(width) - 16,
                              22,
                              juce::Justification::centredLeft,
                              true);
        }
    }

    const auto playheadX = secondsToX(positionSeconds);
    graphics.setColour(juce::Colour(0xffffc857));
    graphics.drawLine(playheadX, 0.0f, playheadX, static_cast<float>(getHeight()), 2.0f);

    if (fileDragOver)
    {
        graphics.setColour(juce::Colour(0x55ffc857));
        graphics.drawVerticalLine(static_cast<int>(fileDragPosition.x), 0.0f, static_cast<float>(getHeight()));

        if (projectModel != nullptr)
        {
            if (const auto trackId = findAudioTrackAtY(fileDragPosition.y); trackId.has_value())
            {
                auto y = headerHeight;

                for (const auto& track : projectModel->getAudioTracks())
                {
                    if (track->state.id == *trackId)
                    {
                        graphics.fillRect(0, y, getWidth(), trackHeight - 1);
                        break;
                    }

                    y += trackHeight;
                }
            }
        }
    }
}

void TimelineComponent::mouseDown(const juce::MouseEvent& event)
{
    if (auto hit = findAudioClipAt(event.position))
    {
        draggingAudioClip = true;
        draggingTrackId = hit->trackId;
        dragPreviewTrackId = hit->trackId;
        dragGrabOffsetSeconds = xToSeconds(event.position.x) - hit->startTimeSeconds;
        dragPreviewStartSeconds = hit->startTimeSeconds;
        dragPreviewLengthSeconds = hit->lengthSeconds;
        dragPreviewName = hit->name;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
        repaint();
        return;
    }

    if (event.position.x < labelWidth)
        return;

    if (onSeek)
        onSeek(xToSeconds(event.position.x));
}

void TimelineComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (! draggingAudioClip)
        return;

    dragPreviewTrackId = findAudioTrackAtY(event.position.y).value_or(dragPreviewTrackId);
    auto newStart = xToSeconds(event.position.x) - dragGrabOffsetSeconds;

    if (! std::isfinite(newStart) || newStart < 0.0)
        newStart = 0.0;

    dragPreviewStartSeconds = newStart;
    repaint();
}

void TimelineComponent::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (draggingAudioClip)
    {
        if (dragPreviewTrackId == draggingTrackId)
        {
            if (onAudioClipMoved)
                onAudioClipMoved(draggingTrackId, dragPreviewStartSeconds);
        }
        else if (onAudioClipMovedToTrack)
        {
            onAudioClipMovedToTrack(draggingTrackId, dragPreviewTrackId, dragPreviewStartSeconds);
        }
    }

    draggingAudioClip = false;
    dragGrabOffsetSeconds = 0.0;
    dragPreviewStartSeconds = 0.0;
    dragPreviewLengthSeconds = 0.0;
    dragPreviewName.clear();
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

bool TimelineComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
        if (isSupportedAudioFile(juce::File(path)))
            return true;

    return false;
}

void TimelineComponent::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(files);
    fileDragOver = true;
    fileDragPosition = { static_cast<float>(x), static_cast<float>(y) };
    repaint();
}

void TimelineComponent::fileDragMove(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(files);
    fileDragPosition = { static_cast<float>(x), static_cast<float>(y) };
    repaint();
}

void TimelineComponent::fileDragExit(const juce::StringArray& files)
{
    juce::ignoreUnused(files);
    fileDragOver = false;
    repaint();
}

void TimelineComponent::filesDropped(const juce::StringArray& files, int x, int y)
{
    fileDragOver = false;
    repaint();

    if (! onAudioFileDropped)
        return;

    const auto trackId = findAudioTrackAtY(static_cast<float>(y));

    if (! trackId.has_value())
        return;

    for (const auto& path : files)
    {
        const juce::File file(path);

        if (isSupportedAudioFile(file))
        {
            onAudioFileDropped(*trackId, file, xToSeconds(static_cast<float>(x)));
            return;
        }
    }
}

std::optional<TimelineComponent::HitAudioClip> TimelineComponent::findAudioClipAt(juce::Point<float> position) const
{
    if (projectModel == nullptr)
        return std::nullopt;

    auto y = headerHeight;

    for (const auto& track : projectModel->getAudioTracks())
    {
        if (track->hasAudio() && ! track->clips.empty())
        {
            const auto& clip = track->clips.front();
            const auto clipBounds = juce::Rectangle<float>(
                secondsToX(clip.startTimeSeconds),
                static_cast<float>(y + 14),
                static_cast<float>(juce::jmax(20, static_cast<int>(clip.lengthSeconds * pixelsPerSecond))),
                36.0f);
            const auto hitBounds = clipBounds.expanded(4.0f, 8.0f);

            if (hitBounds.contains(position))
                return HitAudioClip {
                    track->state.id,
                    hitBounds,
                    clip.startTimeSeconds,
                    clip.lengthSeconds,
                    clip.sourceFile.getFileName()
                };
        }

        y += trackHeight;
    }

    return std::nullopt;
}

std::optional<float> TimelineComponent::getAudioTrackY(const TrackId& trackId) const
{
    if (projectModel == nullptr)
        return std::nullopt;

    auto y = static_cast<float>(headerHeight);

    for (const auto& track : projectModel->getAudioTracks())
    {
        if (track->state.id == trackId)
            return y;

        y += static_cast<float>(trackHeight);
    }

    return std::nullopt;
}

std::optional<TrackId> TimelineComponent::findAudioTrackAtY(float yPosition) const
{
    if (projectModel == nullptr)
        return std::nullopt;

    auto y = static_cast<float>(headerHeight);

    for (const auto& track : projectModel->getAudioTracks())
    {
        if (yPosition >= y && yPosition < y + static_cast<float>(trackHeight))
            return track->state.id;

        y += static_cast<float>(trackHeight);
    }

    return std::nullopt;
}

bool TimelineComponent::isSupportedAudioFile(const juce::File& file)
{
    return file.existsAsFile()
        && file.hasFileExtension(".wav;.aiff;.aif;.mp3");
}

float TimelineComponent::secondsToX(double seconds) const
{
    return static_cast<float>(labelWidth) + static_cast<float>(seconds * pixelsPerSecond);
}

double TimelineComponent::xToSeconds(float x) const
{
    return juce::jmax(0.0, static_cast<double>(x - static_cast<float>(labelWidth)) / pixelsPerSecond);
}
