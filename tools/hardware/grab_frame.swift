// Grab still frames from a macOS video input (e.g. the Blackmagic Intensity Pro 4K
// carrying the Nikon Z6's HDMI live view from the microscope).
//
// Build: swiftc -O tools/hardware/grab_frame.swift -o ~/.local/bin/grab_frame
// Use:   grab_frame --list
//        grab_frame [--device NAME] [--size WxH [--fps F]] [--warmup N] OUT.png
// --size/--fps must match what the camera sends over HDMI, or no frames arrive.
// NAME is a case-insensitive substring of the device name (default "Intensity").
// --warmup skips the first N frames so exposure and the HDMI signal settle (default 10).
import AVFoundation
import CoreImage
import Foundation

func devices() -> [AVCaptureDevice] {
    AVCaptureDevice.DiscoverySession(
        deviceTypes: [.external, .builtInWideAngleCamera],
        mediaType: .video, position: .unspecified).devices
}

var args = Array(CommandLine.arguments.dropFirst())
if args.contains("--list") {
    for d in devices() {
        print("\(d.localizedName)  [\(d.uniqueID)]")
        for f in d.formats {
            let dim = CMVideoFormatDescriptionGetDimensions(f.formatDescription)
            let fps = f.videoSupportedFrameRateRanges.map { String(format: "%.2f", $0.maxFrameRate) }.joined(separator: ",")
            print("   \(dim.width)x\(dim.height) @ \(fps)")
        }
    }
    exit(0)
}
func take(_ flag: String) -> String? {
    guard let i = args.firstIndex(of: flag), i + 1 < args.count else { return nil }
    let v = args[i + 1]; args.removeSubrange(i...(i + 1)); return v
}
let want = (take("--device") ?? "Intensity").lowercased()
let warmup = Int(take("--warmup") ?? "10") ?? 10
let size = take("--size")          // e.g. 3840x2160; default: leave the device's format alone
let fps = Double(take("--fps") ?? "")
guard let outPath = args.first else {
    FileHandle.standardError.write("usage: grab_frame [--device NAME] [--warmup N] OUT.png | --list\n".data(using: .utf8)!)
    exit(2)
}

let sem = DispatchSemaphore(value: 0)
AVCaptureDevice.requestAccess(for: .video) { ok in
    if !ok { FileHandle.standardError.write("camera access denied (System Settings > Privacy > Camera)\n".data(using: .utf8)!); exit(3) }
    sem.signal()
}
sem.wait()

guard let dev = devices().first(where: { $0.localizedName.lowercased().contains(want) }) else {
    FileHandle.standardError.write("no video device matching \"\(want)\"; try --list\n".data(using: .utf8)!)
    exit(4)
}

final class Grabber: NSObject, AVCaptureVideoDataOutputSampleBufferDelegate {
    var seen = 0
    let warmup: Int, out: URL, done: DispatchSemaphore
    init(warmup: Int, out: URL, done: DispatchSemaphore) { self.warmup = warmup; self.out = out; self.done = done }
    func captureOutput(_ o: AVCaptureOutput, didOutput sb: CMSampleBuffer, from c: AVCaptureConnection) {
        seen += 1
        guard seen == warmup + 1, let pb = CMSampleBufferGetImageBuffer(sb) else { return }
        let img = CIImage(cvPixelBuffer: pb)
        let ctx = CIContext()
        let cs = CGColorSpace(name: CGColorSpace.sRGB)!
        do {
            try ctx.writePNGRepresentation(of: img, to: out, format: .RGBA8, colorSpace: cs)
            print("\(out.path) \(Int(img.extent.width))x\(Int(img.extent.height))")
        } catch {
            FileHandle.standardError.write("write failed: \(error)\n".data(using: .utf8)!)
        }
        done.signal()
    }
}

let session = AVCaptureSession()
do { session.addInput(try AVCaptureDeviceInput(device: dev)) } catch {
    FileHandle.standardError.write("cannot open \(dev.localizedName): \(error)\n".data(using: .utf8)!); exit(5)
}
// the capture card only delivers frames when its format matches the incoming HDMI signal
if let size = size {
    let fmts = dev.formats.filter { f in
        let d = CMVideoFormatDescriptionGetDimensions(f.formatDescription)
        return "\(d.width)x\(d.height)" == size &&
            (fps == nil || f.videoSupportedFrameRateRanges.contains { abs($0.maxFrameRate - fps!) < 0.05 })
    }
    guard let f = fmts.first, (try? dev.lockForConfiguration()) != nil else {
        FileHandle.standardError.write("no format \(size) on \(dev.localizedName); try --list\n".data(using: .utf8)!); exit(7)
    }
    dev.activeFormat = f
    if let fps = fps, let r = f.videoSupportedFrameRateRanges.first(where: { abs($0.maxFrameRate - fps) < 0.05 }) {
        dev.activeVideoMinFrameDuration = r.minFrameDuration
        dev.activeVideoMaxFrameDuration = r.minFrameDuration
    }
    dev.unlockForConfiguration()
}
let output = AVCaptureVideoDataOutput()
output.videoSettings = [kCVPixelBufferPixelFormatTypeKey as String: kCVPixelFormatType_32BGRA]
let done = DispatchSemaphore(value: 0)
let grabber = Grabber(warmup: warmup, out: URL(fileURLWithPath: outPath), done: done)
output.setSampleBufferDelegate(grabber, queue: DispatchQueue(label: "grab"))
session.addOutput(output)
session.startRunning()
if done.wait(timeout: .now() + 15) == .timedOut {
    FileHandle.standardError.write("no frames from \(dev.localizedName) within 15 s (is the camera sending HDMI?)\n".data(using: .utf8)!)
    exit(6)
}
session.stopRunning()
