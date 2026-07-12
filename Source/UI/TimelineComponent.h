#pragma once

#include "../Core/ProjectModel.h"

#include <JuceHeader.h>

#include <functional>
#include <optional>

class TimelineComponent final : public juce::Component,
                                public juce::FileDragAndDropTarget
{
public:
    void setProjectModel(const ProjectModel* model);
    void setPosition(double seconds);
    void setPixelsPerSecond(double value);
    void paint(juce::Graphics& graphics) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragMove(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    std::function<void(double)> onSeek;
    std::function<void(const TrackId&, double)> onAudioClipMoved;
    std::function<void(const TrackId&, const TrackId&, double)> onAudioClipMovedToTrack;
    std::function<void(const TrackId&, const juce::File&, double)> onAudioFileDropped;

private:
    struct HitAudioClip
    {
        TrackId trackId;
        juce::Rectangle<float> bounds;
        double startTimeSeconds = 0.0;
        double lengthSeconds = 0.0;
        juce::String name;
    };

    std::optional<HitAudioClip> findAudioClipAt(juce::Point<float> position) const;
    std::optional<TrackId> findAudioTrackAtY(float yPosition) const;
    std::optional<float> getAudioTrackY(const TrackId& trackId) const;
    static bool isSupportedAudioFile(const juce::File& file);
    float secondsToX(double seconds) const;
    double xToSeconds(float x) const;

    const ProjectModel* projectModel = nullptr;
    double positionSeconds = 0.0;
    double pixelsPerSecond = 100.0;
    bool draggingAudioClip = false;
    TrackId draggingTrackId;
    TrackId dragPreviewTrackId;
    double dragGrabOffsetSeconds = 0.0;
    double dragPreviewStartSeconds = 0.0;
    double dragPreviewLengthSeconds = 0.0;
    juce::String dragPreviewName;
    bool fileDragOver = false;
    juce::Point<float> fileDragPosition;
};
