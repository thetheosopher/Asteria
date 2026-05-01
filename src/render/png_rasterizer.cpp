#include "png_rasterizer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace asteria::render {

namespace {

using namespace Gdiplus;

struct GdiPlusSession {
  ULONG_PTR token = 0;

  GdiPlusSession() {
    GdiplusStartupInput input;
    GdiplusStartup(&token, &input, nullptr);
  }

  ~GdiPlusSession() {
    if (token != 0) GdiplusShutdown(token);
  }
};

GdiPlusSession& session() {
  static GdiPlusSession instance;
  return instance;
}

Gdiplus::Color toGdiColor(const asteria::render::Color& c, BYTE alpha = 255) {
  return Gdiplus::Color(alpha, static_cast<BYTE>(c.r), static_cast<BYTE>(c.g), static_cast<BYTE>(c.b));
}

std::wstring utf8ToWide(const std::string& value) {
  if (value.empty()) return {};
  const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  if (size <= 0) return {};

  std::wstring out(static_cast<size_t>(size), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), size);
  out.resize(static_cast<size_t>(size - 1));
  return out;
}

bool getPngEncoderClsid(CLSID* clsid) {
  UINT num = 0;
  UINT size = 0;
  if (GetImageEncodersSize(&num, &size) != Ok || size == 0) return false;

  std::vector<BYTE> buffer(size);
  auto* codecs = reinterpret_cast<ImageCodecInfo*>(buffer.data());
  if (GetImageEncoders(num, size, codecs) != Ok) return false;

  for (UINT i = 0; i < num; ++i) {
    if (std::wcscmp(codecs[i].MimeType, L"image/png") == 0) {
      *clsid = codecs[i].Clsid;
      return true;
    }
  }
  return false;
}

template <typename T>
void collectLayers(const std::vector<T>& elements,
                   std::map<std::string, std::vector<const T*>>& byLayer) {
  for (const auto& element : elements) byLayer[element.layer].push_back(&element);
}

