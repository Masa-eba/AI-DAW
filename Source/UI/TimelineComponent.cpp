#include "TimelineComponent.h"

#include <cmath>
#include <vector>

namespace
{
constexpr auto headerHeight = 30;
constexpr auto trackHeight = 72;
constexpr auto labelWidth = 150;
constexpr auto trimHandleWidth = 8.0f;
constexpr auto minimumAudioClipLengthSeconds = 0.05;
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

void TimelineComponent::setSnapEnabled(bool enabled)
{
    snapEnabled = enabled;
    repaint();
}

void TimelineComponent::setLoopRange(double startSeconds, double endSeconds)
{
    if (! std::isfinite(startSeconds) || ! std::isfinite(endSeconds) || endSeconds <= startSeconds)
    {
        clearLoopRange();
        return;
    }

    hasLoopRange = true;
    loopStartSeconds = juce::jmax(0.0, startSeconds);
    loopEndSeconds = juce::jmax(loopStartSeconds, endSeconds);
    repaint();
}

void TimelineComponent::clearLoopRange()
{
    hasLoopRange = false;
    loopStartSeconds = 0.0;
    loopEndSeconds = 0.0;
    repaint();
}

void TimelineComponent::setSnapGridBeats(double beats)
{
    snapGridBeats = juce::jlimit(0.125, 1.0, beats);
    repaint();
}

double TimelineComponent::getSnapGridBeats() const
{
    return snapGridBeats;
}

bool TimelineComponent::isSnapEnabled() const
{
    return snapEnabled;
}

std::optional<std::pair<TrackId, juce::Uuid>> TimelineComponent::getSelectedAudioClip() const
{
    return selectedAudioClip;
}

std::optional<std::pair<TrackId, juce::Uuid>> TimelineComponent::getSelectedMidiClip() const
{
    return selectedMidiClip;
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
        graphics.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));

        for (const auto& marker : projectModel->getMarkers())
        {
            const auto x = secondsToX(marker.timeSeconds);
            juce::Path markerPath;
            markerPath.addTriangle(x - 5.0f, 0.0f, x + 5.0f, 0.0f, x, static_cast<float>(headerHeight));
            graphics.setColour(juce::Colour(0xfff28c28));
            graphics.fillPath(markerPath);
            graphics.drawVerticalLine(static_cast<int>(x),
                                      static_cast<float>(headerHeight),
                                      static_cast<float>(getHeight()));
            graphics.drawText(marker.name,
                              static_cast<int>(x) + 6,
                              5,
                              90,
                              18,
                              juce::Justification::left,
                              true);
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
                const auto isDraggedClip = (draggingAudioClip || trimmingAudioClip)
                                        && track->state.id == draggingTrackId;

                if (isDraggedClip)
                {
                    y += trackHeight;
                    continue;
                }

                for (const auto& clip : track->clips)
                {
                    const auto isSelected = selectedAudioClip.has_value()
                                         && selectedAudioClip->first == track->state.id
                                         && selectedAudioClip->second == clip.id;
                    const auto x = secondsToX(clip.startTimeSeconds);
                    const auto width = static_cast<int>(clip.lengthSeconds * pixelsPerSecond);
                    graphics.setColour(clip.muted
                                            ? juce::Colour(0xff51565c)
                                            : (isSelected ? juce::Colour(0xff329f68) : juce::Colour(0xff2676a8)));
                    graphics.fillRoundedRectangle(x,
                                                  static_cast<float>(y + 14),
                                                  static_cast<float>(juce::jmax(20, width)),
                                                  36.0f,
                                                  4.0f);
                    drawAudioClipWaveform(graphics,
                                          *track,
                                          clip,
                                          juce::Rectangle<float>(x,
                                                                 static_cast<float>(y + 14),
                                                                 static_cast<float>(juce::jmax(20, width)),
                                                                 36.0f));

                    if (clip.fadeInSeconds > 0.0)
                    {
                        juce::Path fadePath;
                        const auto fadeWidth = juce::jlimit(4.0f,
                                                            static_cast<float>(juce::jmax(20, width)),
                                                            static_cast<float>(clip.fadeInSeconds * pixelsPerSecond));
                        fadePath.startNewSubPath(x, static_cast<float>(y + 50));
                        fadePath.lineTo(x + fadeWidth, static_cast<float>(y + 14));
                        fadePath.lineTo(x, static_cast<float>(y + 14));
                        fadePath.closeSubPath();
                        graphics.setColour(juce::Colour(0x6648d597));
                        graphics.fillPath(fadePath);
                    }

                    if (clip.fadeOutSeconds > 0.0)
                    {
                        juce::Path fadePath;
                        const auto clipWidth = static_cast<float>(juce::jmax(20, width));
                        const auto fadeWidth = juce::jlimit(4.0f,
                                                            clipWidth,
                                                            static_cast<float>(clip.fadeOutSeconds * pixelsPerSecond));
                        fadePath.startNewSubPath(x + clipWidth - fadeWidth, static_cast<float>(y + 14));
                        fadePath.lineTo(x + clipWidth, static_cast<float>(y + 50));
                        fadePath.lineTo(x + clipWidth, static_cast<float>(y + 14));
                        fadePath.closeSubPath();
                        graphics.setColour(juce::Colour(0x6648d597));
                        graphics.fillPath(fadePath);
                    }

                    if (isSelected)
                    {
                        graphics.setColour(juce::Colour(0xffffc857));
                        graphics.drawRoundedRectangle(x,
                                                      static_cast<float>(y + 14),
                                                      static_cast<float>(juce::jmax(20, width)),
                                                      36.0f,
                                                      4.0f,
                                                      2.0f);
                    }

                    auto clipLabel = clip.sourceFile.getFileName();

                    if (std::abs(clip.gain - 1.0f) > 0.01f)
                        clipLabel += "  x" + juce::String(clip.gain, 1);

                    if (clip.muted)
                        clipLabel += "  Muted";

                    graphics.setColour(juce::Colours::white);
                    graphics.drawText(clipLabel,
                                      static_cast<int>(x) + 8,
                                      y + 20,
                                      juce::jmax(20, width) - 16,
                                      22,
                                      juce::Justification::centredLeft,
                                      true);
                }
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
                const auto isSelected = selectedMidiClip.has_value()
                                     && selectedMidiClip->first == track->state.id
                                     && selectedMidiClip->second == clip.id;
                graphics.setColour(clip.muted
                                        ? juce::Colour(0xff51565c)
                                        : (isSelected ? juce::Colour(0xff9b78f0) : juce::Colour(0xff7b5cc7)));
                const auto clipBounds = juce::Rectangle<float>(
                    static_cast<float>(labelWidth) + static_cast<float>(start * pixelsPerSecond),
                    static_cast<float>(y + 14),
                    static_cast<float>(juce::jmax(24, static_cast<int>(length * pixelsPerSecond))),
                    36.0f);
                graphics.fillRoundedRectangle(clipBounds, 4.0f);
                drawMidiClipNotes(graphics, clip, clipBounds);

                if (isSelected)
                {
                    graphics.setColour(juce::Colour(0xffffc857));
                    graphics.drawRoundedRectangle(clipBounds,
                                                  4.0f,
                                                  2.0f);
                }
                graphics.setColour(juce::Colours::white);
                graphics.drawText(clip.muted ? "MIDI Clip  Muted" : "MIDI Clip",
                                  labelWidth + static_cast<int>(start * pixelsPerSecond) + 8,
                                  y + 20, 100, 22, juce::Justification::left);
            }

            y += trackHeight;
        }
    }

    if (hasLoopRange)
    {
        const auto loopX = secondsToX(loopStartSeconds);
        const auto loopWidth = static_cast<float>((loopEndSeconds - loopStartSeconds) * pixelsPerSecond);
        const auto loopArea = juce::Rectangle<float>(
            loopX,
            static_cast<float>(headerHeight),
            juce::jmax(1.0f, loopWidth),
            static_cast<float>(getHeight() - headerHeight));

        graphics.setColour(juce::Colour(0x22ffc857));
        graphics.fillRect(loopArea);
        graphics.setColour(juce::Colour(0xaaffc857));
        graphics.drawLine(loopArea.getX(), 0.0f, loopArea.getX(), static_cast<float>(getHeight()), 1.5f);
        graphics.drawLine(loopArea.getRight(), 0.0f, loopArea.getRight(), static_cast<float>(getHeight()), 1.5f);
        graphics.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        graphics.drawText("Loop",
                          static_cast<int>(loopArea.getX()) + 6,
                          5,
                          60,
                          18,
                          juce::Justification::left,
                          true);
    }

    if (draggingAudioClip || trimmingAudioClip)
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

    if (draggingMidiClip || trimmingMidiClip)
    {
        if (const auto previewY = getMidiTrackY(dragPreviewMidiTrackId))
        {
            const auto startSeconds = projectModel != nullptr
                ? projectModel->getTempoMap().beatsToSeconds(dragPreviewStartBeats)
                : 0.0;
            const auto lengthSeconds = projectModel != nullptr
                ? projectModel->getTempoMap().beatsToSeconds(dragPreviewLengthBeats)
                : 0.0;
            const auto x = secondsToX(startSeconds);
            const auto width = juce::jmax(36.0f, static_cast<float>(lengthSeconds * pixelsPerSecond));

            graphics.setColour(juce::Colour(0x667b5cc7));
            graphics.fillRect(0, static_cast<int>(*previewY), getWidth(), trackHeight - 1);
            graphics.setColour(juce::Colour(0xdd9b78f0));
            graphics.fillRoundedRectangle(x, *previewY + 14.0f, width, 36.0f, 4.0f);
            graphics.setColour(juce::Colour(0xffffc857));
            graphics.drawRoundedRectangle(x, *previewY + 14.0f, width, 36.0f, 4.0f, 2.0f);
            graphics.drawVerticalLine(static_cast<int>(x), 0.0f, static_cast<float>(getHeight()));
            graphics.setColour(juce::Colours::white);
            graphics.drawText(dragPreviewMidiName,
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
        const auto leftDistance = std::abs(event.position.x - hit->bounds.getX());
        const auto rightDistance = std::abs(event.position.x - hit->bounds.getRight());
        trimmingAudioClip = leftDistance <= trimHandleWidth || rightDistance <= trimHandleWidth;
        trimmingAudioClipLeftEdge = leftDistance <= rightDistance;
        draggingAudioClip = ! trimmingAudioClip;
        draggingTrackId = hit->trackId;
        draggingClipId = hit->clipId;
        dragPreviewTrackId = hit->trackId;
        dragGrabOffsetSeconds = xToSeconds(event.position.x) - hit->startTimeSeconds;
        trimOriginalStartSeconds = hit->startTimeSeconds;
        trimOriginalSourceOffsetSeconds = hit->sourceOffsetSeconds;
        trimOriginalLengthSeconds = hit->lengthSeconds;
        trimSourceDurationSeconds = hit->sourceDurationSeconds;
        dragPreviewStartSeconds = hit->startTimeSeconds;
        dragPreviewSourceOffsetSeconds = hit->sourceOffsetSeconds;
        dragPreviewLengthSeconds = hit->lengthSeconds;
        dragPreviewName = hit->name;
        selectedAudioClip = std::make_pair(hit->trackId, hit->clipId);
        selectedMidiClip.reset();
        setMouseCursor(trimmingAudioClip ? juce::MouseCursor::LeftRightResizeCursor
                                         : juce::MouseCursor::DraggingHandCursor);
        repaint();
        return;
    }

    if (auto hit = findMidiClipAt(event.position))
    {
        const auto rightDistance = std::abs(event.position.x - hit->bounds.getRight());
        trimmingMidiClip = rightDistance <= trimHandleWidth;
        draggingMidiClip = ! trimmingMidiClip;
        draggingMidiTrackId = hit->trackId;
        draggingMidiClipId = hit->clipId;
        dragPreviewMidiTrackId = hit->trackId;
        dragGrabOffsetBeats = projectModel != nullptr
            ? projectModel->getTempoMap().secondsToBeats(xToSeconds(event.position.x)) - hit->startBeat
            : 0.0;
        trimOriginalMidiStartBeats = hit->startBeat;
        trimOriginalMidiLengthBeats = hit->lengthBeats;
        dragPreviewStartBeats = hit->startBeat;
        dragPreviewLengthBeats = hit->lengthBeats;
        dragPreviewMidiName = hit->name;
        selectedMidiClip = std::make_pair(hit->trackId, hit->clipId);
        selectedAudioClip.reset();
        setMouseCursor(trimmingMidiClip ? juce::MouseCursor::LeftRightResizeCursor
                                        : juce::MouseCursor::DraggingHandCursor);
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
    if (draggingAudioClip)
    {
        dragPreviewTrackId = findAudioTrackAtY(event.position.y).value_or(dragPreviewTrackId);
        auto newStart = xToSeconds(event.position.x) - dragGrabOffsetSeconds;

        if (! std::isfinite(newStart) || newStart < 0.0)
            newStart = 0.0;

        dragPreviewStartSeconds = snapSeconds(newStart);
        repaint();
        return;
    }

    if (trimmingAudioClip)
    {
        const auto originalEnd = trimOriginalStartSeconds + trimOriginalLengthSeconds;
        auto targetSeconds = snapSeconds(xToSeconds(event.position.x));

        if (trimmingAudioClipLeftEdge)
        {
            const auto earliestStart = juce::jmax(0.0, trimOriginalStartSeconds - trimOriginalSourceOffsetSeconds);
            targetSeconds = juce::jlimit(earliestStart, originalEnd - minimumAudioClipLengthSeconds, targetSeconds);
            dragPreviewStartSeconds = targetSeconds;
            dragPreviewSourceOffsetSeconds = trimOriginalSourceOffsetSeconds
                                           + (targetSeconds - trimOriginalStartSeconds);
            dragPreviewLengthSeconds = originalEnd - targetSeconds;
        }
        else
        {
            const auto latestEnd = trimOriginalStartSeconds
                                 + juce::jmax(0.0, trimSourceDurationSeconds - trimOriginalSourceOffsetSeconds);
            targetSeconds = juce::jlimit(trimOriginalStartSeconds + minimumAudioClipLengthSeconds,
                                         latestEnd,
                                         targetSeconds);
            dragPreviewLengthSeconds = targetSeconds - trimOriginalStartSeconds;
        }

        repaint();
        return;
    }

    if (trimmingMidiClip && projectModel != nullptr)
    {
        const auto targetSeconds = snapSeconds(xToSeconds(event.position.x));
        const auto targetBeat = projectModel->getTempoMap().secondsToBeats(targetSeconds);
        dragPreviewLengthBeats = juce::jmax(0.25, targetBeat - trimOriginalMidiStartBeats);
        repaint();
        return;
    }

    if (draggingMidiClip && projectModel != nullptr)
    {
        dragPreviewMidiTrackId = findMidiTrackAtY(event.position.y).value_or(dragPreviewMidiTrackId);
        auto newStartSeconds = xToSeconds(event.position.x);
        auto newStartBeats = projectModel->getTempoMap().secondsToBeats(newStartSeconds) - dragGrabOffsetBeats;

        if (! std::isfinite(newStartBeats) || newStartBeats < 0.0)
            newStartBeats = 0.0;

        const auto beatSeconds = 60.0 / projectModel->getBpm();
        const auto snappedSeconds = snapSeconds(projectModel->getTempoMap().beatsToSeconds(newStartBeats));
        dragPreviewStartBeats = beatSeconds > 0.0
            ? projectModel->getTempoMap().secondsToBeats(snappedSeconds)
            : newStartBeats;
        repaint();
    }
}

void TimelineComponent::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (draggingAudioClip)
    {
        if (dragPreviewTrackId == draggingTrackId)
        {
            if (onAudioClipMoved)
                onAudioClipMoved(draggingTrackId, draggingClipId, dragPreviewStartSeconds);
        }
        else if (onAudioClipMovedToTrack)
        {
            onAudioClipMovedToTrack(draggingTrackId, dragPreviewTrackId, draggingClipId, dragPreviewStartSeconds);
        }

        selectedAudioClip = std::make_pair(dragPreviewTrackId, draggingClipId);
    }

    if (trimmingAudioClip)
    {
        if (onAudioClipTrimmed)
            onAudioClipTrimmed(draggingTrackId,
                               draggingClipId,
                               dragPreviewStartSeconds,
                               dragPreviewSourceOffsetSeconds,
                               dragPreviewLengthSeconds);

        selectedAudioClip = std::make_pair(draggingTrackId, draggingClipId);
    }

    if (draggingMidiClip)
    {
        if (dragPreviewMidiTrackId == draggingMidiTrackId)
        {
            if (onMidiClipMoved)
                onMidiClipMoved(draggingMidiTrackId, draggingMidiClipId, dragPreviewStartBeats);
        }
        else if (onMidiClipMovedToTrack)
        {
            onMidiClipMovedToTrack(draggingMidiTrackId,
                                   dragPreviewMidiTrackId,
                                   draggingMidiClipId,
                                   dragPreviewStartBeats);
        }

        selectedMidiClip = std::make_pair(dragPreviewMidiTrackId, draggingMidiClipId);
    }

    if (trimmingMidiClip)
    {
        if (onMidiClipTrimmed)
            onMidiClipTrimmed(draggingMidiTrackId, draggingMidiClipId, dragPreviewLengthBeats);

        selectedMidiClip = std::make_pair(draggingMidiTrackId, draggingMidiClipId);
    }

    draggingAudioClip = false;
    trimmingAudioClip = false;
    trimmingAudioClipLeftEdge = false;
    draggingMidiClip = false;
    trimmingMidiClip = false;
    dragGrabOffsetSeconds = 0.0;
    trimOriginalStartSeconds = 0.0;
    trimOriginalSourceOffsetSeconds = 0.0;
    trimOriginalLengthSeconds = 0.0;
    trimSourceDurationSeconds = 0.0;
    dragPreviewStartSeconds = 0.0;
    dragPreviewSourceOffsetSeconds = 0.0;
    dragPreviewLengthSeconds = 0.0;
    dragPreviewName.clear();
    dragGrabOffsetBeats = 0.0;
    trimOriginalMidiStartBeats = 0.0;
    trimOriginalMidiLengthBeats = 0.0;
    dragPreviewStartBeats = 0.0;
    dragPreviewLengthBeats = 0.0;
    dragPreviewMidiName.clear();
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
            for (const auto& clip : track->clips)
            {
                const auto clipBounds = juce::Rectangle<float>(
                    secondsToX(clip.startTimeSeconds),
                    static_cast<float>(y + 14),
                    static_cast<float>(juce::jmax(20, static_cast<int>(clip.lengthSeconds * pixelsPerSecond))),
                    36.0f);
                const auto hitBounds = clipBounds.expanded(4.0f, 8.0f);

                if (hitBounds.contains(position))
                    return HitAudioClip {
                        track->state.id,
                        clip.id,
                        clipBounds,
                        clip.startTimeSeconds,
                        clip.sourceOffsetSeconds,
                        clip.lengthSeconds,
                        track->getAudioDurationSeconds(),
                        clip.sourceFile.getFileName()
                    };
            }
        }

        y += trackHeight;
    }

    return std::nullopt;
}

