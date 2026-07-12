#pragma once

#include "../Core/ProjectModel.h"

#include <JuceHeader.h>

#include <functional>
#include <optional>
#include <utility>

class TimelineComponent final : public juce::Component,
                                public juce::FileDragAndDropTarget
{
public:
    void setProjectModel(const ProjectModel* model);
    void setPosition(double seconds);
    void setPixelsPerSecond(double value);
    void setSnapEnabled(bool enabled);
    void setLoopRange(double startSeconds, double endSeconds);
    void clearLoopRange();
    bool isSnapEnabled() const;
    std::optional<std::pair<TrackId, juce::Uuid>> getSelectedAudioClip() const;
    std::optional<std::pair<TrackId, juce::Uuid>> getSelectedMidiClip() const;
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
    std::function<void(const TrackId&, const juce::Uuid&, double)> onAudioClipMoved;
    std::function<void(const TrackId&, const juce::Uuid&, double, double, double)> onAudioClipTrimmed;
    std::function<void(const TrackId&, const TrackId&, const juce::Uuid&, double)> onAudioClipMovedToTrack;
    std::function<void(const TrackId&, const juce::Uuid&, double)> onMidiClipMoved;
    std::function<void(const TrackId&, const juce::Uuid&, double)> onMidiClipTrimmed;
    std::function<void(const TrackId&, const TrackId&, const juce::Uuid&, double)> onMidiClipMovedToTrack;
    std::function<void(const TrackId&, const juce::File&, double)> onAudioFileDropped;

private:
    struct HitAudioClip
    {
        TrackId trackId;
        juce::Uuid clipId;
        juce::Rectangle<float> bounds;
        double startTimeSeconds = 0.0;
        double sourceOffsetSeconds = 0.0;
        double lengthSeconds = 0.0;
        double sourceDurationSeconds = 0.0;
        juce::String name;
    };

    struct HitMidiClip
    {
        TrackId trackId;
        juce::Uuid clipId;
        juce::Rectangle<float> bounds;
        double startBeat = 0.0;
        double lengthBeats = 0.0;
        juce::String name;
    };

    std::optional<HitAudioClip> findAudioClipAt(juce::Point<float> position) const;
    std::optional<HitMidiClip> findMidiClipAt(juce::Point<float> position) const;
    std::optional<TrackId> findAudioTrackAtY(float yPosition) const;
    std::optional<TrackId> findMidiTrackAtY(float yPosition) const;
    std::optional<float> getAudioTrackY(const TrackId& trackId) const;
    std::optional<float> getMidiTrackY(const TrackId& trackId) const;
    static bool isSupportedAudioFile(const juce::File& file);
    float secondsToX(double seconds) const;
    double xToSeconds(float x) const;
    double snapSeconds(double seconds) const;

    const ProjectModel* projectModel = nullptr;
    double positionSeconds = 0.0;
    double pixelsPerSecond = 100.0;
    bool hasLoopRange = false;
    double loopStartSeconds = 0.0;
    double loopEndSeconds = 0.0;
    bool snapEnabled = true;
    bool draggingAudioClip = false;
    bool trimmingAudioClip = false;
    bool trimmingAudioClipLeftEdge = false;
    TrackId draggingTrackId;
    juce::Uuid draggingClipId;
    TrackId dragPreviewTrackId;
    double dragGrabOffsetSeconds = 0.0;
    double trimOriginalStartSeconds = 0.0;
    double trimOriginalSourceOffsetSeconds = 0.0;
    double trimOriginalLengthSeconds = 0.0;
    double trimSourceDurationSeconds = 0.0;
    double dragPreviewStartSeconds = 0.0;
    double dragPreviewSourceOffsetSeconds = 0.0;
    double dragPreviewLengthSeconds = 0.0;
    juce::String dragPreviewName;
    bool draggingMidiClip = false;
    bool trimmingMidiClip = false;
    TrackId draggingMidiTrackId;
    juce::Uuid draggingMidiClipId;
    TrackId dragPreviewMidiTrackId;
    double dragGrabOffsetBeats = 0.0;
    double trimOriginalMidiStartBeats = 0.0;
    double trimOriginalMidiLengthBeats = 0.0;
    double dragPreviewStartBeats = 0.0;
    double dragPreviewLengthBeats = 0.0;
    juce::String dragPreviewMidiName;
    bool fileDragOver = false;
    juce::Point<float> fileDragPosition;
    std::optional<std::pair<TrackId, juce::Uuid>> selectedAudioClip;
    std::optional<std::pair<TrackId, juce::Uuid>> selectedMidiClip;
};