std::unique_ptr<Bitmap> renderSceneBitmap(const ChartScene& scene,
                                          int widthPx,
                                          int heightPx,
                                          int dpi) {
  if (widthPx <= 0 || heightPx <= 0) return nullptr;

  auto bitmap = std::make_unique<Bitmap>(widthPx, heightPx, PixelFormat32bppARGB);
  bitmap->SetResolution(static_cast<REAL>(dpi), static_cast<REAL>(dpi));

  Graphics graphics(bitmap.get());
  graphics.SetSmoothingMode(SmoothingModeAntiAlias);
  graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
  graphics.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
  graphics.Clear(toGdiColor(scene.background));

  const double sx = static_cast<double>(widthPx) / static_cast<double>(scene.width);
  const double sy = static_cast<double>(heightPx) / static_cast<double>(scene.height);

  auto scaleX = [&](double x) { return static_cast<REAL>(x * sx); };
  auto scaleY = [&](double y) { return static_cast<REAL>(y * sy); };
  auto scaleStroke = [&](double v) {
    return static_cast<REAL>(std::max(1.0, v * (sx + sy) * 0.5));
  };
  const REAL sceneWidthPx = static_cast<REAL>(scene.width * sx);

  std::map<std::string, std::vector<const CircleElement*>> circles;
  std::map<std::string, std::vector<const LineElement*>> lines;
  std::map<std::string, std::vector<const TextElement*>> texts;
  collectLayers(scene.circles, circles);
  collectLayers(scene.lines, lines);
  collectLayers(scene.texts, texts);

  std::vector<std::string> layers;
  for (const auto& kv : circles) if (!kv.first.empty()) layers.push_back(kv.first);
  for (const auto& kv : lines) if (!kv.first.empty()) layers.push_back(kv.first);
  for (const auto& kv : texts) if (!kv.first.empty()) layers.push_back(kv.first);
  std::sort(layers.begin(), layers.end());
  layers.erase(std::unique(layers.begin(), layers.end()), layers.end());
  layers.push_back("");

  auto drawLayer = [&](const std::string& layer) {
    if (auto it = circles.find(layer); it != circles.end()) {
      for (const auto* c : it->second) {
        Pen pen(toGdiColor(c->stroke), scaleStroke(c->strokeWidth));
        graphics.DrawEllipse(&pen,
                             scaleX(c->cx - c->radius),
                             scaleY(c->cy - c->radius),
                             static_cast<REAL>(c->radius * 2.0 * sx),
                             static_cast<REAL>(c->radius * 2.0 * sy));
      }
    }

    if (auto it = lines.find(layer); it != lines.end()) {
      for (const auto* l : it->second) {
        Pen pen(toGdiColor(l->stroke), scaleStroke(l->strokeWidth));
        graphics.DrawLine(&pen,
                          scaleX(l->x1), scaleY(l->y1),
                          scaleX(l->x2), scaleY(l->y2));
      }
    }

    if (auto it = texts.find(layer); it != texts.end()) {
      for (const auto* t : it->second) {
        FontFamily family(L"Segoe UI Symbol");
        Font font(&family, static_cast<REAL>(std::max(8.0, t->fontSize * (sx + sy) * 0.5)),
                  FontStyleRegular, UnitPixel);

        SolidBrush brush(toGdiColor(t->fill));
        StringFormat format;
        if (t->anchor == "middle") format.SetAlignment(StringAlignmentCenter);
        else if (t->anchor == "end") format.SetAlignment(StringAlignmentFar);
        else format.SetAlignment(StringAlignmentNear);
        format.SetLineAlignment(StringAlignmentCenter);

        const std::wstring text = utf8ToWide(t->content);
        const REAL textX = scaleX(t->x);
        const REAL textY = scaleY(t->y - t->fontSize);
        const REAL textHeight = static_cast<REAL>(std::max(18.0, t->fontSize * 2.6 * sy));

        REAL rectX = textX;
        REAL rectWidth = std::max<REAL>(1.0f, sceneWidthPx - textX);
        if (t->anchor == "middle") {
          rectX = textX - sceneWidthPx * 0.5f;
          rectWidth = sceneWidthPx;
        } else if (t->anchor == "end") {
          rectX = 0.0f;
          rectWidth = std::max<REAL>(1.0f, textX);
        }

        RectF rect(rectX, textY, rectWidth, textHeight);
        graphics.DrawString(text.c_str(), -1, &font, rect, &format, &brush);
      }
    }
  };

  for (const auto& layer : layers) drawLayer(layer);
  return bitmap;
}

}  // namespace

bool rasterizeSceneRgb(const ChartScene& scene,
                       int widthPx,
                       int heightPx,
                       int dpi,
                       RasterImage& outImage) {
  session();

  auto bitmap = renderSceneBitmap(scene, widthPx, heightPx, dpi);
  if (!bitmap) return false;

  Rect rect(0, 0, widthPx, heightPx);
  BitmapData data{};
  if (bitmap->LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data) != Ok) {
    return false;
  }

  outImage.widthPx = widthPx;
  outImage.heightPx = heightPx;
  outImage.rgb.assign(static_cast<size_t>(widthPx) * static_cast<size_t>(heightPx) * 3u, 0);

  const auto* source = static_cast<const unsigned char*>(data.Scan0);
  for (int y = 0; y < heightPx; ++y) {
    const auto* row = source + static_cast<ptrdiff_t>(data.Stride) * y;
    for (int x = 0; x < widthPx; ++x) {
      const auto* pixel = row + x * 4;
      const size_t destIndex = (static_cast<size_t>(y) * static_cast<size_t>(widthPx) + static_cast<size_t>(x)) * 3u;
      outImage.rgb[destIndex + 0] = pixel[2];
      outImage.rgb[destIndex + 1] = pixel[1];
      outImage.rgb[destIndex + 2] = pixel[0];
    }
  }

  bitmap->UnlockBits(&data);
  return true;
}

bool writePngFile(const ChartScene& scene,
                  const std::string& filePath,
                  int widthPx,
                  int heightPx,
                  int dpi) {
  session();

  auto bitmap = renderSceneBitmap(scene, widthPx, heightPx, dpi);
  if (!bitmap) return false;

  CLSID pngClsid{};
  if (!getPngEncoderClsid(&pngClsid)) return false;
  const std::wstring widePath = utf8ToWide(filePath);
  return bitmap->Save(widePath.c_str(), &pngClsid, nullptr) == Ok;
}

}  // namespace asteria::render