std::optional<TimelineComponent::HitMidiClip> TimelineComponent::findMidiClipAt(juce::Point<float> position) const
{
    if (projectModel == nullptr)
        return std::nullopt;

    auto y = headerHeight + static_cast<int>(projectModel->getAudioTracks().size()) * trackHeight;

    for (const auto& track : projectModel->getMidiTracks())
    {
        for (const auto& clip : track->clips)
        {
            const auto startSeconds = projectModel->getTempoMap().beatsToSeconds(clip.startBeat);
            const auto lengthSeconds = projectModel->getTempoMap().beatsToSeconds(clip.lengthBeats);
            const auto clipBounds = juce::Rectangle<float>(
                secondsToX(startSeconds),
                static_cast<float>(y + 14),
                static_cast<float>(juce::jmax(24, static_cast<int>(lengthSeconds * pixelsPerSecond))),
                36.0f);
            const auto hitBounds = clipBounds.expanded(4.0f, 8.0f);

            if (hitBounds.contains(position))
                return HitMidiClip {
                    track->state.id,
                    clip.id,
                    hitBounds,
                    clip.startBeat,
                    clip.lengthBeats,
                    "MIDI Clip"
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

std::optional<float> TimelineComponent::getMidiTrackY(const TrackId& trackId) const
{
    if (projectModel == nullptr)
        return std::nullopt;

    auto y = static_cast<float>(headerHeight)
           + static_cast<float>(projectModel->getAudioTracks().size() * trackHeight);

    for (const auto& track : projectModel->getMidiTracks())
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

std::optional<TrackId> TimelineComponent::findMidiTrackAtY(float yPosition) const
{
    if (projectModel == nullptr)
        return std::nullopt;

    auto y = static_cast<float>(headerHeight)
           + static_cast<float>(projectModel->getAudioTracks().size() * trackHeight);

    for (const auto& track : projectModel->getMidiTracks())
    {
        if (yPosition >= y && yPosition < y + static_cast<float>(trackHeight))
            return track->state.id;

        y += static_cast<float>(trackHeight);
    }

    return std::nullopt;
}

void TimelineComponent::drawAudioClipWaveform(juce::Graphics& graphics,
                                              const AudioTrack& track,
                                              const AudioClip& clip,
                                              juce::Rectangle<float> clipBounds) const
{
    if (! track.hasAudio() || track.sampleRate <= 0.0 || clipBounds.getWidth() <= 1.0f)
        return;

    const auto waveformBounds = clipBounds.reduced(4.0f, 5.0f);
    const auto centreY = waveformBounds.getCentreY();
    const auto maxHeight = waveformBounds.getHeight() * 0.5f;
    const auto firstPixel = static_cast<int>(std::floor(waveformBounds.getX()));
    const auto lastPixel = static_cast<int>(std::ceil(waveformBounds.getRight()));

    graphics.setColour(clip.muted ? juce::Colour(0x889ca3ad) : juce::Colour(0xddeaf6ff));

    for (auto pixel = firstPixel; pixel <= lastPixel; ++pixel)
    {
        const auto clipSecondStart = juce::jlimit(0.0,
                                                  clip.lengthSeconds,
                                                  (static_cast<double>(pixel) - clipBounds.getX()) / pixelsPerSecond);
        const auto clipSecondEnd = juce::jlimit(0.0,
                                                clip.lengthSeconds,
                                                (static_cast<double>(pixel + 1) - clipBounds.getX()) / pixelsPerSecond);
        const auto sourceSecondStart = clip.sourceOffsetSeconds + clipSecondStart;
        const auto sourceSecondEnd = clip.sourceOffsetSeconds + juce::jmax(clipSecondStart, clipSecondEnd);
        const auto startSample = juce::jlimit(0,
                                              track.audioBuffer.getNumSamples(),
                                              static_cast<int>(sourceSecondStart * track.sampleRate));
        const auto endSample = juce::jlimit(startSample,
                                            track.audioBuffer.getNumSamples(),
                                            static_cast<int>(std::ceil(sourceSecondEnd * track.sampleRate)));
        const auto numSamples = endSample - startSample;

        if (numSamples <= 0)
            continue;

        auto peak = 0.0f;

        for (auto channel = 0; channel < track.audioBuffer.getNumChannels(); ++channel)
            peak = juce::jmax(peak,
                              track.audioBuffer.getMagnitude(channel, startSample, numSamples));

        const auto gain = juce::jlimit(0.0f, 2.0f, clip.gain);
        const auto height = juce::jlimit(1.0f, maxHeight, peak * gain * maxHeight);
        graphics.drawVerticalLine(pixel, centreY - height, centreY + height);
    }
}

void TimelineComponent::drawMidiClipNotes(juce::Graphics& graphics,
                                          const MidiClip& clip,
                                          juce::Rectangle<float> clipBounds) const
{
    struct NotePreview
    {
        int note = 60;
        double startBeat = 0.0;
        double endBeat = 0.0;
    };

    std::vector<NotePreview> notes;
    notes.reserve(static_cast<size_t>(clip.sequence.getNumEvents()));

    for (auto i = 0; i < clip.sequence.getNumEvents(); ++i)
    {
        const auto* event = clip.sequence.getEventPointer(i);

        if (event == nullptr || ! event->message.isNoteOn())
            continue;

        const auto noteNumber = event->message.getNoteNumber();
        const auto startBeat = event->message.getTimeStamp();
        auto endBeat = clip.lengthBeats;

        for (auto next = i + 1; next < clip.sequence.getNumEvents(); ++next)
        {
            const auto* nextEvent = clip.sequence.getEventPointer(next);

            if (nextEvent == nullptr)
                continue;

            const auto& message = nextEvent->message;

            if (message.getNoteNumber() == noteNumber && message.isNoteOff())
            {
                endBeat = message.getTimeStamp();
                break;
            }
        }

        if (startBeat < clip.lengthBeats)
            notes.push_back({ noteNumber, startBeat, juce::jlimit(startBeat + 0.05, clip.lengthBeats, endBeat) });
    }

    if (notes.empty() || clip.lengthBeats <= 0.0)
        return;

    auto lowest = notes.front().note;
    auto highest = notes.front().note;

    for (const auto& note : notes)
    {
        lowest = juce::jmin(lowest, note.note);
        highest = juce::jmax(highest, note.note);
    }

    const auto noteRange = juce::jmax(1, highest - lowest);
    const auto noteArea = clipBounds.reduced(5.0f, 6.0f);
    graphics.setColour(clip.muted ? juce::Colour(0x889ca3ad) : juce::Colour(0xeef6f0ff));

    for (const auto& note : notes)
    {
        const auto x = noteArea.getX()
                     + static_cast<float>((note.startBeat / clip.lengthBeats) * noteArea.getWidth());
        const auto right = noteArea.getX()
                         + static_cast<float>((note.endBeat / clip.lengthBeats) * noteArea.getWidth());
        const auto normalizedPitch = static_cast<float>(note.note - lowest) / static_cast<float>(noteRange);
        const auto y = noteArea.getBottom() - normalizedPitch * noteArea.getHeight();
        graphics.drawLine(x, y, juce::jmax(x + 2.0f, right), y, 2.0f);
    }
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

double TimelineComponent::snapSeconds(double seconds) const
{
    if (! snapEnabled || projectModel == nullptr)
        return juce::jmax(0.0, seconds);

    const auto beatSeconds = 60.0 / projectModel->getBpm();
    const auto snapStep = beatSeconds * snapGridBeats;

    if (snapStep <= 0.0)
        return juce::jmax(0.0, seconds);

    return juce::jmax(0.0, std::round(seconds / snapStep) * snapStep);
}
