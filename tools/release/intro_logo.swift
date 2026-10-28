// Builds the intro logo scene (Intro/En_open_loading_co.tga, see Intro.cfg
// scene 1) from a PNG: 1024x768, 32-bit TGA, logo centred on black.
// Usage: swift tools/release/intro_logo.swift LOGO.png OUT.tga [LOGO_WIDTH]
import CoreGraphics
import Foundation
import ImageIO

let args = CommandLine.arguments
guard args.count >= 3 else {
    FileHandle.standardError.write("usage: intro_logo.swift LOGO.png OUT.tga [LOGO_WIDTH]\n".data(using: .utf8)!)
    exit(2)
}
let width = 1024, height = 768
let logoWidth = args.count > 3 ? Int(args[3]) ?? 420 : 420
guard let source = CGImageSourceCreateWithURL(URL(fileURLWithPath: args[1]) as CFURL, nil),
      let logo = CGImageSourceCreateImageAtIndex(source, 0, nil) else { print("cannot read \(args[1])"); exit(1) }
let logoHeight = Int((Double(logo.height) * Double(logoWidth) / Double(logo.width)).rounded())

var pixels = [UInt8](repeating: 0, count: width * height * 4)
let space = CGColorSpaceCreateDeviceRGB()
let info = CGImageAlphaInfo.premultipliedLast.rawValue
pixels.withUnsafeMutableBytes { raw in
    let context = CGContext(data: raw.baseAddress, width: width, height: height, bitsPerComponent: 8,
                            bytesPerRow: width * 4, space: space, bitmapInfo: info)!
    context.setFillColor(CGColor(red: 0, green: 0, blue: 0, alpha: 1))
    context.fill(CGRect(x: 0, y: 0, width: width, height: height))
    context.interpolationQuality = .high
    context.draw(logo, in: CGRect(x: (width - logoWidth) / 2, y: (height - logoHeight) / 2,
                                  width: logoWidth, height: logoHeight))
}

// TGA: uncompressed true colour, 32 bpp, 8 alpha bits, bottom-left origin
// (the same header as the original file). CGContext rows start at the top.
var out = Data([0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                UInt8(width & 255), UInt8(width >> 8), UInt8(height & 255), UInt8(height >> 8), 32, 8])
for row in (0..<height).reversed() {
    for x in 0..<width {
        let i = (row * width + x) * 4
        out.append(contentsOf: [pixels[i + 2], pixels[i + 1], pixels[i], 255])
    }
}
try out.write(to: URL(fileURLWithPath: args[2]))
print("wrote \(args[2]) (logo \(logoWidth)x\(logoHeight))")
