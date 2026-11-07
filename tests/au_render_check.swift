// Plays the installed Audio Unit through Apple's own AVAudioEngine, with no JUCE involved.
//   swift tests/au_render_check.swift
import AVFoundation
import Foundation

func fourCC(_ s: String) -> OSType {
    s.utf8.reduce(0) { ($0 << 8) | OSType($1) }
}

let description = AudioComponentDescription(
    componentType: kAudioUnitType_MusicDevice,
    componentSubType: fourCC("Rygo"),
    componentManufacturer: fourCC("ZHRI"),
    componentFlags: 0,
    componentFlagsMask: 0)

var loaded: AVAudioUnit?
var done = false
AVAudioUnit.instantiate(with: description, options: [.loadInProcess]) { unit, error in
    if let error { print("instantiate error:", error) }
    loaded = unit
    done = true
}
while !done { RunLoop.current.run(until: Date(timeIntervalSinceNow: 0.01)) }

guard let instrument = loaded as? AVAudioUnitMIDIInstrument else {
    print("FAIL: the Audio Unit did not load as an instrument")
    exit(1)
}

print("loaded:", instrument.manufacturerName, "/", instrument.name, "version", instrument.version)

let format = AVAudioFormat(standardFormatWithSampleRate: 48000, channels: 2)!

func heardSeconds(note: UInt8, seconds: Double) throws -> Double {
    let engine = AVAudioEngine()
    engine.attach(instrument)
    engine.connect(instrument, to: engine.mainMixerNode, format: format)
    try engine.enableManualRenderingMode(.offline, format: format, maximumFrameCount: 512)
    try engine.start()

    let buffer = AVAudioPCMBuffer(pcmFormat: engine.manualRenderingFormat, frameCapacity: 512)!
    var rendered = 0
    var lastSound = -1.0
    let total = Int(seconds * 48000)

    instrument.startNote(note, withVelocity: 127, onChannel: 0)

    while rendered < total {
        let status = try engine.renderOffline(512, to: buffer)
        guard status == .success else { break }
        let samples = buffer.floatChannelData![0]
        var peak: Float = 0
        for i in 0..<Int(buffer.frameLength) { peak = max(peak, abs(samples[i])) }
        rendered += Int(buffer.frameLength)
        if peak > 0.001 { lastSound = Double(rendered) / 48000 }
    }

    instrument.stopNote(note, onChannel: 0)
    engine.stop()
    engine.detach(instrument)
    return lastSound
}

let longest = try heardSeconds(note: 0, seconds: 7.0)
let shortest = try heardSeconds(note: 120, seconds: 2.0)
print("lowest key:", longest, "s; highest key:", shortest, "s")

guard longest > 5.0, shortest > 0, shortest < 0.3 else {
    print("FAIL")
    exit(1)
}
print("ok")
