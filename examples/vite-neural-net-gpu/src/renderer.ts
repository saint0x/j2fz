import type { PointRecord, RenderMode } from "./state";

function decodeFrame(frameBase64: string): Uint8ClampedArray {
  const binary = atob(frameBase64);
  const bytes = new Uint8ClampedArray(binary.length);
  for (let index = 0; index < binary.length; index += 1) {
    bytes[index] = binary.charCodeAt(index);
  }
  return bytes;
}

function pointPixelX(point: PointRecord, width: number): number {
  return ((point.x + 1) * 0.5) * width;
}

function pointPixelY(point: PointRecord, height: number): number {
  return ((point.y + 1) * 0.5) * height;
}

function glyphForPixel(r: number, g: number, b: number): string {
  const brightness = Math.max(r, g, b);
  if (brightness < 48) {
    return ".";
  }
  if (brightness < 96) {
    return ":";
  }
  if (brightness < 128) {
    return "=";
  }
  if (brightness < 164) {
    return "+";
  }
  if (brightness < 212) {
    return "*";
  }
  return "@";
}

export function paintField(
  canvas: HTMLCanvasElement,
  frameBase64: string,
  width: number,
  height: number,
  points: PointRecord[],
  classColors: string[],
  mode: RenderMode,
): void {
  const context = canvas.getContext("2d");
  if (!context) {
    throw new Error("canvas 2d context unavailable");
  }
  const bytes = decodeFrame(frameBase64);
  const image = new ImageData(bytes, width, height);
  context.putImageData(image, 0, 0);

  if (mode === "ascii") {
    context.save();
    context.font = "6px monospace";
    context.textAlign = "center";
    context.textBaseline = "middle";
    for (let y = 0; y < height; y += 4) {
      for (let x = 0; x < width; x += 4) {
        const offset = ((y * width) + x) * 4;
        const glyph = glyphForPixel(bytes[offset] ?? 0, bytes[offset + 1] ?? 0, bytes[offset + 2] ?? 0);
        context.fillStyle = `rgba(${bytes[offset]}, ${bytes[offset + 1]}, ${bytes[offset + 2]}, 0.85)`;
        context.fillText(glyph, x + 2, y + 2);
      }
    }
    context.restore();
  }

  context.save();
  for (const point of points) {
    context.beginPath();
    context.fillStyle = classColors[point.classId] ?? "#ffffff";
    context.arc(pointPixelX(point, width), pointPixelY(point, height), 3.5, 0, Math.PI * 2);
    context.fill();
    context.lineWidth = 1.2;
    context.strokeStyle = "#0a0f17";
    context.stroke();
  }
  context.restore();
}
