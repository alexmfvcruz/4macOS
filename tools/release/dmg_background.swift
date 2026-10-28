// Draws the DMG window background (660x420 points) at 1x and 2x.
// Usage: swift dmg_background.swift ART.png LOGO.png OUT_1x.png OUT_2x.png
import AppKit

let a = CommandLine.arguments
let art = NSImage(contentsOfFile: a[1])!, logo = NSImage(contentsOfFile: a[2])!
let W: CGFloat = 660, H: CGFloat = 420

func render(scale: CGFloat, to path: String) {
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: Int(W * scale), pixelsHigh: Int(H * scale),
                               bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                               colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    rep.size = NSSize(width: W, height: H)
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    NSColor.black.setFill(); NSRect(x: 0, y: 0, width: W, height: H).fill()
    // Art: fill the window height, centred, then darken towards the bottom.
    let ah = H, aw = art.size.width * ah / art.size.height
    art.draw(in: NSRect(x: (W - aw) / 2, y: 0, width: aw, height: ah))
    NSGradient(starting: NSColor(white: 0, alpha: 0.35), ending: NSColor(white: 0, alpha: 0.80))!
        .draw(in: NSRect(x: 0, y: 0, width: W, height: H), angle: -90)
    // Logo at the top.
    let lw: CGFloat = 230, lh = logo.size.height * lw / logo.size.width
    logo.draw(in: NSRect(x: (W - lw) / 2, y: H - lh - 18, width: lw, height: lh))
    // Arrow between the icons (icon centres at x 165 and 495, y 210 from top).
    let y = H - 210
    let arrow = NSBezierPath(); arrow.lineWidth = 4; arrow.lineCapStyle = .round; arrow.lineJoinStyle = .round
    arrow.move(to: NSPoint(x: 265, y: y)); arrow.line(to: NSPoint(x: 395, y: y))
    arrow.move(to: NSPoint(x: 375, y: y + 16)); arrow.line(to: NSPoint(x: 395, y: y)); arrow.line(to: NSPoint(x: 375, y: y - 16))
    NSColor(white: 1, alpha: 0.9).setStroke(); arrow.stroke()
    // Caption.
    let style = NSMutableParagraphStyle(); style.alignment = .center
    let shadow = NSShadow(); shadow.shadowColor = .black; shadow.shadowBlurRadius = 6
    ("Drag 4Story (Beta) to Applications" as NSString).draw(in: NSRect(x: 0, y: 34, width: W, height: 30), withAttributes: [
        .font: NSFont.systemFont(ofSize: 20, weight: .medium), .foregroundColor: NSColor.white,
        .paragraphStyle: style, .shadow: shadow])
    NSGraphicsContext.restoreGraphicsState()
    try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: path))
}
render(scale: 1, to: a[3]); render(scale: 2, to: a[4])
